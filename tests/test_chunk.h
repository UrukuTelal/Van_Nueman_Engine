// test_chunk.h - Unit tests for Chunk system
#pragma once

#include <cassert>
#include <cstdio>
#include "../scene/chunk.h"

inline void test_chunk_creation() {
    printf("Test: Chunk creation... ");
    Chunk chunk(0, 0, 0);
    // Chunk should be created with valid coordinates
    assert(chunk.get_chunk_x() == 0);
    assert(chunk.get_chunk_y() == 0);
    assert(chunk.get_chunk_z() == 0);
    printf("PASS (chunk 0,0,0)\n");
}

inline void test_voxel_access() {
    printf("Test: Voxel access... ");
    Chunk chunk(1, 2, 3);
    
    // Test valid positions
    // Use get_voxel which is public
    const Voxel& v = chunk.get_voxel(0, 0, 0);
    // Just accessing shouldn't crash
    (void)v;  // Suppress unused variable warning
    
    // Test that we can set and get at valid positions
    Voxel voxel;
    voxel.material = 1;
    chunk.set_voxel(0, 0, 0, voxel);
    
    printf("PASS\n");
}

inline void test_voxel_set_and_get() {
    printf("Test: Voxel set and get... ");
    Chunk chunk(0, 0, 0);
    
    Voxel voxel;
    voxel.material = 1;
    voxel.r = 255;
    voxel.g = 128;
    voxel.b = 64;
    voxel.a = 255;
    
    chunk.set_voxel(1, 2, 3, voxel);
    const Voxel& retrieved = chunk.get_voxel(1, 2, 3);
    
    assert(retrieved.material == 1);
    assert(retrieved.r == 255);
    assert(retrieved.g == 128);
    printf("PASS (material=%d, r=%d, g=%d)\n", retrieved.material, retrieved.r, retrieved.g);
}

inline void test_invalid_position_handling() {
    printf("Test: Invalid position handling... ");
    Chunk chunk(0, 0, 0);
    
    Voxel voxel;
    voxel.material = 1;
    
    // These should not crash, just return early (set_voxel checks bounds)
    chunk.set_voxel(-1, 0, 0, voxel);
    chunk.set_voxel(CHUNK_SIZE, 0, 0, voxel);
    
    // For get_voxel, we can only test valid positions since it returns a reference
    // and likely asserts on invalid positions
    
    printf("PASS (no crash)\n");
}

inline void test_chunk_coordinates() {
    printf("Test: Chunk coordinates... ");
    Chunk chunk(-5, 10, -3);
    
    assert(chunk.get_chunk_x() == -5);
    assert(chunk.get_chunk_y() == 10);
    assert(chunk.get_chunk_z() == -3);
    printf("PASS\n");
}

inline void test_voxel_valid_pos() {
    printf("Test: Voxel valid position check... ");
    // is_valid_pos is static and should work correctly
    assert(Chunk::is_valid_pos(0, 0, 0) == true);
    assert(Chunk::is_valid_pos(CHUNK_SIZE-1, CHUNK_SIZE-1, CHUNK_SIZE-1) == true);
    assert(Chunk::is_valid_pos(-1, 0, 0) == false);
    assert(Chunk::is_valid_pos(CHUNK_SIZE, 0, 0) == false);
    printf("PASS\n");
}

// ── Active Cell (Truncated Octahedron) Tests ─────────────────

inline void test_active_cell_promotion() {
    printf("Test: Active cell promote/get... ");
    Chunk chunk(0, 0, 0);
    YieldMatrix mat = YieldMatrix::default_rock();

    VoxelCell* cell = chunk.promote_to_active(10, 20, 30, mat);
    assert(cell != nullptr);
    assert(cell->active);
    assert(cell->coord.i == 10);
    assert(cell->coord.j == 20);
    assert(cell->coord.k == 30);
    assert(cell->coord.l == ((10 + 20 + 30) & 1));

    uint64_t key = chunk.bcc_key_for(10, 20, 30);
    VoxelCell* retrieved = chunk.get_active_cell(key);
    assert(retrieved == cell);
    assert(chunk.active_cell_count() == 1);

    const auto& cells = chunk.get_active_cells();
    assert(cells.size() == 1);
    assert(cells.count(key) == 1);

    printf("PASS (cell coords %d,%d,%d l=%d id=%u)\n",
           cell->coord.i, cell->coord.j, cell->coord.k, cell->coord.l, cell->cell_id);
}

inline void test_active_cell_idempotent_promote() {
    printf("Test: Active cell idempotent promote... ");
    Chunk chunk(0, 0, 0);
    YieldMatrix mat = YieldMatrix::default_rock();

    VoxelCell* first = chunk.promote_to_active(5, 5, 5, mat);
    VoxelCell* second = chunk.promote_to_active(5, 5, 5, mat);
    assert(first == second);
    assert(chunk.active_cell_count() == 1);
    printf("PASS\n");
}

inline void test_active_cell_remove() {
    printf("Test: Active cell remove... ");
    Chunk chunk(0, 0, 0);
    YieldMatrix mat = YieldMatrix::default_rock();

    chunk.promote_to_active(1, 1, 1, mat);
    chunk.promote_to_active(2, 2, 2, mat);
    assert(chunk.active_cell_count() == 2);

    uint64_t key = chunk.bcc_key_for(1, 1, 1);
    chunk.remove_active_cell(key);
    assert(chunk.active_cell_count() == 1);
    assert(chunk.get_active_cell(key) == nullptr);

    // Remaining cell still accessible
    uint64_t key2 = chunk.bcc_key_for(2, 2, 2);
    assert(chunk.get_active_cell(key2) != nullptr);
    printf("PASS\n");
}

inline void test_bcc_key_consistency() {
    printf("Test: BCC key consistency... ");
    Chunk chunk(0, 0, 0);
    YieldMatrix mat = YieldMatrix::default_rock();

    // bcc_key_for should produce the same key for the same coordinates
    uint64_t k1 = chunk.bcc_key_for(30, 40, 50);
    uint64_t k2 = chunk.bcc_key_for(30, 40, 50);
    assert(k1 == k2);

    // And the key should match what promote_to_active stores internally
    chunk.promote_to_active(30, 40, 50, mat);
    assert(chunk.get_active_cell(k1) != nullptr);
    printf("PASS\n");
}

inline void test_bcc_l_bit_correctness() {
    printf("Test: BCC l-bit correctness... ");
    // l = (x + y + z) & 1 for BCC lattice parity
    BCCCoord c0 = bcc_from_cartesian(0, 0, 0);
    assert(c0.l == 0);

    BCCCoord c1 = bcc_from_cartesian(1, 1, 1);
    assert(c1.l == 1);  // (1+1+1) & 1 = 1

    BCCCoord c2 = bcc_from_cartesian(2, 0, 0);
    assert(c2.l == 0);  // (2+0+0) & 1 = 0

    BCCCoord c3 = bcc_from_cartesian(3, 5, 7);
    assert(c3.l == ((3 + 5 + 7) & 1));  // = 15 & 1 = 1
    printf("PASS\n");
}

inline void run_chunk_tests() {
    printf("=== Chunk Tests ===\n");
    test_chunk_creation();
    test_voxel_access();
    test_voxel_set_and_get();
    test_invalid_position_handling();
    test_chunk_coordinates();
    test_voxel_valid_pos();
    test_active_cell_promotion();
    test_active_cell_idempotent_promote();
    test_active_cell_remove();
    test_bcc_key_consistency();
    test_bcc_l_bit_correctness();
    printf("=== All Chunk Tests PASSED ===\n\n");
}
