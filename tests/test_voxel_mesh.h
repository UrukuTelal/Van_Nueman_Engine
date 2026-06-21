#pragma once

#include <cassert>
#include <cstdio>
#include "../rendering/voxel_mesh_generator.h"

inline void test_voxel_mesh_generate() {
    printf("Test: Voxel mesh generate... ");
    TruncatedOctahedron geom;
    init_truncated_octahedron(geom, vn::fp20_t(1.0f));
    VoxelMeshData mesh;

    vn::fp20_t face_weights[TRUNC_OCT_FACES][NumPillars];
    for (uint32_t f = 0; f < TRUNC_OCT_FACES; f++)
        for (uint32_t p = 0; p < NumPillars; p++)
            face_weights[f][p] = vn::fp20_t(0.5f);

    generate_voxel_mesh(geom, face_weights, mesh, false);

    // Should produce 24 vertices and up to 132 indices (44 triangles × 3)
    assert(mesh.vertex_count == 24);
    assert(mesh.index_count > 0);
    assert(mesh.index_count <= VOXEL_MESH_MAX_INDICES);

    // Each vertex should have valid position and normal
    for (uint32_t i = 0; i < mesh.vertex_count; i++) {
        float len = sqrtf(mesh.vertices[i].nx * mesh.vertices[i].nx +
                          mesh.vertices[i].ny * mesh.vertices[i].ny +
                          mesh.vertices[i].nz * mesh.vertices[i].nz);
        assert(len > 0.5f);  // normal should be unit-ish
    }

    printf("PASS (%u verts, %u indices, %u tris)\n",
           mesh.vertex_count, mesh.index_count, mesh.index_count / 3);
}

inline void test_voxel_mesh_dome() {
    printf("Test: Voxel mesh dome mode... ");
    TruncatedOctahedron geom;
    init_truncated_octahedron(geom, vn::fp20_t(1.0f));
    VoxelMeshData full, dome;

    vn::fp20_t face_weights[TRUNC_OCT_FACES][NumPillars];
    for (uint32_t f = 0; f < TRUNC_OCT_FACES; f++)
        for (uint32_t p = 0; p < NumPillars; p++)
            face_weights[f][p] = vn::fp20_t(0.5f);

    generate_voxel_mesh(geom, face_weights, full, false);
    generate_voxel_mesh(geom, face_weights, dome, true);

    // Dome mode should have fewer indices (only faces with upward normals)
    assert(dome.index_count < full.index_count);
    assert(dome.vertex_count == 24);
    printf("PASS (full=%u tris, dome=%u tris)\n", full.index_count / 3, dome.index_count / 3);
}

inline void test_voxel_cell_mesh() {
    printf("Test: Voxel cell mesh generation... ");
    VoxelCell cell;
    BCCCoord bc = {0, 0, 0, 0};
    YieldMatrix mat = YieldMatrix::default_rock();
    cell.init(bc, vn::fp20_t(1.0f), 42, mat);

    VoxelMeshData mesh;
    generate_voxel_cell_mesh(cell, mesh, false);

    assert(mesh.vertex_count == 24);
    assert(mesh.index_count > 0);
    assert(mesh.index_count % 3 == 0);

    // Vertex colors should reflect the pyramid material composition
    bool has_color = false;
    for (uint32_t i = 0; i < mesh.vertex_count; i++) {
        if (mesh.vertices[i].r > 0.01f || mesh.vertices[i].g > 0.01f || mesh.vertices[i].b > 0.01f) {
            has_color = true;
            break;
        }
    }
    assert(has_color);

    printf("PASS (cell_id=%u, %u tris, colors=%s)\n",
           cell.cell_id, mesh.index_count / 3, has_color ? "yes" : "no");
}

inline void run_voxel_mesh_tests() {
    printf("=== Voxel Mesh Generator Tests ===\n");
    test_voxel_mesh_generate();
    test_voxel_mesh_dome();
    test_voxel_cell_mesh();
    printf("=== All Voxel Mesh Generator Tests PASSED ===\n\n");
}
