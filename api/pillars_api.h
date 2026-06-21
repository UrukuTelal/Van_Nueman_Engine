// pillars_api.h - C API header for Pillar API
// Use this instead of including .cu files directly

#pragma once

#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle (cast from struct PillarsAPIContext defined in pillars_api_simple.cpp)
typedef void* PillarsAPIContext;

PillarsAPIContext pillars_api_init(uint32_t max_entities);
void pillars_api_reset(PillarsAPIContext ctx, uint32_t count);
void pillars_api_update(PillarsAPIContext ctx, float dt);
void pillars_api_cleanup(PillarsAPIContext ctx);

#ifdef __cplusplus
}
#endif

// ── Inline C++ math helpers (standalone, no context needed) ──
#ifdef __cplusplus
#include <cmath>
#include <algorithm>

inline float pillars_api_value_to_theta(float value) {
    float v = (value < 0.0f) ? 0.0f : (value > 1.0f) ? 1.0f : value;
    return std::acos(2.0f * v - 1.0f);
}

inline float pillars_api_theta_to_value(float theta) {
    float t = (theta < 0.0f) ? 0.0f : (theta > 3.14159265f) ? 3.14159265f : theta;
    return (std::cos(t) + 1.0f) * 0.5f;
}

inline float pillars_api_apply_bloch_rotation(float current_value, float delta_theta) {
    float theta = pillars_api_value_to_theta(current_value);
    float t = theta + delta_theta;
    t = std::fmod(t, 6.283185307179586f);
    if (t < 0.0f) t += 6.283185307179586f;
    if (t > 3.14159265f) t = 6.283185307179586f - t;
    return pillars_api_theta_to_value(t);
}

inline float pillars_api_compute_angular_shift(float incoming_force, float operator_magnitude,
                                                float willpower, float depth) {
    if (willpower < 0.001f || depth < 0.001f) return 0.0f;
    return (incoming_force * operator_magnitude) / (willpower * depth);
}
#endif
