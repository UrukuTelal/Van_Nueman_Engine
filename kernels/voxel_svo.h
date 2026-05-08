#ifndef VAN_NUEMAN_VOXEL_SVO_H
#define VAN_NUEMAN_VOXEL_SVO_H

#include <cstdint>

#define SVO_CHUNK_SIZE 64
#define SVO_MAX_LODS 8
#define SVO_BASE_SIZE 256
#define MAX_ENTITIES 500000
#define MAX_CHUNKS 10000

struct Voxel_SVO {
    uint8_t material;
    uint8_t r, g, b, a;
    uint16_t lod_level;
};

struct SVO_Node {
    uint32_t children[8];  // 0 = null, else index into node pool
    Voxel_SVO voxel;
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
extern SVO_Node* g_node_pool;
extern uint32_t g_node_pool_size;
extern uint32_t g_node_pool_used;

// Initialize SVO node pool
void svo_init(SVO_Node* node_pool, uint32_t pool_size);

// Allocate a node from pool
uint32_t svo_alloc_node();

// Build SVO from dirty chunks
void svo_build(SVO_Chunk* chunks, uint32_t num_chunks, uint32_t* dirty_voxels);

// Mark chunk dirty for rebuild
void svo_mark_dirty(SVO_Chunk* chunks, uint32_t num_chunks,
                     uint32_t x, uint32_t y, uint32_t z);

// SVO ray intersection
float svo_ray_intersect(SVO_Node* nodes, uint32_t node_idx,
                         float ray_origin[3], float ray_dir[3],
                         float box_min[3], float box_max[3],
                         float* t_out, float normal_out[3]);

// Main entry point
void svo_main(SVO_Node* node_pool, uint32_t pool_size,
              SVO_Chunk* chunks, uint32_t num_chunks,
              uint32_t* dirty_voxels,
              int mode, uint32_t mx, uint32_t my, uint32_t mz);

#endif // VAN_NUEMAN_VOXEL_SVO_H
