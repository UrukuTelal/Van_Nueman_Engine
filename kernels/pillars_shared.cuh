#pragma once
#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <cmath>

#include "PillarEnum.h"

#define MAX_ENTITIES 500000
#define MAX_SERVERS 1024
#define MAX_FEDERATIONS 64
#define TICK_RATE 30

struct PillarStateVector {
    float p[NumPillars];
    __device__ inline float& operator[](int idx) { return p[idx]; }
    __device__ inline const float& operator[](int idx) const { return p[idx]; }
};

struct GPUEntity {
    uint32_t id;
    uint32_t type;
    float3 position;
    float3 velocity;
    PillarStateVector pillars;
    PillarStateVector baseline;
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

__device__ inline float calculate_coherence(const PillarStateVector* psv) {
    float sum = 0.0f;
    for (int i = 0; i < NumPillars; ++i) {
        sum += psv->p[i];
    }
    return sum / NumPillars;
}

__device__ inline float bloch_value_to_theta(float v) {
    if (v <= 0.0f) return 3.14159265f;
    if (v >= 1.0f) return 0.0f;
    return acosf(2.0f * v - 1.0f);
}

__device__ inline float bloch_theta_to_value(float theta) {
    return (cosf(theta) + 1.0f) * 0.5f;
}

__device__ inline float bloch_rotate(float current, float delta_theta) {
    float theta = bloch_value_to_theta(current);
    return bloch_theta_to_value(theta + delta_theta);
}

__device__ inline float calculate_influence(float force, float attraction,
                                             float resistance, float willpower,
                                             float depth) {
    float raw = force * attraction;
    float damping = resistance * willpower * (depth + 0.1f);
    if (damping < 0.001f) damping = 0.001f;
    return raw / damping;
}

extern __constant__ float interaction_matrix[NumPillars][NumPillars];

__device__ inline float bloch_apply_distortion_torsion(float awareness, float distortion) {
    float theta = bloch_value_to_theta(awareness);
    float phi_twist = distortion * 3.14159265f;
    return bloch_theta_to_value(theta + phi_twist);
}
