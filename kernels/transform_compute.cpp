// Van Nueman TRANSFORM Compute Shader - UGC C++ -> SPIR-V
// Compile: ugc -o transform_compute.spv transform_compute.cpp
//
// SoA (Structure-of-Arrays) layout: separate float* arrays per field
// for cache-friendly GPU access.
// See also: kernels/pillars_compute.cu (AoS layout) — DO NOT dispatch
// SoA data to AoS entry points or vice versa.
// Push constants: dt, entity_count, mode.
//
// Part of the ECS migration: mirrors simulation::AgentECS on the GPU side.

#ifndef UGC_COMPILER
#include <cstdint>
#else
typedef signed int int32_t;
#endif

#include "vn/PillarTypes.h"

// SPIR-V compatible math — use Clang builtins that map directly to SPIR-V GLSL.std.450
namespace {
inline float sinf_(float x) { return __builtin_elementwise_sin(x); }
// Unified PillarIndex enum from pillars.yaml (via toolchain copy)
#include "vn/PillarEnumUGC.h"

inline float cosf_(float x) { return __builtin_elementwise_cos(x); }
inline float acosf_(float x) { return __builtin_elementwise_acos(x); }
inline float sqrtf_(float x) { return __builtin_elementwise_sqrt(x); }
inline float fabsf_(float x) { return __builtin_elementwise_abs(x); }
}

using namespace vn;


// Pillar indices (match Entity.h enum)

// SoA SSBO layout (mirrors AgentECS pillar arrays)
// These are bound as SSBOs in Vulkan. The vncc compiler maps
// buffer_reference annotations to SPIR-V descriptor bindings.
struct TransformSSBO {
    // Positions (float3 per entity, stride 12 bytes)
    float* __restrict positions_x;
    float* __restrict positions_y;
    float* __restrict positions_z;

    // Velocities
    float* __restrict velocities_x;
    float* __restrict velocities_y;
    float* __restrict velocities_z;

    // Pillar arrays (one per pillar)
    fp20_t* __restrict pillars[NumPillars];

    // Active mask
    uint32_t* __restrict active;

    // Resources
    int32_t* __restrict resources;
};

// Push-constant equivalen (passed as function parameters by vncc,
// mapped to SPIR-V push constants in the Vulkan pipeline).
struct TransformPushConstants {
    float dt;
    uint32_t entity_count;
    uint32_t mode;  // 0=update, 1=interaction, 2=decay
    uint32_t _pad;
};

// ── Bloch Sphere Helpers ─────────────────────────────────

inline float pillar_value_to_theta(float val) {
    if (val < 0.0f) val = 0.0f;
    if (val > 1.0f) val = 1.0f;
    return acosf_(2.0f * val - 1.0f);
}

inline float theta_to_pillar_value(float theta) {
    if (theta < 0.0f) theta = 0.0f;
    if (theta > 3.14159265f) theta = 3.14159265f;
    return (cosf_(theta) + 1.0f) * 0.5f;
}

inline fp20_t bloch_rotate(fp20_t current, float delta_theta) {
    float theta = pillar_value_to_theta(current.to_float());
    float new_theta = theta + delta_theta;
    return fp20_t(theta_to_pillar_value(new_theta));
}

// ── Core TRANSFORM: Single entity update ─────────────────

