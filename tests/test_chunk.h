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

inline void run_chunk_tests() {
    printf("=== Chunk Tests ===\n");
    test_chunk_creation();
    test_voxel_access();
    test_voxel_set_and_get();
    test_invalid_position_handling();
    test_chunk_coordinates();
    test_voxel_valid_pos();
    printf("=== All Chunk Tests PASSED ===\n\n");
}
