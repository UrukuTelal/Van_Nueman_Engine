#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <cmath>

// 16 Pillar indices (must match FULL_ARCHITECTURE.md)
#define PILLAR_AWARENESS     0
#define PILLAR_WILLPOWER     1
#define PILLAR_FORCE         2
#define PILLAR_INFLUENCE     3
#define PILLAR_RESISTANCE    4
#define PILLAR_INTEGRITY     5
#define PILLAR_COHESION      6
#define PILLAR_RELATION      7
#define PILLAR_PRESENCE      8
#define PILLAR_WARMTH        9
#define PILLAR_MEMORY        10
#define PILLAR_ATTRACTION    11
#define PILLAR_HARM          12
#define PILLAR_DISTORTION    13
#define PILLAR_FLUX          14  // Energy flow rate between entities; drives dynamic state changes
#define PILLAR_DEPTH         15  // Recursive self-reference depth; enables meta-cognitive processes
#define NUM_PILLARS          16

#define MAX_ENTITIES 500000
#define MAX_SERVERS 1024
#define MAX_FEDERATIONS 64
#define TICK_RATE 30  // 30 Hz

// Scaled integer: 1 = 2^20 (from architecture spec)
#define SCALE_FACTOR 1048576  // 2^20
#define TO_SCALED(f) ((int32_t)((f) * SCALE_FACTOR))
#define FROM_SCALED(i) ((float)(i) / SCALE_FACTOR)

// Pillar State Vector (14 values per entity)
struct __align__(16) PillarStateVector {
    int32_t p[NUM_PILLARS];  // scaled integers
};

// Entity types from architecture spec
enum EntityType {
    ENTITY_CELESTIAL = 0,
    ENTITY_LIVING_PLANET = 1,
    ENTITY_DRONE = 2,
    ENTITY_ROBOT = 3,
    ENTITY_CREATURE = 4,
    ENTITY_HUMAN = 5,
    ENTITY_SERVER = 6,
    ENTITY_FEDERATION = 7
};

struct Entity {
    uint32_t id;
    uint32_t type;
    float3 position;
    float3 velocity;
    PillarStateVector pillars;
    PillarStateVector baseline;  // baseline values for reset
    uint32_t alive;
    uint32_t server_id;
    float energy;
    float health;
};

struct ServerState {
    uint32_t id;
    PillarStateVector pillars;
    uint32_t player_count;
    uint32_t entity_count;
    float tick_accumulator;
};

struct FederationState {
    uint32_t id;
    PillarStateVector pillars;
    uint32_t server_count;
};

// Interaction matrix: how pillar A affects pillar B
__device__ float interaction_matrix[NUM_PILLARS][NUM_PILLARS];

// Initialize interaction matrix
__global__ void pillars_init_kernel() {
    if (threadIdx.x == 0 && blockIdx.x == 0) {
        for (int i = 0; i < NUM_PILLARS; i++) {
            for (int j = 0; j < NUM_PILLARS; j++) {
                interaction_matrix[i][j] = (i == j) ? 1.0f : 0.1f;
            }
        }
        // Custom interactions based on architecture spec:
        interaction_matrix[PILLAR_FORCE][PILLAR_RESISTANCE] = 0.8f;
        interaction_matrix[PILLAR_INFLUENCE][PILLAR_DISTORTION] = 0.6f;
        interaction_matrix[PILLAR_WARMTH][PILLAR_HARM] = -0.5f;
        interaction_matrix[PILLAR_MEMORY][PILLAR_AWARENESS] = 0.7f;
    }
}

// Logarithmic decay: base_signal / log(hop_count + 1)
__device__ float log_decay(float signal, int hop_count) {
    return signal / logf(hop_count + 1.0f + 1e-6f);
}

