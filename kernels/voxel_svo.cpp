// Van Nueman SVO (Sparse Voxel Octree) - vncc C++ Version
// Compile: vncc -emit-spirv voxel_svo.cpp -o voxel_svo.spv
//          vncc -emit-llvm voxel_svo.cpp -o voxel_svo.bc

#include <cstdint>
#include <cmath>

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

// Global node pool (vncc handles device/host)
SVO_Node* g_node_pool = nullptr;
uint32_t g_node_pool_size = 0;
uint32_t g_node_pool_used = 0;

// Initialize SVO node pool
void svo_init(SVO_Node* node_pool, uint32_t pool_size) {
    g_node_pool = node_pool;
    g_node_pool_size = pool_size;
    g_node_pool_used = 1;  // root at index 0
    if (g_node_pool) {
        g_node_pool[0].valid_mask = 0;
        for (int i = 0; i < 8; i++) g_node_pool[0].children[i] = 0;
    }
}

// Allocate a node from pool
uint32_t svo_alloc_node() {
    if (g_node_pool_used >= g_node_pool_size) return 0;
    uint32_t idx = g_node_pool_used++;
    SVO_Node* node = &g_node_pool[idx];
    node->valid_mask = 0;
    for (int i = 0; i < 8; i++) node->children[i] = 0;
    return idx;
}

// Hash function for 3D coordinates at a given LOD
uint32_t svo_hash(int x, int y, int z, int lod) {
    return ((x * 73856093) ^ (y * 19349663) ^ (z * 83492791) ^ (lod * 1234567)) & 0x7FFFFFFF;
}

// Get octant index for a given position relative to center
int get_octant(int px, int py, int pz, int cx, int cy, int cz) {
    int ox = (px >= cx) ? 1 : 0;
    int oy = (py >= cy) ? 1 : 0;
    int oz = (pz >= cz) ? 1 : 0;
    return (oz << 2) | (oy << 1) | ox;
}

// Octohedron math: project position onto octohedral surface
float3 octohedron_project(float3 pos, float size) {
    float3 n = float3_normalize(pos);
    float3 abs_n = make_float3(fabsf(n.x), fabsf(n.y), fabsf(n.z));
    float max_comp = fmaxf(abs_n.x, fmaxf(abs_n.y, abs_n.z));
    if (max_comp > 0) {
        n.x /= max_comp; n.y /= max_comp; n.z /= max_comp;
    }
    return float3_mul_scalar(n, size);
}

// Build SVO from dirty chunks (called per chunk)
void svo_build(SVO_Chunk* chunks, uint32_t num_chunks, uint32_t* dirty_voxels) {
    for (uint32_t chunk_idx = 0; chunk_idx < num_chunks; chunk_idx++) {
        if (!chunks[chunk_idx].dirty) continue;

        SVO_Chunk& chunk = chunks[chunk_idx];
        // Simple rebuild: traverse and update nodes
        chunk.dirty = 0;
    }
}

// Mark chunk dirty for rebuild
void svo_mark_dirty(SVO_Chunk* chunks, uint32_t num_chunks,
                     uint32_t x, uint32_t y, uint32_t z) {
    for (uint32_t idx = 0; idx < num_chunks; idx++) {
        if (chunks[idx].x == x && chunks[idx].y == y && chunks[idx].z == z) {
            chunks[idx].dirty = 1;
        }
    }
}

// SVO ray intersection (for rendering)
// Returns: 0 = no hit, >0 = t_max - t_min (depth)
float svo_ray_intersect(SVO_Node* nodes, uint32_t node_idx,
                          float3 ray_origin, float3 ray_dir,
                          float3 box_min, float3 box_max,
                          float* t_out, float3* normal_out) {
    // Simple AABB test first
    float t_min = 0.0f, t_max = 1e10f;

    float3 inv_dir = make_float3(1.0f / (ray_dir.x + 1e-8f),
                                  1.0f / (ray_dir.y + 1e-8f),
                                  1.0f / (ray_dir.z + 1e-8f));

    float3 t1 = float3_mul_scalar(float3_sub(box_min, ray_origin), inv_dir.x);
    float3 t2 = float3_mul_scalar(float3_sub(box_max, ray_origin), inv_dir.x);

    float3 t_min_v = make_float3(fminf(t1.x, t2.x), fminf(t1.y, t2.y), fminf(t1.z, t2.z));
    float3 t_max_v = make_float3(fmaxf(t1.x, t2.x), fmaxf(t1.y, t2.y), fmaxf(t1.z, t2.z));

    t_min = fmaxf(fmaxf(t_min_v.x, t_min_v.y), t_min_v.z);
    t_max = fminf(fminf(t_max_v.x, t_max_v.y), t_max_v.z);

    if (t_min > t_max || t_max < 0) return 0.0f;

    *t_out = t_min;

    // Calculate normal (simplified)
    float3 hit = float3_add(ray_origin, float3_mul_scalar(ray_dir, t_min));
    float3 center = float3_mul_scalar(float3_add(box_min, box_max), 0.5f);
    float3 local = float3_sub(hit, center);

    float3 abs_local = make_float3(fabsf(local.x), fabsf(local.y), fabsf(local.z));
    if (abs_local.x >= abs_local.y && abs_local.x >= abs_local.z)
        *normal_out = make_float3(local.x > 0 ? 1 : -1, 0, 0);
    else if (abs_local.y >= abs_local.x && abs_local.y >= abs_local.z)
        *normal_out = make_float3(0, local.y > 0 ? 1 : -1, 0);
    else
        *normal_out = make_float3(0, 0, local.z > 0 ? 1 : -1);

    return t_max - t_min;  // "depth" of intersection
}

// Main entry point for SVO operations
extern "C" void svo_main(
    SVO_Node* node_pool,
    uint32_t pool_size,
    SVO_Chunk* chunks,
    uint32_t num_chunks,
    uint32_t* dirty_voxels,
    int mode,  // 0 = init, 1 = build, 2 = mark dirty
    uint32_t mx, uint32_t my, uint32_t mz
) {
    switch (mode) {
        case 0:
            svo_init(node_pool, pool_size);
            break;
        case 1:
            svo_build(chunks, num_chunks, dirty_voxels);
            break;
        case 2:
            svo_mark_dirty(chunks, num_chunks, mx, my, mz);
            break;
    }
}
