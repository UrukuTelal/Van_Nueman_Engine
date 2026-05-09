// SkellyGPU.h - GPU structures for SDF raymarching
// Must match GLSL layout (std430) for compute shader
#pragma once
#include <cstdint>

struct BoneGPU {
    float start_x, start_y, start_z;  // vec3 start
    float _pad0;                       // align to vec4
    float end_x, end_y, end_z;        // vec3 end  
    float radius;                       // end.w in GLSL
    float r, g, b, a;                  // color (glow / health tint)
};

struct MuscleGPU {
    float start_x, start_y, start_z;  // vec3 start
    float _pad0;                       // align to vec4
    float end_x, end_y, end_z;        // vec3 end
    float radius;                       // end.w in GLSL
    float activation;                    // 0.0-1.0, affects color intensity
    float thickness;
    float r, g, b, a;
    float _pad[1];
};

struct OrganGPU {
    float center_x, center_y, center_z;  // vec3 center
    float radius;                         // center.w in GLSL
    uint32_t type;
    float r, g, b, a;
    float pulse_intensity;
    uint32_t _pad[2];
};

