#pragma once

#include <cassert>
#include <cstdio>
#include "../physics/voxel_coupling.h"

inline void test_cell_deformation_creation() {
    printf("Test: Cell deformation creation... ");
    DeformationMap dm;
    uint64_t key = 42;

    CellDeformation& def = get_or_create_deformation(dm, key);
    assert(def.active);
    assert(def.mass == vn::fp20_t(1));
    assert(def.spring_k == vn::fp20_t(10));
    assert(def.damping == vn::fp20_t(2));

    // Same key returns same deformation
    CellDeformation& def2 = get_or_create_deformation(dm, key);
    assert(&def == &def2);
    printf("PASS\n");
}

inline void test_entity_forces_to_cell() {
    printf("Test: Entity forces to cell... ");
    VoxelCell cell;
    BCCCoord bc = {0, 0, 0, 0};
    YieldMatrix mat = YieldMatrix::default_rock();
    cell.init(bc, vn::fp20_t(1.0f), 1, mat);

    CellDeformation def;
    def.active = true;

    Entity entity;
    entity.id = 0;
    entity.native_scale = SCALE_ORGANISM;
    entity.pos_x = vn::fp20_t(0);
    entity.pos_y = vn::fp20_t(0);
    for (int i = 0; i < NumPillars; i++)
        entity.pillars[i] = vn::fp20_t(0);
    entity.pillars[Force] = vn::fp20_t(1.0f);

    apply_entity_forces_to_cell(entity, cell, def);

    // Each vertex should have accumulated force from Force * matrix[FORCE][v]
    bool any_force = false;
    for (uint32_t v = 0; v < TRUNC_OCT_VERTICES; v++) {
        if (def.force[v][0] != vn::fp20_t(0) ||
            def.force[v][1] != vn::fp20_t(0) ||
            def.force[v][2] != vn::fp20_t(0)) {
            any_force = true;
            break;
        }
    }
    assert(any_force);
    printf("PASS\n");
}

inline void test_radial_force() {
    printf("Test: Radial force to cell... ");
    VoxelCell cell;
    BCCCoord bc = {0, 0, 0, 0};
    YieldMatrix mat = YieldMatrix::default_rock();
    cell.init(bc, vn::fp20_t(1.0f), 2, mat);

    CellDeformation def;
    def.active = true;

    // Apply explosion at cell center
    apply_radial_force_to_cell(0.0f, 0.0f, 0.0f, vn::fp20_t(100.0f), 5.0f, cell, def);

    // Some vertices should have radial force
    bool any_force = false;
    for (uint32_t v = 0; v < TRUNC_OCT_VERTICES; v++) {
        float fx = def.force[v][0].to_float();
        float fy = def.force[v][1].to_float();
        float fz = def.force[v][2].to_float();
        float mag = sqrtf(fx * fx + fy * fy + fz * fz);
        if (mag > 0.01f) {
            any_force = true;
            break;
        }
    }
    assert(any_force);
    printf("PASS\n");
}

inline void test_deformation_integration() {
    printf("Test: Deformation Verlet integration... ");
    VoxelCell cell;
    BCCCoord bc = {5, 5, 5, 1};
    YieldMatrix mat = YieldMatrix::default_rock();
    cell.init(bc, vn::fp20_t(1.0f), 3, mat);

    CellDeformation def;
    def.active = true;
    def.mass = vn::fp20_t(1);
    def.spring_k = vn::fp20_t(10);
    def.damping = vn::fp20_t(2);

    // Store rest positions before deformation
    float rest_x[TRUNC_OCT_VERTICES], rest_y[TRUNC_OCT_VERTICES], rest_z[TRUNC_OCT_VERTICES];
    for (uint32_t v = 0; v < TRUNC_OCT_VERTICES; v++) {
        rest_x[v] = cell.geometry.vertices[v].x.to_float();
        rest_y[v] = cell.geometry.vertices[v].y.to_float();
        rest_z[v] = cell.geometry.vertices[v].z.to_float();
    }

    // Apply a small impulse force
    for (uint32_t v = 0; v < TRUNC_OCT_VERTICES; v++) {
        def.force[v][1] = vn::fp20_t(5.0f);  // upward force
    }

    // Run one tick
    tick_cell_deformation(cell, def, vn::fp20_t(0.016f));  // ~60Hz dt

    // After one tick, vertices should have moved
    bool any_moved = false;
    for (uint32_t v = 0; v < TRUNC_OCT_VERTICES; v++) {
        float dx = cell.geometry.vertices[v].x.to_float() - rest_x[v];
        float dy = cell.geometry.vertices[v].y.to_float() - rest_y[v];
        float dz = cell.geometry.vertices[v].z.to_float() - rest_z[v];
        if (fabsf(dx) > 0.0001f || fabsf(dy) > 0.0001f || fabsf(dz) > 0.0001f) {
            any_moved = true;
            break;
        }
    }
    assert(any_moved);

    // Strain accumulator should have increased
    bool any_strain = false;
    for (uint32_t f = 0; f < TRUNC_OCT_FACES; f++) {
        if (cell.pyramids[f].strain_accumulator > vn::fp20_t(0)) {
            any_strain = true;
            break;
        }
    }
    assert(any_strain);

    printf("PASS (vertices displaced, strain accumulated)\n");
}

inline void run_voxel_coupling_tests() {
    printf("=== Voxel Coupling Tests ===\n");
    test_cell_deformation_creation();
    test_entity_forces_to_cell();
    test_radial_force();
    test_deformation_integration();
    printf("=== All Voxel Coupling Tests PASSED ===\n\n");
}
