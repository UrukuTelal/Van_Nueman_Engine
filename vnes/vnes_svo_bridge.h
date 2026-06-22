#pragma once

#include "lattice.h"
#include "../kernels/voxel_svo.h"
#include <algorithm>
#include <cstring>
#include <vector>

// ── VNES ↔ SVO coordinate conversion ─────────────────────
// SVO world space is a 16384³ voxel grid (256 chunks × 64 voxels/chunk).
// VNES cell centers are at BCC lattice positions; each cell has a
// circumscribed sphere radius of VNES_CELL_RADIUS_MM (=125 mm).
//
// Scale factor: 1 SVO voxel unit = VNES_SVO_SCALE_MM.
// Default 2 mm/voxel → 32.8 m world extent, matching the SVO base size.

inline constexpr float VNES_SVO_SCALE_MM = 2.0f;

// ── Material constants for VNES cells in SVO ─────────────
// Materials 0-9 reserved for other game systems.
inline constexpr uint8_t VNES_MAT_A = 10;
inline constexpr uint8_t VNES_MAT_B = 11;
inline constexpr uint8_t VNES_MAT_C = 12;
inline constexpr uint8_t VNES_MAT_D = 13;
inline constexpr uint8_t VNES_MAT_E = 14;

// ── Convert LatticeCoord → SVO world voxel coordinates ───
inline void vnes_to_svo_world(LatticeCoord coord,
                              int& sx, int& sy, int& sz,
                              float scale_mm = VNES_SVO_SCALE_MM)
{
    float wx, wy, wz;
    coord.to_world(wx, wy, wz);
    float inv = 1.0f / scale_mm;
    sx = (int)(wx * inv);
    sy = (int)(wy * inv);
    sz = (int)(wz * inv);
}

// ── Convert LatticeCoord → SVO chunk + local coords ──────
inline void vnes_to_svo_chunk(LatticeCoord coord,
                              uint32_t& cx, uint32_t& cy, uint32_t& cz,
                              uint32_t& lx, uint32_t& ly, uint32_t& lz,
                              float scale_mm = VNES_SVO_SCALE_MM)
{
    int sx, sy, sz;
    vnes_to_svo_world(coord, sx, sy, sz, scale_mm);
    cx = (uint32_t)(sx >> 6);
    cy = (uint32_t)(sy >> 6);
    cz = (uint32_t)(sz >> 6);
    lx = (uint32_t)(sx & 63);
    ly = (uint32_t)(sy & 63);
    lz = (uint32_t)(sz & 63);
}

// ── Convert LatticeCell to SVO voxel payload ─────────────
// Encodes the variant's metadata into material + color.
//   material = 10-14 (one per variant type)
//   r        = load_bearing flag (200/100)
//   g        = compressive_priority (0-255)
//   b        = fluid_priority (0-255)
//   a        = bio_support bitmask
//   lod_level= 0 (leaf)

inline Voxel_SVO vnes_cell_to_voxel(const LatticeCell& cell)
{
    Voxel_SVO v = {};
    if (!cell.metadata) return v;

    auto id = cell.metadata->class_id.id;
    if      (std::memcmp(id, "VNE-A-250", 9) == 0) v.material = VNES_MAT_A;
    else if (std::memcmp(id, "VNE-B-250", 9) == 0) v.material = VNES_MAT_B;
    else if (std::memcmp(id, "VNE-C-250", 9) == 0) v.material = VNES_MAT_C;
    else if (std::memcmp(id, "VNE-D-250", 9) == 0) v.material = VNES_MAT_D;
    else if (std::memcmp(id, "VNE-E-250", 9) == 0) v.material = VNES_MAT_E;
    else v.material = VNES_MAT_A;

    v.r = cell.metadata->load_bearing ? 200 : 100;
    v.g = (uint8_t)(cell.metadata->compressive_priority * 255.0f);
    v.b = (uint8_t)(cell.metadata->fluid_priority * 255.0f);
    v.a = (uint8_t)cell.metadata->bio_support;
    v.lod_level = 0;
    return v;
}

// ── SVO build state ───────────────────────────────────────
// Wraps the SVO node pool for VNES population.

struct VNES_SVOState {
    SVO_Node* node_pool;
    uint32_t pool_capacity;
    uint32_t pool_used;

    void init(SVO_Node* pool, uint32_t capacity) {
        node_pool = pool;
        pool_capacity = capacity;
        pool_used = 0;
        if (capacity > 0) {
            SVO_Node& root = pool[0];
            std::memset(&root, 0, sizeof(SVO_Node));
            pool_used = 1;
        }
    }

    uint32_t alloc() {
        if (pool_used >= pool_capacity) return 0;
        uint32_t idx = pool_used++;
        std::memset(&node_pool[idx], 0, sizeof(SVO_Node));
        return idx;
    }

    bool has_space_for(uint32_t additional) const {
        return pool_used + additional <= pool_capacity;
    }
};

