// SkellyGPU.h - GPU structures for SDF raymarching
// Must match GLSL layout (std430) for compute shader
// See render_skelly_sdf.comp for the GPU-side definitions
#pragma once
#include <cstdint>

struct BoneGPU {
    float start_x, start_y, start_z, _pad0;  // vec4 start (.w unused)
    float end_x,   end_y,   end_z,   radius;  // vec4 end   (.w = radius)
};

struct MuscleGPU {
    float start_x, start_y, start_z, _pad0;  // vec4 start (.w unused)
    float end_x,   end_y,   end_z,   radius;  // vec4 end   (.w = radius)
    float activation;
    float _pad[3];
};

struct OrganGPU {
    float center_x, center_y, center_z, radius;  // vec4 center (.w = radius)
    uint32_t type;
    uint32_t _pad[3];
};
