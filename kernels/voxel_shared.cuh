#pragma once
#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <cmath>

// Float3 operators for device code
__device__ inline float3 operator+(float3 a, float3 b) { return make_float3(a.x+b.x, a.y+b.y, a.z+b.z); }
__device__ inline float3 operator-(float3 a, float3 b) { return make_float3(a.x-b.x, a.y-b.y, a.z-b.z); }
__device__ inline float3 operator*(float3 a, float s) { return make_float3(a.x*s, a.y*s, a.z*s); }
__device__ inline float3 operator*(float s, float3 a) { return make_float3(a.x*s, a.y*s, a.z*s); }
__device__ inline float dot(float3 a, float3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
__device__ inline float3 cross(float3 a, float3 b) {
    return make_float3(a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x);
}
__device__ inline float length(float3 v) { return sqrtf(dot(v, v)); }
__device__ inline float3 normalize(float3 v) { float l = length(v); return l > 0 ? v * (1.0f/l) : v; }

#define SVO_CHUNK_SIZE 64
#define SVO_MAX_LODS 8
#define SVO_BASE_SIZE 256
#define MAX_ENTITIES 500000
#define MAX_CHUNKS 10000

#define MAT_OCTAHEDRON 0
#define MAT_CUBE 1
#define MAT_POLYHEDRA 2

struct Voxel {
    uint8_t material;
    uint8_t r, g, b, a;
    uint16_t lod_level;
};

struct SVO_Node {
    uint32_t children[8];
    Voxel voxel;
    uint8_t valid_mask;
};

struct SVO_Chunk {
    uint32_t x, y, z;
    uint32_t lod_level;
    uint32_t node_count;
    uint32_t dirty;
    uint32_t nodes_offset;
};