// ── Insert a single VNES cell as an SVO leaf node ────────
// Returns the node index, or 0 on failure.
inline uint32_t vnes_insert_cell(VNES_SVOState& state,
                                 const LatticeCell& cell,
                                 float scale_mm = VNES_SVO_SCALE_MM)
{
    if (!state.has_space_for(8 + 1)) return 0;

    int sx, sy, sz;
    vnes_to_svo_world(cell.coord, sx, sy, sz, scale_mm);

    // Normalize to positive range (SVO coords are unsigned)
    // Shift by half the SVO base extent so the origin is centered.
    uint32_t half = (SVO_BASE_SIZE * SVO_CHUNK_SIZE) / 2;
    uint32_t ux = (uint32_t)(sx + (int)half);
    uint32_t uy = (uint32_t)(sy + (int)half);
    uint32_t uz = (uint32_t)(sz + (int)half);

    // Build leaf-to-root chain
    // We traverse from fine to coarse, creating any missing parent nodes.
    // Since we're inserting sparsely, we start from the root (index 0)
    // and descend, creating children as needed.

    uint32_t depth = SVO_MAX_LODS - 1;

    // Descend from root, creating the path to the leaf
    // At each level, compute which child index (0-7) the position falls in
    uint32_t level_size = SVO_BASE_SIZE * SVO_CHUNK_SIZE;  // 16384 at root
    uint32_t node_idx = 0;  // start at root

    for (uint32_t lod = 0; lod < depth; lod++) {
        // Which child at this level?
        uint32_t child_size = level_size >> 1;
        uint32_t child_bit = 0;
        if (ux & child_size) child_bit |= 1;  // X bit
        if (uy & child_size) child_bit |= 2;  // Y bit
        if (uz & child_size) child_bit |= 4;  // Z bit

        SVO_Node& parent = state.node_pool[node_idx];

        // Create child if it doesn't exist
        if (parent.children[child_bit] == 0) {
            uint32_t child_idx = state.alloc();
            if (child_idx == 0) return 0;
            parent.children[child_bit] = child_idx;
            parent.valid_mask |= (1u << child_bit);
        }

        node_idx = parent.children[child_bit];
        level_size = child_size;
    }

    // Set the leaf node's voxel data
    state.node_pool[node_idx].voxel = vnes_cell_to_voxel(cell);
    return node_idx;
}

// ── Convert entire lattice to SVO ─────────────────────────
// Returns the number of leaf nodes inserted.
inline uint32_t vnes_lattice_to_svo(VNES_SVOState& state,
                                    const Lattice& lattice,
                                    float scale_mm = VNES_SVO_SCALE_MM)
{
    // Reserve enough pool space: each cell needs up to SVO_MAX_LODS nodes
    uint32_t needed = (uint32_t)lattice.size() * SVO_MAX_LODS + 1;
    if (!state.has_space_for(needed)) {
        uint32_t room = state.pool_capacity - state.pool_used;
        // Only insert as many as we have room for
    }

    uint32_t count = 0;
    for (const auto& [coord, cell] : lattice.cells()) {
        (void)coord;
        uint32_t idx = vnes_insert_cell(state, cell, scale_mm);
        if (idx > 0) count++;
    }
    return count;
}

// ── Chunk descriptor creation ─────────────────────────────
// Scans the VNES lattice and creates SVO_Chunk descriptors
// for all occupied chunks. Returns the number of chunks written.

inline uint32_t vnes_create_chunk_descs(const Lattice& lattice,
                                         SVO_Chunk* chunks_out,
                                         uint32_t max_chunks,
                                         float scale_mm = VNES_SVO_SCALE_MM)
{
    uint32_t count = 0;
    uint64_t last_key = 0;

    for (const auto& [coord, cell] : lattice.cells()) {
        uint32_t cx, cy, cz, lx, ly, lz;
        vnes_to_svo_chunk(coord, cx, cy, cz, lx, ly, lz, scale_mm);

        // Simple dedup: pack chunk coords into a key
        uint64_t key = ((uint64_t)cx << 42) | ((uint64_t)cy << 21) | (uint64_t)cz;
        if (key == last_key && count > 0) continue;

        if (count < max_chunks) {
            SVO_Chunk& ch = chunks_out[count];
            ch.x = cx;
            ch.y = cy;
            ch.z = cz;
            ch.lod_level = 0;
            ch.node_count = 0;
            ch.dirty = 0;
            ch.nodes_offset = 0;
            count++;
            last_key = key;
        }
    }
    return count;
}

// ── Voxel grid filling (alternative approach) ─────────────
// Fills a 3D voxel grid with VNES cell data. Each VNES cell
// places its voxel data at the cell-center position in the grid.

inline void vnes_fill_voxel_grid(const Lattice& lattice,
                                  Voxel_SVO* grid,
                                  uint32_t dim_x, uint32_t dim_y, uint32_t dim_z,
                                  float origin_x, float origin_y, float origin_z,
                                  float voxel_size_mm)
{
    float inv_size = 1.0f / voxel_size_mm;

    for (const auto& [coord, cell] : lattice.cells()) {
        float wx, wy, wz;
        coord.to_world(wx, wy, wz);

        int gx = (int)((wx - origin_x) * inv_size);
        int gy = (int)((wy - origin_y) * inv_size);
        int gz = (int)((wz - origin_z) * inv_size);

        if (gx < 0 || (uint32_t)gx >= dim_x) continue;
        if (gy < 0 || (uint32_t)gy >= dim_y) continue;
        if (gz < 0 || (uint32_t)gz >= dim_z) continue;

        Voxel_SVO v = vnes_cell_to_voxel(cell);
        grid[(uint32_t)gz * dim_y * dim_x + (uint32_t)gy * dim_x + (uint32_t)gx] = v;
    }
}