// Pillar update for a single entity (30 Hz tick)
__global__ void pillars_update_kernel(Entity* entities, uint32_t count, float dt) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= count) return;
    if (!entities[idx].alive) return;

    Entity& e = entities[idx];

    // 1. Awareness: Sample world state (simplified - just decay toward baseline)
    int32_t awareness = e.pillars.p[PILLAR_AWARENESS];
    // Awareness reduces Distortion effect
    int32_t distortion = e.pillars.p[PILLAR_DISTORTION];
    awareness -= (distortion >> 10);  // distortion erodes awareness slightly
    e.pillars.p[PILLAR_AWARENESS] = awareness;

    // 2. Willpower: Persistence - drift toward baseline slowly
    for (int p = 0; p < NUM_PILLARS; p++) {
        int32_t current = e.pillars.p[p];
        int32_t base = e.baseline.p[p];
        int32_t diff = base - current;
        // Slow drift toward baseline (willpower maintains direction)
        int32_t willpower = e.pillars.p[PILLAR_WILLPOWER];
        int32_t drift = (diff >> 10) * (willpower >> 16);  // scaled adjustment
        e.pillars.p[p] = current + drift;
    }

    // 14. Flux: System volatility monitor - high flux requires more frequent perception
    // Flux represents the rate of energy/information flow through the system
    int32_t flux = e.pillars.p[PILLAR_FLUX];
    // High flux (>0.7 scaled) increases the effective update frequency
    // In a real implementation, this would affect how often Perceive step is called
    // For now, we just ensure flux influences other pillars appropriately
    if (flux > TO_SCALED(0.7f)) {
        // High flux increases sensitivity to changes
        // This could be implemented by amplifying other pillar updates
        // For demonstration, we slightly increase willpower effect under high flux
        int32_t willpower = e.pillars.p[PILLAR_WILLPOWER];
        int32_t flux_boost = (willpower >> 18) * ((flux - TO_SCALED(0.7f)) >> 10);
        // Apply flux boost to awareness (makes system more responsive)
        awareness += flux_boost;
        e.pillars.p[PILLAR_AWARENESS] = awareness;
    }

    // 15. Depth: Recursive self-reference depth - prevents shallow modifications that increase distortion
    // Depth represents the level of meta-cognitive processing and self-reflection
    int32_t depth = e.pillars.p[PILLAR_DEPTH];
    // Low depth (<0.3 scaled) indicates shallow processing which can increase distortion
    // High depth enables more thoughtful, resilient state changes
    if (depth < TO_SCALED(0.3f)) {
        // Shallow depth increases distortion susceptibility
        // Reduce the distortion filtering effect of awareness
        int32_t distortion = e.pillars.p[PILLAR_DISTORTION];
        int32_t depth_penalty = (distortion >> 12) * (TO_SCALED(0.3f) - depth) >> 10;
        distortion += depth_penalty;  // Increase distortion when depth is too low
        e.pillars.p[PILLAR_DISTORTION] = distortion;
    }

    // 3. Force application (entity moves based on Force + Attraction)
    int32_t force = e.pillars.p[PILLAR_FORCE];
    int32_t attraction = e.pillars.p[PILLAR_ATTRACTION];
    float force_float = FROM_SCALED(force) * FROM_SCALED(attraction);
    // Apply as velocity (simplified)
    float3 dir = normalize(e.velocity);  // would be calculated from attraction sources
    e.velocity = dir * force_float;
}

// Server-level pillar aggregation
__global__ void pillars_server_aggregate_kernel(Entity* entities, uint32_t entity_count,
                                                  ServerState* servers, uint32_t server_count) {
    uint32_t server_idx = blockIdx.x;
    if (server_idx >= server_count) return;

    ServerState& server = servers[server_idx];

    // Reset server pillars
    for (int p = 0; p < NUM_PILLARS; p++) {
        server.pillars.p[p] = 0;
    }

    // Aggregate from all entities on this server
    uint32_t count = 0;
    for (uint32_t i = 0; i < entity_count; i++) {
        if (entities[i].alive && entities[i].server_id == server.id) {
            for (int p = 0; p < NUM_PILLARS; p++) {
                server.pillars.p[p] += entities[i].pillars.p[p];
            }
            count++;
        }
    }

    // Average
    if (count > 0) {
        for (int p = 0; p < NUM_PILLARS; p++) {
            server.pillars.p[p] /= (int32_t)count;
        }
    }
}

// Logarithmic feedback loop between servers (1 Hz background)
__global__ void pillars_feedback_kernel(ServerState* servers, uint32_t server_count,
                                         float* distance_matrix, float dt) {
    uint32_t i = blockIdx.x;
    if (i >= server_count) return;

    for (uint32_t j = 0; j < server_count; j++) {
        if (i == j) continue;

        float distance = distance_matrix[i * server_count + j];
        if (distance <= 0) continue;

        ServerState& A = servers[i];
        ServerState& B = servers[j];

        // Raw force: A.Force * A.Attraction * (1 / log(distance + 1))
        float raw_force = FROM_SCALED(A.pillars.p[PILLAR_FORCE]) *
                          FROM_SCALED(A.pillars.p[PILLAR_ATTRACTION]) *
                          (1.0f / logf(distance + 1.0f + 1e-6f));

        // Pillar interaction resolution
        for (int p = 0; p < NUM_PILLARS; p++) {
            float net_force = raw_force * interaction_matrix[PILLAR_FORCE][p];
            float resistance = FROM_SCALED(B.pillars.p[PILLAR_RESISTANCE]) *
                              interaction_matrix[PILLAR_RESISTANCE][p];
            net_force -= resistance;

            if (fabsf(net_force) > 0.001f) {
                int32_t delta = TO_SCALED(net_force * dt);
                atomicAdd(&B.pillars.p[p], delta);
            }
        }
    }
}