void transform_entity(
    uint32_t idx,
    TransformSSBO& ssbo,
    float dt
) {
    if (!ssbo.active[idx]) return;

    // Read current pillar values from SoA
    fp20_t p[NumPillars];
    for (int i = 0; i < NumPillars; i++) {
        p[i] = ssbo.pillars[i][idx];
    }

    float awareness_f = p[Awareness].to_float();
    float distortion_f = p[Distortion].to_float();
    float force_f = p[Force].to_float();
    float willpower_f = p[Willpower].to_float();
    float depth_f = p[Depth].to_float();
    float harm_f = p[Harm].to_float();
    float attraction_f = p[Attraction].to_float();
    float relation_f = p[Relation].to_float();

    // 1. Distortion → Awareness (phase twist, no dampening)
    if (distortion_f > 0.001f) {
        float theta = pillar_value_to_theta(awareness_f);
        float phi_twist = distortion_f * dt * 0.5f;
        float z = cosf_(theta) * cosf_(phi_twist);
        awareness_f = (z + 1.0f) * 0.5f;
    }

    // 2. Harm → Integrity Bloch rotation
    if (harm_f > 0.01f && willpower_f > 0.001f) {
        float resistance = willpower_f * depth_f;
        if (resistance < 1e-8f) resistance = 1e-8f;
        float delta_theta = (harm_f * 0.1f * dt) / resistance;
        if (delta_theta > 6.2832f) delta_theta = 6.2832f;
        if (delta_theta < -6.2832f) delta_theta = -6.2832f;
        p[Integrity] = bloch_rotate(p[Integrity], delta_theta);
    }

    // 3. Force + Attraction → pilot rotation
    if (force_f > 0.01f && attraction_f > 0.01f) {
        float action_rotation = force_f * attraction_f * dt * 0.2f;
        p[Force] = bloch_rotate(p[Force], action_rotation * 0.01f);
        p[Relation] = bloch_rotate(p[Relation], action_rotation * 0.005f);
    }

    // 4. Natural Bloch drift toward baseline
    for (int i = 0; i < NumPillars; i++) {
        float baseline = (i == Harm) ? 0.1f : 0.5f;
        float current = p[i].to_float();
        float theta = pillar_value_to_theta(current);
        float target_theta = pillar_value_to_theta(baseline);
        float diff = target_theta - theta;
        float drift = diff * 0.1f * dt;
        if (fabsf_(drift) > 1e-8f) {
            float new_theta = theta + drift;
            p[i] = fp20_t(theta_to_pillar_value(new_theta));
        }
    }

    // 5. Clamp and write back to SoA arrays
    p[Awareness] = fp20_t(awareness_f);
    for (int i = 0; i < NumPillars; i++) {
        ssbo.pillars[i][idx] = p[i];
    }

    // 6. Update velocity from Force pillar
    float speed = force_f * dt * 2.0f;
    ssbo.velocities_x[idx] += cosf_(relation_f * 6.2832f) * speed;
    ssbo.velocities_y[idx] += sinf_(relation_f * 6.2832f) * speed;
    ssbo.velocities_z[idx] += cosf_(depth_f * 6.2832f) * speed * 0.5f;

    // Apply velocity damping (prevents unbounded acceleration)
    ssbo.velocities_x[idx] *= 0.99f;
    ssbo.velocities_y[idx] *= 0.99f;
    ssbo.velocities_z[idx] *= 0.99f;

    // Clamp velocity to prevent extreme speeds
    float vx = ssbo.velocities_x[idx];
    float vy = ssbo.velocities_y[idx];
    float vz = ssbo.velocities_z[idx];
    float v2 = vx*vx + vy*vy + vz*vz;
    if (v2 > 2500.0f) {
        float scale = 50.0f / sqrtf_(v2);
        ssbo.velocities_x[idx] = vx * scale;
        ssbo.velocities_y[idx] = vy * scale;
        ssbo.velocities_z[idx] = vz * scale;
    }

    // 7. Update position from velocity
    ssbo.positions_x[idx] += ssbo.velocities_x[idx] * dt;
    ssbo.positions_y[idx] += ssbo.velocities_y[idx] * dt;
    ssbo.positions_z[idx] += ssbo.velocities_z[idx] * dt;
}

// ── Interaction kernel: pillar influence between nearby agents ──

void transform_interaction(
    uint32_t idx,
    TransformSSBO& ssbo,
    float dt,
    uint32_t entity_count
) {
    if (!ssbo.active[idx]) return;

    float px = ssbo.positions_x[idx];
    float py = ssbo.positions_y[idx];
    float pz = ssbo.positions_z[idx];

    fp20_t p[NumPillars];
    for (int i = 0; i < NumPillars; i++) {
        p[i] = ssbo.pillars[i][idx];
    }

    float force_f = p[Force].to_float();
    if (force_f < 0.01f) return;

    // Scan nearby agents for interaction (simplified: fixed stride)
    // In a full implementation, this would use spatial hashing results.
    uint32_t count = entity_count;
    uint32_t search_radius = 5;
    for (uint32_t j = idx + 1; j < count && j <= idx + search_radius; j++) {
        if (!ssbo.active[j]) continue;

        float ox = ssbo.positions_x[j];
        float oy = ssbo.positions_y[j];
        float oz = ssbo.positions_z[j];

        float dx = ox - px;
        float dy = oy - py;
        float dz = oz - pz;
        float dist_sq = dx*dx + dy*dy + dz*dz;
        if (dist_sq < 1e-8f || dist_sq > 100.0f) continue;

        float dist = sqrtf_(dist_sq);
        float influence = force_f / (dist + 1e-6f);

        // Bloch-rotate target pillars
        for (int i = 0; i < NumPillars; i++) {
            float delta = influence * dt * 0.01f;
            if (fabsf_(delta) > 1e-8f) {
                fp20_t current = ssbo.pillars[i][j];
                ssbo.pillars[i][j] = bloch_rotate(current, delta);
            }
        }
    }
}

