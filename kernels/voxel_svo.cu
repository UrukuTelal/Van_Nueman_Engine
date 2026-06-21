#include "voxel_shared.cuh"

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

// ---------------------------------------------------------------------------
// Procedural density evaluation (CUDA device functions)
// ---------------------------------------------------------------------------

// Evaluate density at a single world-space point.
// Returns: -1 = empty, 0 = MAT_OCTAHEDRON, 1 = MAT_CUBE, 2 = MAT_POLYHEDRA
__device__ int eval_density(float px, float py, float pz,
                             float chunk_ox, float chunk_oy, float chunk_oz) {
    float lx = px - chunk_ox;
    float ly = py - chunk_oy;
    float lz = pz - chunk_oz;
    float half = (float)SVO_CHUNK_SIZE * 0.5f;
    float3 local = make_float3(lx - half, ly - half, lz - half);

    float sphere_dist = sqrtf(local.x * local.x + local.y * local.y + local.z * local.z);
    float sphere_radius = half * 0.35f;
    float oct_dist = fabsf(local.x) + fabsf(local.y) + fabsf(local.z);
    float oct_size = half * 0.65f;

    if (sphere_dist < sphere_radius)
        return MAT_OCTAHEDRON;
    if (oct_dist < oct_size && sphere_dist < half * 0.9f)
        return MAT_CUBE;
    return -1;
}

// Classify a sub-volume: -1 empty, -2 mixed, 0-2 uniform material
__device__ int eval_density_volume(float3 center, float half_size,
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
            return -2;
    }

    int cm = eval_density(center.x, center.y, center.z,
                          chunk_ox, chunk_oy, chunk_oz);
    if (cm < 0)
        has_empty = 1;
    else if (first_mat < 0)
        first_mat = cm;
    else if (first_mat != cm)
        return -2;

    if (first_mat < 0) return -1;
    if (has_empty)     return -2;
    return first_mat;
}

// Recursively build the octree for a sub-volume (device-side).
// Returns: node pool index of the sub-tree root (0 = empty).
// node_count is incremented for each node allocated in this subtree.
__device__ uint32_t build_subtree(float3 center, float half_size, int depth,
                                   float chunk_ox, float chunk_oy, float chunk_oz,
                                   uint32_t& node_count) {
    int dv = eval_density_volume(center, half_size, chunk_ox, chunk_oy, chunk_oz);
    if (dv < 0 && dv != -2) return 0;          // fully empty
    if (dv >= 0 || depth >= SVO_MAX_LODS) {    // uniform solid or at leaf depth
        uint32_t idx = svo_alloc_node();
        if (idx == 0) return 0;
        node_count++;
        uint8_t mat = (uint8_t)((dv >= 0) ? dv : MAT_POLYHEDRA);
        d_node_pool[idx].voxel.material = mat;
        d_node_pool[idx].voxel.r = mat == MAT_OCTAHEDRON ? 200
                                 : mat == MAT_CUBE       ? 100 : 150;
        d_node_pool[idx].voxel.g = mat == MAT_OCTAHEDRON ? 100
                                 : mat == MAT_CUBE       ? 200 : 100;
        d_node_pool[idx].voxel.b = mat == MAT_OCTAHEDRON ? 100
                                 : mat == MAT_CUBE       ? 100 : 200;
        d_node_pool[idx].voxel.a = 255;
        d_node_pool[idx].voxel.lod_level = (uint16_t)depth;
        return idx;
    }

    // dv == -2 (mixed): subdivide into up to 8 children.
    uint32_t node_idx = svo_alloc_node();
    if (node_idx == 0) return 0;
    node_count++;
    SVO_Node* node = &d_node_pool[node_idx];
    node->valid_mask = 0;

    float child_half = half_size * 0.5f;
    for (int octant = 0; octant < 8; octant++) {
        float ox = (octant & 1) ? child_half : -child_half;
        float oy = (octant & 2) ? child_half : -child_half;
        float oz = (octant & 4) ? child_half : -child_half;
        float3 cc = make_float3(center.x + ox, center.y + oy, center.z + oz);

        uint32_t ci = build_subtree(cc, child_half, depth + 1,
                                     chunk_ox, chunk_oy, chunk_oz, node_count);
        if (ci != 0) {
            node->children[octant] = ci;
            node->valid_mask |= (uint8_t)(1 << octant);
        }
    }

    return node_idx;
}

// Build SVO from dirty chunks — one block per chunk, thread 0 builds the tree.
__global__ void svo_build_kernel(SVO_Chunk* chunks, uint32_t num_chunks, uint32_t* dirty_voxels) {
    uint32_t chunk_idx = blockIdx.x;
    if (chunk_idx >= num_chunks) return;
    if (!chunks[chunk_idx].dirty) return;

    if (threadIdx.x == 0) {
        SVO_Chunk& chunk = chunks[chunk_idx];

        float chunk_ox = (float)(chunk.x * SVO_CHUNK_SIZE);
        float chunk_oy = (float)(chunk.y * SVO_CHUNK_SIZE);
        float chunk_oz = (float)(chunk.z * SVO_CHUNK_SIZE);
        float half = (float)SVO_CHUNK_SIZE * 0.5f;
        float3 center = make_float3(chunk_ox + half, chunk_oy + half, chunk_oz + half);

        uint32_t local_count = 0;
        uint32_t root_idx = build_subtree(center, half, 0,
                                           chunk_ox, chunk_oy, chunk_oz,
                                           local_count);
        chunk.nodes_offset = root_idx;
        chunk.node_count   = local_count;
        chunk.dirty        = 0;
    }
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
