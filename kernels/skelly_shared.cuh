#pragma once
#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <cmath>

#include "voxel_shared.cuh"
#include "pillars_shared.cuh"

#define MAX_BONES 1000000
#define MAX_SEGMENTS 1000000
#define MAX_MUSCLES 2000000
#define MAX_ORGANS 500000
#define MAX_TRANSPORTS 500000

#define _USE_MATH_DEFINES
constexpr float SK_PI = 3.14159265358979f;

struct BoneNode {
    float3 local_pos;
    float3 global_pos;
    uint32_t parent_idx;
    uint32_t is_fractured;
    float constraint_pitch_min, constraint_pitch_max;
    float constraint_yaw_min, constraint_yaw_max;
    float constraint_roll_min, constraint_roll_max;
};

struct BoneSegment {
    uint32_t start_node, end_node;
    float flexibility;
    float break_threshold;
    uint32_t is_fractured;
    float total_capacity;
    uint32_t organ_count;
    uint32_t organ_start;
};

struct MuscleStrand {
    float base_r;
    float current_r;
    float origin_rot, insertion_rot;
};

struct MuscleGroup {
    uint32_t origin_node, insertion_node;
    uint32_t strand_start;
    uint32_t strand_count;
    float activation;
    float current_volume;
};

struct Organ {
    uint32_t type;
    float volume;
    float active_state;
    float energy_output;
    uint32_t segment_idx;
};

struct Transport {
    uint32_t start_node, end_node;
    float flow_rate;
    float resistance;
    float pressure;
    uint32_t is_severed;
    float elasticity;
};

struct InterstitialFluid {
    uint32_t transport_idx;
    uint32_t segment_idx;
    float turgor_pressure;
};

struct SkellyInstance {
    uint32_t entity_id;
    uint32_t bone_start, bone_count;
    uint32_t segment_start, segment_count;
    uint32_t muscle_start, muscle_count;
    uint32_t organ_start, organ_count;
    uint32_t transport_start, transport_count;
};