// ── Decay phase: environmental effects on pillars ──

void transform_decay(
    uint32_t idx,
    TransformSSBO& ssbo,
    float dt,
    float hazard_level,
    float resource_density
) {
    if (!ssbo.active[idx]) return;

    fp20_t harm = ssbo.pillars[Harm][idx];
    float harm_f = harm.to_float() + hazard_level * 0.01f * dt;
    if (harm_f > 1.0f) harm_f = 1.0f;
    ssbo.pillars[Harm][idx] = fp20_t(harm_f);

    fp20_t attraction = fp20_t(resource_density);
    ssbo.pillars[Attraction][idx] = attraction;
}

// ── Main entry point (called by Vulkan dispatch) ──

extern "C" void transform_compute_main_soa(
    float* positions_x,
    float* positions_y,
    float* positions_z,
    float* velocities_x,
    float* velocities_y,
    float* velocities_z,
    fp20_t* pillar_awareness,
    fp20_t* pillar_willpower,
    fp20_t* pillar_force,
    fp20_t* pillar_influence,
    fp20_t* pillar_resistance,
    fp20_t* pillar_integrity,
    fp20_t* pillar_cohesion,
    fp20_t* pillar_relation,
    fp20_t* pillar_presence,
    fp20_t* pillar_warmth,
    fp20_t* pillar_memory,
    fp20_t* pillar_attraction,
    fp20_t* pillar_harm,
    fp20_t* pillar_distortion,
    fp20_t* pillar_flux,
    fp20_t* pillar_depth,
    uint32_t* active,
    int32_t* resources,
    uint32_t entity_count,
    float dt,
    uint32_t mode,
    float hazard_level,
    float resource_density
) {
    // Build SSBO wrapper
    TransformSSBO ssbo;
    ssbo.positions_x = positions_x;
    ssbo.positions_y = positions_y;
    ssbo.positions_z = positions_z;
    ssbo.velocities_x = velocities_x;
    ssbo.velocities_y = velocities_y;
    ssbo.velocities_z = velocities_z;
    ssbo.pillars[0] = pillar_awareness;    // Awareness = 0
    ssbo.pillars[1] = pillar_willpower;    // Willpower = 1
    ssbo.pillars[2] = pillar_force;        // Force = 2
    ssbo.pillars[3] = pillar_influence;    // Influence = 3
    ssbo.pillars[4] = pillar_resistance;   // Resistance = 4
    ssbo.pillars[5] = pillar_integrity;    // Integrity = 5
    ssbo.pillars[6] = pillar_cohesion;     // Cohesion = 6
    ssbo.pillars[7] = pillar_relation;     // Relation = 7
    ssbo.pillars[8] = pillar_presence;     // Presence = 8
    ssbo.pillars[9] = pillar_warmth;       // Warmth = 9
    ssbo.pillars[10] = pillar_memory;      // Memory = 10
    ssbo.pillars[11] = pillar_attraction;  // Attraction = 11
    ssbo.pillars[12] = pillar_harm;        // Harm = 12
    ssbo.pillars[13] = pillar_distortion;  // Distortion = 13
    ssbo.pillars[14] = pillar_flux;        // Flux = 14
    ssbo.pillars[15] = pillar_depth;       // Depth = 15
    ssbo.active = active;
    ssbo.resources = resources;

    switch (mode) {
        case 0:  // Update
            for (uint32_t i = 0; i < entity_count; i++) {
                transform_entity(i, ssbo, dt);
            }
            break;

        case 1:  // Interaction
            if (entity_count > 1) {
                for (uint32_t i = 0; i < entity_count - 1; i++) {
                    transform_interaction(i, ssbo, dt, entity_count);
                }
            }
            break;

        case 2:  // Decay
            for (uint32_t i = 0; i < entity_count; i++) {
                transform_decay(i, ssbo, dt, hazard_level, resource_density);
            }
            break;

        case 3:  // Full pipeline: update + interaction + decay
            for (uint32_t i = 0; i < entity_count; i++) {
                transform_entity(i, ssbo, dt);
            }
            if (entity_count > 1) {
                for (uint32_t i = 0; i < entity_count - 1; i++) {
                    transform_interaction(i, ssbo, dt, entity_count);
                }
            }
            for (uint32_t i = 0; i < entity_count; i++) {
                transform_decay(i, ssbo, dt, hazard_level, resource_density);
            }
            break;
    }
}
