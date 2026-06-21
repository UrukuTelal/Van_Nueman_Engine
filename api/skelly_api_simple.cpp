// skelly_api_simple.cpp - Simple C++ implementation of Skelly API
#include <cstdint>
#include <cstring>
#include <cmath>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque context
struct SkellyAPIContextImpl {
    // Simple implementation - just track a few variables
    float energy;
    float pressure;
    bool alive;
    uint32_t creature_count;
};

typedef SkellyAPIContextImpl* SkellyAPIContext;

// Skelly structure types (simplified)
typedef struct BoneNode {
    uint32_t id;
    float local_x, local_y, local_z;
    float global_x, global_y, global_z;
    int32_t parent_idx;
    uint32_t is_fractured;
    float constraint_pitch_min, constraint_pitch_max;
    float constraint_yaw_min, constraint_yaw_max;
    float constraint_roll_min, constraint_roll_max;
} BoneNode;

typedef struct BoneSegment {
    uint32_t id;
    int32_t start_node_idx;
    int32_t end_node_idx;
    float flexibility;
    float break_threshold;
    uint32_t is_fractured;
    float total_capacity;
    uint32_t organ_count;
    uint32_t organ_start;    // internal: index into organ list
} BoneSegment;

typedef struct MuscleStrand {
    float base_r;
    float current_r;
    float origin_rot;
    float insertion_rot;
} MuscleStrand;

typedef struct MuscleGroup {
    uint32_t id;
    int32_t origin_node_idx;
    int32_t insertion_node_idx;
    float activation;
    uint32_t strand_start_idx;
    uint32_t strand_count;
    float current_volume;    // internal
} MuscleGroup;

typedef struct Organ {
    uint32_t id;
    int32_t type;
    float volume;
    float active_state;
    float energy_output;
    int32_t segment_idx;
} Organ;

// API functions
SkellyAPIContext skelly_api_init() {
    auto* ctx = new SkellyAPIContextImpl();
    ctx->energy = 100.0f;
    ctx->pressure = 1.0f;
    ctx->alive = true;
    ctx->creature_count = 0;
    return ctx;
}

void skelly_api_cleanup(SkellyAPIContext ctx) {
    delete (SkellyAPIContextImpl*)ctx;
}

int32_t skelly_api_create_creature(SkellyAPIContext ctx, int32_t parent_idx,
                                     float x, float y, float z, const char* name) {
    auto* c = (SkellyAPIContextImpl*)ctx;
    c->creature_count++;
    return c->creature_count;  // Return creature ID
}

int32_t skelly_api_add_organ(SkellyAPIContext ctx, const char* name, int32_t type,
                              int32_t segment_idx, float volume) {
    return 1;  // organ ID
}

void skelly_api_set_organ_activity(SkellyAPIContext ctx, const char* organ_name, float intensity) {
    // Simple: do nothing for now
}

void skelly_api_step(SkellyAPIContext ctx, float dt) {
    auto* c = (SkellyAPIContextImpl*)ctx;
    // Simple: energy decreases over time
    c->energy -= dt * 0.1f;
    if (c->energy < 0) c->energy = 0;
    c->pressure = 1.0f + sinf(c->creature_count * dt) * 0.2f;
}

typedef struct SystemState {
    float total_energy;
    float pressure_map[1024];
    uint32_t is_alive;
} SystemState;

SystemState skelly_api_get_system_state(SkellyAPIContext ctx) {
    auto* c = (SkellyAPIContextImpl*)ctx;
    SystemState state;
    state.total_energy = c->energy;
    for (int i = 0; i < 1024; i++) state.pressure_map[i] = c->pressure;
    state.is_alive = c->alive ? 1 : 0;
    return state;
}

#ifdef __cplusplus
}
#endif
