// skelly_api.h - C API header for Skelly API
// Use this instead of including .cu files directly

#pragma once

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque context pointer
typedef struct SkellyAPIContextImpl* SkellyAPIContext;

// Skelly structure types
typedef struct BoneNode {
    uint32_t id;
    float local_x, local_y, local_z;
    float global_x, global_y, global_z;
    int32_t parent_idx;  // -1 for root
    uint32_t is_fractured;
    float constraint_pitch_min, constraint_pitch_max;
    float constraint_yaw_min, constraint_yaw_max;
    float constraint_roll_min, constraint_roll_max;
} BoneNode;

typedef struct BoneSegment {
    uint32_t id;
    int32_t start_node_idx;  // -1 if invalid
    int32_t end_node_idx;
    float flexibility;
    float break_threshold;
    uint32_t is_fractured;
    float total_capacity;
    uint32_t organ_count;
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
} MuscleGroup;

typedef struct Organ {
    uint32_t id;
    int32_t type;  // 0=pump, 1=valve, 2=power_plant, 3=factory
    float volume;
    float active_state;
    float energy_output;
    int32_t segment_idx;
} Organ;

// API functions
SkellyAPIContext skelly_api_init();
void skelly_api_cleanup(SkellyAPIContext ctx);

// Create creature
int32_t skelly_api_create_creature(SkellyAPIContext ctx, int32_t parent_idx, 
                                     float x, float y, float z, const char* name);

// Add organ to segment
int32_t skelly_api_add_organ(SkellyAPIContext ctx, const char* name, int32_t type, 
                              int32_t segment_idx, float volume);

// Set organ activity
void skelly_api_set_organ_activity(SkellyAPIContext ctx, const char* organ_name, float intensity);

// Step simulation
void skelly_api_step(SkellyAPIContext ctx, float dt);

// Get system state
typedef struct SystemState {
    float total_energy;
    float pressure_map[1024];  // Simplified: fixed size
    uint32_t is_alive;
} SystemState;

SystemState skelly_api_get_system_state(SkellyAPIContext ctx);

#ifdef __cplusplus
}
#endif
