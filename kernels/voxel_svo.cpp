// Van Nueman SVO (Sparse Voxel Octree) - UGC C++ Version
// Compile: ugc -o voxel_svo.spv voxel_svo.cpp
//          ugc -o voxel_svo.bc voxel_svo.cpp

#ifndef UGC_COMPILER
#include <cstdint>
#endif
#include <cmath>

// SPIR-V compatible math builtins (map to GLSL.std.450)
namespace {
inline float sqrtf_(float x)  { return __builtin_elementwise_sqrt(x); }
inline float fabsf_(float x)  { return __builtin_elementwise_abs(x); }
inline float fmaxf_(float a, float b) { return __builtin_elementwise_max(a, b); }
inline float fminf_(float a, float b) { return __builtin_elementwise_min(a, b); }
}

// Float3 operations (vncc maps to SPIR-V/PTX float3)
struct float3 { float x, y, z; };

float3 make_float3(float x, float y, float z) {
    float3 r = {x, y, z};
    return r;
}

float3 float3_add(float3 a, float3 b) {
    return make_float3(a.x + b.x, a.y + b.y, a.z + b.z);
}

float3 float3_sub(float3 a, float3 b) {
    return make_float3(a.x - b.x, a.y - b.y, a.z - b.z);
}

float3 float3_mul_scalar(float3 a, float s) {
    return make_float3(a.x * s, a.y * s, a.z * s);
}

float float3_dot(float3 a, float3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float float3_length(float3 v) {
    return sqrtf_(float3_dot(v, v));
}

float3 float3_normalize(float3 v) {
    float l = float3_length(v);
    return l > 0 ? float3_mul_scalar(v, 1.0f / l) : v;
}

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
    float3 abs_n = make_float3(fabsf_(n.x), fabsf_(n.y), fabsf_(n.z));
    float max_comp = fmaxf_(abs_n.x, fmaxf_(abs_n.y, abs_n.z));
    if (max_comp > 0) {
        n.x /= max_comp; n.y /= max_comp; n.z /= max_comp;
    }
    return float3_mul_scalar(n, size);
}

// ---------------------------------------------------------------------------
// Procedural density evaluation (sphere + octahedron inside each chunk)
// ---------------------------------------------------------------------------

// Evaluate density at a single world-space point.
// Returns: -1 = empty, 0 = MAT_OCTAHEDRON, 1 = MAT_CUBE, 2 = MAT_POLYHEDRA
static int eval_density(float px, float py, float pz,
                        float chunk_ox, float chunk_oy, float chunk_oz) {
    float lx = px - chunk_ox;
    float ly = py - chunk_oy;
    float lz = pz - chunk_oz;
    float half = (float)SVO_CHUNK_SIZE * 0.5f;
    float3 local = make_float3(lx - half, ly - half, lz - half);

    float sphere_dist = float3_length(local);
    float sphere_radius = half * 0.35f;
    float oct_dist = fabsf_(local.x) + fabsf_(local.y) + fabsf_(local.z);
    float oct_size = half * 0.65f;

    if (sphere_dist < sphere_radius)
        return MAT_OCTAHEDRON;
    if (oct_dist < oct_size && sphere_dist < half * 0.9f)
        return MAT_CUBE;
    return -1;
}

// Classify a sub-volume: -1 empty, -2 mixed, 0-2 uniform material
static int eval_density_volume(float3 center, float half_size,
                                float chunk_ox, float chunk_oy, float chunk_oz) {
    int first_mat = -1;
    int has_empty = 0;

    for (int i = 0; i < 8; i++) {
        float sx = (i & 1) ? half_size : -half_size;
        float sy = (i & 2) ? half_size : -half_size;
        float sz = (i & 4) ? half_size : -half_size;

        int m = eval_density(center.x + sx, center.y + sy, center.z + sz,
                             chunk_ox, chunk_oy, chunk_oz);
        if (m < 0)
            has_empty = 1;
        else if (first_mat < 0)
            first_mat = m;
        else if (first_mat != m)
            return -2;  // mixed materials
    }

    int cm = eval_density(center.x, center.y, center.z,
                          chunk_ox, chunk_oy, chunk_oz);
    if (cm < 0)
        has_empty = 1;
    else if (first_mat < 0)
        first_mat = cm;
    else if (first_mat != cm)
        return -2;

    if (first_mat < 0) return -1;  // all empty
    if (has_empty)     return -2;  // mixed solid/empty
    return first_mat;              // uniform
}

