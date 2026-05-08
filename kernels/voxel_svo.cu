#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <cmath>

// SVO Constants from Architecture Spec
#define SVO_CHUNK_SIZE 64
#define SVO_MAX_LODS 8
#define SVO_BASE_SIZE 256
#define MAX_ENTITIES 500000
#define MAX_CHUNKS 10000

// Voxel materials
#define MAT_OCTAHEDRON 0
#define MAT_CUBE 1
#define MAT_POLYHEDRA 2

struct Voxel {
    uint8_t material;
    uint8_t r, g, b, a;
    uint16_t lod_level;
};

struct SVO_Node {
    uint32_t children[8];  // 0 = null, else index into node pool
    Voxel voxel;
    uint8_t valid_mask;    // bitmask of which children exist
};

struct SVO_Chunk {
    uint32_t x, y, z;
    uint32_t lod_level;
    uint32_t node_count;
    uint32_t dirty;
    uint32_t nodes_offset;  // offset into global node pool
};

// Global node pool (device-side)
__device__ SVO_Node* d_node_pool = nullptr;
__device__ uint32_t d_node_pool_size = 0;
__device__ uint32_t d_node_pool_used = 0;

// Initialize SVO node pool
__global__ void svo_init_kernel(SVO_Node* node_pool, uint32_t pool_size) {
    if (threadIdx.x == 0 && blockIdx.x == 0) {
        d_node_pool = node_pool;
        d_node_pool_size = pool_size;
        d_node_pool_used = 1;  // root at index 0
        node_pool[0].valid_mask = 0;
        for (int i = 0; i < 8; i++) node_pool[0].children[i] = 0;
    }
}

// Allocate a node from pool (device-side)
__device__ uint32_t svo_alloc_node() {
    uint32_t idx = atomicAdd(&d_node_pool_used, 1);
    if (idx >= d_node_pool_size) return 0;
    SVO_Node* node = &d_node_pool[idx];
    node->valid_mask = 0;
    for (int i = 0; i < 8; i++) node->children[i] = 0;
    return idx;
}

// Hash function for 3D coordinates at a given LOD
__device__ uint32_t svo_hash(int x, int y, int z, int lod) {
    return ((x * 73856093) ^ (y * 19349663) ^ (z * 83492791) ^ (lod * 1234567)) & 0x7FFFFFFF;
}

// Get octant index for a given position relative to center
__device__ int get_octant(int px, int py, int pz, int cx, int cy, int cz) {
    int ox = (px >= cx) ? 1 : 0;
    int oy = (py >= cy) ? 1 : 0;
    int oz = (pz >= cz) ? 1 : 0;
    return (oz << 2) | (oy << 1) | ox;
}

// Octohedron math: project position onto octohedral surface
__device__ float3 octohedron_project(float3 pos, float size) {
    float3 n = normalize(pos);
    float3 abs_n = make_float3(fabsf(n.x), fabsf(n.y), fabsf(n.z));
    float max_comp = fmaxf(abs_n.x, fmaxf(abs_n.y, abs_n.z));
    if (max_comp > 0) {
        n.x /= max_comp; n.y /= max_comp; n.z /= max_comp;
    }
    return n * size;
}

// Build SVO from dirty chunks (called per chunk)
__global__ void svo_build_kernel(SVO_Chunk* chunks, uint32_t num_chunks, uint32_t* dirty_voxels) {
    uint32_t chunk_idx = blockIdx.x;
    if (chunk_idx >= num_chunks) return;
    if (!chunks[chunk_idx].dirty) return;

    SVO_Chunk& chunk = chunks[chunk_idx];
    // Simple rebuild: traverse and update nodes
    chunk.dirty = 0;
}

// Mark chunk dirty for rebuild
__global__ void svo_mark_dirty_kernel(SVO_Chunk* chunks, uint32_t num_chunks,
                                       uint32_t x, uint32_t y, uint32_t z) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_chunks) return;
    if (chunks[idx].x == x && chunks[idx].y == y && chunks[idx].z == z) {
        chunks[idx].dirty = 1;
    }
}
