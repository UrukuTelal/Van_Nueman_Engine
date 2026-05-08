// SkellyGPU.h - GPU structures for SDF raymarching
// Must match GLSL layout (std430) for compute shader
#pragma once
#include <cstdint>

struct BoneGPU {
    float start_x, start_y, start_z;  // vec3 start
    float end_x, end_y, end_z;        // vec3 end  
    float radius;                       // stored in .w of end in GLSL (adjusted in shader)
    uint32_t _pad;
};

struct MuscleGPU {
    float start_x, start_y, start_z;  // vec3 start
    float end_x, end_y, end_z;        // vec3 end
    float radius;                       // stored in .w of end in GLSL
    float activation;                    // 0.0-1.0, affects color intensity
};

struct OrganGPU {
    float center_x, center_y, center_z;  // vec3 center
    float radius;                         // stored in .w of center in GLSL
    uint32_t type;                        // 0=pump, 1=valve, 2=power_plant, 3=factory
    uint32_t _pad[3];
};
