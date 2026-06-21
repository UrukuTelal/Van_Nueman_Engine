#include "pillars_shared.cuh"

__constant__ float interaction_matrix[NumPillars][NumPillars];

// Pillar update kernel (flat PillarStateVector array)
__global__ void pillars_update_kernel(
    PillarStateVector* pillar_vectors,
    const float* external_forces,
    int num_entities,
    float dt
) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_entities) return;

    for (int p = 0; p < NumPillars; ++p) {
        float force = external_forces[idx * NumPillars + p];
        float new_val = pillar_vectors[idx].p[p] + force * dt;
        if (new_val > 1.0f) new_val = 1.0f;
        else if (new_val < 0.0f) new_val = 0.0f;
        pillar_vectors[idx].p[p] = new_val;
    }
}

// Harm calculation kernel
__global__ void pillars_harm_kernel(
    PillarStateVector* pillar_vectors,
    const float* damage,
    int num_entities,
    float dt
) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_entities) return;

    float harm = pillar_vectors[idx].p[Harm];
    float resistance = pillar_vectors[idx].p[Resistance];
    float integrity = pillar_vectors[idx].p[Integrity];
    float dmg = damage[idx];
    float harm_increase = dmg - resistance - integrity;
    if (harm_increase > 0.0f) {
        harm += harm_increase * dt;
        if (harm > 1.0f) harm = 1.0f;
        pillar_vectors[idx].p[Harm] = harm;
    }
}

// Kernel 1: AoS update with full Bloch-space physics
__global__ void pillars_update_kernel_aos(
    GPUEntity* entities,
    int num_entities,
    float dt
) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_entities) return;
    if (!entities[idx].alive) return;

    const float DRIFT_RATE = 0.001f;
    const float ACTION_RATE = 0.1f;
    const float VELOCITY_DAMPING = 0.5f;

    float* p = entities[idx].pillars.p;

    // 0) Velocity damping (prevents infinite velocity accumulation)
    float3 vel = entities[idx].velocity;
    vel.x *= expf(-VELOCITY_DAMPING * dt);
    vel.y *= expf(-VELOCITY_DAMPING * dt);
    vel.z *= expf(-VELOCITY_DAMPING * dt);
    entities[idx].velocity = vel;

    // 1) Distortion twists Awareness via Bloch phase
    float distortion = p[Distortion];
    if (distortion > 0.001f) {
        p[Awareness] = bloch_apply_distortion_torsion(p[Awareness], distortion * dt);
    }

    // 2) Theta-space drift toward baseline for all pillars
    for (int i = 0; i < NumPillars; ++i) {
        float baseline = (i == Harm) ? 0.1f : 0.5f;
        float theta = bloch_value_to_theta(p[i]);
        float target_theta = bloch_value_to_theta(baseline);
        float drift = (target_theta - theta) * DRIFT_RATE * dt;
        p[i] = bloch_theta_to_value(theta + drift);
    }

    // 3) Force + Attraction drives rotation on all pillars
    float force = p[Force];
    float attraction = p[Attraction];
    float rotation_strength = force * attraction * ACTION_RATE * dt;
    if (fabsf(rotation_strength) > 1e-6f) {
        for (int i = 0; i < NumPillars; ++i) {
            float influence = interaction_matrix[Force][i];
            p[i] = bloch_rotate(p[i], rotation_strength * influence * 0.01f);
        }
    }

    // 4) Harm decays toward baseline via Integrity + Resistance
    float harm = p[Harm];
    float integrity = p[Integrity];
    float resistance = p[Resistance];
    float harm_reduction = (integrity + resistance) * 0.5f * DRIFT_RATE * dt;
    if (harm > 0.01f && harm_reduction > 0.0f) {
        p[Harm] = fmaxf(0.0f, harm - harm_reduction);
    }
}

// Kernel 2a: Entity-parallel server accumulation (shared-mem single-pass)
__global__ void pillars_server_accum_kernel(
    GPUEntity* entities,
    int num_entities,
    ServerState* servers,
    int num_servers,
    float* theta_accum,
    uint32_t* server_count
) {
    extern __shared__ char shared_mem[];
    float* s_theta = (float*)shared_mem;
    uint32_t* s_servers_in_block = (uint32_t*)&s_theta[blockDim.x * NumPillars];
    uint32_t* s_count = (uint32_t*)&s_servers_in_block[1];

    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int tid = threadIdx.x;

    bool valid = idx < num_entities && entities[idx].alive;

    if (tid < NumPillars) {
        s_theta[tid] = 0.0f;
    }
    if (tid == 0) {
        s_servers_in_block[0] = 0;
        s_count[0] = 0;
    }
    __syncthreads();

    if (valid) {
        int sid = entities[idx].server_id;
        if (sid < 0 || sid >= num_servers) {
            sid = 0;
        }

        for (int p = 0; p < NumPillars; ++p) {
            float theta = acosf(2.0f * entities[idx].pillars.p[p] - 1.0f);
            atomicAdd(&theta_accum[sid * NumPillars + p], theta);
        }
        atomicAdd(&server_count[sid], 1);
    }
}

// Kernel 2b: Normalize server accumulators to final pillar values
__global__ void pillars_server_normalize_kernel(
    ServerState* servers,
    int num_servers,
    const float* theta_accum,
    const uint32_t* server_count
) {
    int sid = blockIdx.x;
    int p = threadIdx.x;
    if (sid >= num_servers) return;
    if (p >= NumPillars) return;

    uint32_t cnt = server_count[sid];
    if (cnt == 0) {
        servers[sid].pillars.p[p] = 0.5f;
        return;
    }

    float avg_theta = theta_accum[sid * NumPillars + p] / cnt;
    servers[sid].pillars.p[p] = bloch_theta_to_value(avg_theta);
}

// Kernel 3: Logarithmic feedback loop between servers
__global__ void pillars_feedback_kernel(
    ServerState* servers,
    int num_servers,
    const float* distance_matrix,
    float dt
) {
    int i = blockIdx.x;
    if (i >= num_servers) return;

    float force_i = servers[i].pillars.p[Force];
    float attraction_i = servers[i].pillars.p[Attraction];

    for (int j = 0; j < num_servers; ++j) {
        if (i == j) continue;

        float dist = distance_matrix[i * num_servers + j];
        if (dist < 0.001f) dist = 0.001f;

        float log_dist = logf(dist);
        float influence = calculate_influence(force_i, attraction_i,
                                               servers[j].pillars.p[Resistance],
                                               servers[j].pillars.p[Willpower],
                                               servers[j].pillars.p[Depth]);
        float coupling = influence / (1.0f + fabsf(log_dist));

        for (int p = 0; p < NumPillars; ++p) {
            float net = coupling * interaction_matrix[p][p] * dt;
            if (fabsf(net) > 1e-8f) {
                servers[j].pillars.p[p] = bloch_rotate(servers[j].pillars.p[p], net);
            }
        }
    }
}


