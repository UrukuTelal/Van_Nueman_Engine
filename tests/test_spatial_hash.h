// test_spatial_hash.h - Unit tests for Spatial Hash implementation
#pragma once

#include <cassert>
#include <cstdio>
#include <cmath>
#include <unordered_map>
#include <vector>

// Replicate the spatial hash logic from tick_loop.cpp for testing
// This avoids including tick_loop.h which has type conflicts
constexpr float SPATIAL_HASH_CELL_SIZE_TEST = 15.0f;
constexpr float INTERACTION_RADIUS_TEST = 10.0f;

inline uint64_t test_hash_cell(int cx, int cy, int cz) {
    return ((static_cast<uint64_t>(cx) & 0xFFFFF) << 40) |
           ((static_cast<uint64_t>(cy) & 0xFFFFF) << 20) |
           (static_cast<uint64_t>(cz) & 0xFFFFF);
}

inline void test_get_cell_coords(float x, float y, float z, int& cx, int& cy, int& cz) {
    cx = static_cast<int>(std::floor(x / SPATIAL_HASH_CELL_SIZE_TEST));
    cy = static_cast<int>(std::floor(y / SPATIAL_HASH_CELL_SIZE_TEST));
    cz = static_cast<int>(std::floor(z / SPATIAL_HASH_CELL_SIZE_TEST));
}

inline void test_spatial_hash_cell_coords() {
    printf("Test: Spatial hash cell coords... ");
    int cx, cy, cz;
    
    // Test basic coordinate conversion
    test_get_cell_coords(0.0f, 0.0f, 0.0f, cx, cy, cz);
    assert(cx == 0 && cy == 0 && cz == 0);
    
    test_get_cell_coords(14.9f, 14.9f, 14.9f, cx, cy, cz);
    assert(cx == 0 && cy == 0 && cz == 0);  // Still in cell 0
    
    test_get_cell_coords(15.0f, 15.0f, 15.0f, cx, cy, cz);
    assert(cx == 1 && cy == 1 && cz == 1);  // Now in cell 1
    
    test_get_cell_coords(-1.0f, -1.0f, -1.0f, cx, cy, cz);
    assert(cx == -1 && cy == -1 && cz == -1);  // Negative coords
    
    printf("PASS\n");
}

inline void test_spatial_hash_key_generation() {
    printf("Test: Spatial hash key generation... ");
    uint64_t key1 = test_hash_cell(0, 0, 0);
    uint64_t key2 = test_hash_cell(1, 0, 0);
    uint64_t key3 = test_hash_cell(0, 1, 0);
    uint64_t key4 = test_hash_cell(0, 0, 1);
    
    assert(key1 != key2);
    assert(key1 != key3);
    assert(key1 != key4);
    assert(key2 != key3);
    
    // Same cell should give same key
    uint64_t key1_dup = test_hash_cell(0, 0, 0);
    assert(key1 == key1_dup);
    
    printf("PASS\n");
}

inline void test_spatial_hash_collision_detection() {
    printf("Test: Spatial hash collision detection... ");
    // Test that different cells produce different keys
    std::vector<uint64_t> keys(1000);
    int unique_count = 0;
    
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            for (int k = 0; k < 10; k++) {
                keys[i*100 + j*10 + k] = test_hash_cell(i, j, k);
            }
        }
    }
    
    // Count unique keys (should be 1000)
    for (int i = 0; i < 1000; i++) {
        bool unique = true;
        for (int j = 0; j < i; j++) {
            if (keys[i] == keys[j]) {
                unique = false;
                break;
            }
        }
        if (unique) unique_count++;
    }
    
    assert(unique_count == 1000);  // All keys should be unique
    printf("PASS (1000 unique keys)\n");
}

inline void test_spatial_hash_nearby_radius() {
    printf("Test: Spatial hash nearby radius calculation... ");
    
    // Test that cells within interaction radius are calculated correctly
    int radius_in_cells = static_cast<int>(std::ceil(INTERACTION_RADIUS_TEST / SPATIAL_HASH_CELL_SIZE_TEST));
    assert(radius_in_cells == 1);  // 10/15 rounded up = 1
    
    // Test cell coordinate calculation for nearby cells
    int center_cx, center_cy, center_cz;
    test_get_cell_coords(15.0f, 15.0f, 15.0f, center_cx, center_cy, center_cz);
    
    // Should check cells from (center_cx - radius) to (center_cx + radius)
    for (int dx = -radius_in_cells; dx <= radius_in_cells; dx++) {
        for (int dy = -radius_in_cells; dy <= radius_in_cells; dy++) {
            for (int dz = -radius_in_cells; dz <= radius_in_cells; dz++) {
                int cx = center_cx + dx;
                int cy = center_cy + dy;
                int cz = center_cz + dz;
                uint64_t key = test_hash_cell(cx, cy, cz);
                assert(key != 0 || (cx == 0 && cy == 0 && cz == 0));  // key could be 0 for (0,0,0)
            }
        }
    }
    
    printf("PASS (radius=%d cells)\n", radius_in_cells);
}

inline void run_spatial_hash_tests() {
    printf("=== Spatial Hash Tests ===\n");
    test_spatial_hash_cell_coords();
    test_spatial_hash_key_generation();
    test_spatial_hash_collision_detection();
    test_spatial_hash_nearby_radius();
    printf("=== All Spatial Hash Tests PASSED ===\n\n");
}