// Recursively build the octree for a sub-volume.
// Returns: node pool index of the sub-tree root (0 = empty).
static uint32_t build_subtree(float3 center, float half_size, int depth,
                              float chunk_ox, float chunk_oy, float chunk_oz) {
    int dv = eval_density_volume(center, half_size, chunk_ox, chunk_oy, chunk_oz);
    if (dv < 0 && depth < SVO_MAX_LODS) {
        // Mixed (-2) and not at max depth → subdivide.
        // Fully empty (-1) at any depth → stop (return 0).
        // Mix at leaf depth → fall through to create leaf.
    }
    if (dv < 0 && dv != -2) return 0;          // fully empty
    if (dv >= 0 || depth >= SVO_MAX_LODS) {    // uniform solid or at leaf depth
        uint32_t idx = svo_alloc_node();
        if (idx == 0) return 0;
        uint8_t mat = (uint8_t)((dv >= 0) ? dv : MAT_POLYHEDRA);
        g_node_pool[idx].voxel.material = mat;
        g_node_pool[idx].voxel.r = mat == MAT_OCTAHEDRON ? 200
                                  : mat == MAT_CUBE       ? 100 : 150;
        g_node_pool[idx].voxel.g = mat == MAT_OCTAHEDRON ? 100
                                  : mat == MAT_CUBE       ? 200 : 100;
        g_node_pool[idx].voxel.b = mat == MAT_OCTAHEDRON ? 100
                                  : mat == MAT_CUBE       ? 100 : 200;
        g_node_pool[idx].voxel.a = 255;
        g_node_pool[idx].voxel.lod_level = (uint16_t)depth;
        return idx;
    }

    // dv == -2 (mixed): subdivide into up to 8 children.
    uint32_t node_idx = svo_alloc_node();
    if (node_idx == 0) return 0;
    SVO_Node* node = &g_node_pool[node_idx];
    node->valid_mask = 0;

    float child_half = half_size * 0.5f;
    for (int octant = 0; octant < 8; octant++) {
        float ox = (octant & 1) ? child_half : -child_half;
        float oy = (octant & 2) ? child_half : -child_half;
        float oz = (octant & 4) ? child_half : -child_half;
        float3 cc = make_float3(center.x + ox, center.y + oy, center.z + oz);

        uint32_t ci = build_subtree(cc, child_half, depth + 1,
                                     chunk_ox, chunk_oy, chunk_oz);
        if (ci != 0) {
            node->children[octant] = ci;
            node->valid_mask |= (uint8_t)(1 << octant);
        }
    }

    return node_idx;
}

// Build SVO from dirty chunks (called per chunk)
void svo_build(SVO_Chunk* chunks, uint32_t num_chunks, uint32_t* dirty_voxels) {
    (void)dirty_voxels;
    for (uint32_t chunk_idx = 0; chunk_idx < num_chunks; chunk_idx++) {
        if (!chunks[chunk_idx].dirty) continue;

        SVO_Chunk& chunk = chunks[chunk_idx];

        float chunk_ox = (float)(chunk.x * SVO_CHUNK_SIZE);
        float chunk_oy = (float)(chunk.y * SVO_CHUNK_SIZE);
        float chunk_oz = (float)(chunk.z * SVO_CHUNK_SIZE);
        float half = (float)SVO_CHUNK_SIZE * 0.5f;
        float3 center = make_float3(chunk_ox + half, chunk_oy + half, chunk_oz + half);

        uint32_t start_used = g_node_pool_used;
        uint32_t root_idx = build_subtree(center, half, 0,
                                           chunk_ox, chunk_oy, chunk_oz);
        chunk.nodes_offset = root_idx;
        chunk.node_count   = g_node_pool_used - start_used;
        chunk.dirty        = 0;
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

    float3 t1 = make_float3(
        (box_min.x - ray_origin.x) * inv_dir.x,
        (box_min.y - ray_origin.y) * inv_dir.y,
        (box_min.z - ray_origin.z) * inv_dir.z
    );
    float3 t2 = make_float3(
        (box_max.x - ray_origin.x) * inv_dir.x,
        (box_max.y - ray_origin.y) * inv_dir.y,
        (box_max.z - ray_origin.z) * inv_dir.z
    );

    float3 t_min_v = make_float3(fminf_(t1.x, t2.x), fminf_(t1.y, t2.y), fminf_(t1.z, t2.z));
    float3 t_max_v = make_float3(fmaxf_(t1.x, t2.x), fmaxf_(t1.y, t2.y), fmaxf_(t1.z, t2.z));

    t_min = fmaxf_(fmaxf_(t_min_v.x, t_min_v.y), t_min_v.z);
    t_max = fminf_(fminf_(t_max_v.x, t_max_v.y), t_max_v.z);

    if (t_min > t_max || t_max < 0) return 0.0f;

    *t_out = t_min;

    // Calculate normal (simplified)
    float3 hit = float3_add(ray_origin, float3_mul_scalar(ray_dir, t_min));
    float3 center = float3_mul_scalar(float3_add(box_min, box_max), 0.5f);
    float3 local = float3_sub(hit, center);

    float3 abs_local = make_float3(fabsf_(local.x), fabsf_(local.y), fabsf_(local.z));
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
