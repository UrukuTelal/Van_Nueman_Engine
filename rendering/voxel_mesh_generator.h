#pragma once

#include "../voxel/TruncatedOctahedron.h"
#include "../voxel/VoxelCell.h"
#include "../include/Entity.h"
#include <cstdint>
#include <cmath>
#include <cstring>

constexpr uint32_t VOXEL_MESH_MAX_VERTS = 24;
constexpr uint32_t VOXEL_MESH_MAX_TRIS = 44;
constexpr uint32_t VOXEL_MESH_MAX_INDICES = VOXEL_MESH_MAX_TRIS * 3;

struct VoxelMeshVertex {
    float x, y, z;
    float nx, ny, nz;
    float r, g, b, a;
};

struct VoxelMeshData {
    VoxelMeshVertex vertices[VOXEL_MESH_MAX_VERTS];
    uint32_t indices[VOXEL_MESH_MAX_INDICES];
    uint32_t vertex_count;
    uint32_t index_count;

    VoxelMeshData() : vertex_count(0), index_count(0) {}
};

namespace detail {
inline void compute_normal(float ax, float ay, float az,
                           float bx, float by, float bz,
                           float cx, float cy, float cz,
                           float& nx, float& ny, float& nz) {
    float e1x = bx - ax, e1y = by - ay, e1z = bz - az;
    float e2x = cx - ax, e2y = cy - ay, e2z = cz - az;
    nx = e1y * e2z - e1z * e2y;
    ny = e1z * e2x - e1x * e2z;
    nz = e1x * e2y - e1y * e2x;
    float len = sqrtf(nx * nx + ny * ny + nz * nz);
    if (len > 1e-8f) { nx /= len; ny /= len; nz /= len; }
}

inline float pillar_weight_to_color(vn::fp20_t val) {
    float f = val.to_float();
    if (f < 0.0f) f = 0.0f;
    if (f > 1.0f) f = 1.0f;
    return f;
}
}

inline void generate_voxel_mesh(const TruncatedOctahedron& geom,
                                 const vn::fp20_t face_pillar_weights[TRUNC_OCT_FACES][NumPillars],
                                 VoxelMeshData& out_mesh,
                                 bool use_dome = false) {
    std::memset(&out_mesh, 0, sizeof(out_mesh));

    float vtx_nx[TRUNC_OCT_VERTICES] = {0};
    float vtx_ny[TRUNC_OCT_VERTICES] = {0};
    float vtx_nz[TRUNC_OCT_VERTICES] = {0};
    float vtx_r[TRUNC_OCT_VERTICES] = {0};
    float vtx_g[TRUNC_OCT_VERTICES] = {0};
    float vtx_b[TRUNC_OCT_VERTICES] = {0};
    float vtx_a[TRUNC_OCT_VERTICES] = {0};
    uint32_t vtx_count[TRUNC_OCT_VERTICES] = {0};

    out_mesh.vertex_count = TRUNC_OCT_VERTICES;
    out_mesh.index_count = 0;

    for (uint32_t i = 0; i < TRUNC_OCT_VERTICES; i++) {
        out_mesh.vertices[i].x = geom.vertices[i].x.to_float();
        out_mesh.vertices[i].y = geom.vertices[i].y.to_float();
        out_mesh.vertices[i].z = geom.vertices[i].z.to_float();
    }

    for (uint32_t f = 0; f < TRUNC_OCT_FACES; f++) {
        const TruncOctFace& face = geom.faces[f];
        uint32_t nv = face.vertex_count;

        if (use_dome) {
            float fn[3];
            float ax = geom.vertices[face.vertex_indices[0]].x.to_float();
            float ay = geom.vertices[face.vertex_indices[0]].y.to_float();
            float az = geom.vertices[face.vertex_indices[0]].z.to_float();
            float bx = geom.vertices[face.vertex_indices[1]].x.to_float();
            float by = geom.vertices[face.vertex_indices[1]].y.to_float();
            float bz = geom.vertices[face.vertex_indices[1]].z.to_float();
            float cx = geom.vertices[face.vertex_indices[2]].x.to_float();
            float cy = geom.vertices[face.vertex_indices[2]].y.to_float();
            float cz = geom.vertices[face.vertex_indices[2]].z.to_float();
            detail::compute_normal(ax, ay, az, bx, by, bz, cx, cy, cz, fn[0], fn[1], fn[2]);
            if (fn[1] < -0.01f) continue;
        }

        float face_n[3];
        {
            float ax = geom.vertices[face.vertex_indices[0]].x.to_float();
            float ay = geom.vertices[face.vertex_indices[0]].y.to_float();
            float az = geom.vertices[face.vertex_indices[0]].z.to_float();
            float bx = geom.vertices[face.vertex_indices[1]].x.to_float();
            float by = geom.vertices[face.vertex_indices[1]].y.to_float();
            float bz = geom.vertices[face.vertex_indices[1]].z.to_float();
            float cx = geom.vertices[face.vertex_indices[2]].x.to_float();
            float cy = geom.vertices[face.vertex_indices[2]].y.to_float();
            float cz = geom.vertices[face.vertex_indices[2]].z.to_float();
            detail::compute_normal(ax, ay, az, bx, by, bz, cx, cy, cz, face_n[0], face_n[1], face_n[2]);
        }

        uint32_t tri_count = nv - 2;
        for (uint32_t t = 0; t < tri_count; t++) {
            uint32_t i0 = face.vertex_indices[0];
            uint32_t i1 = face.vertex_indices[t + 1];
            uint32_t i2 = face.vertex_indices[t + 2];

            out_mesh.indices[out_mesh.index_count++] = i0;
            out_mesh.indices[out_mesh.index_count++] = i1;
            out_mesh.indices[out_mesh.index_count++] = i2;

            vtx_nx[i0] += face_n[0]; vtx_ny[i0] += face_n[1]; vtx_nz[i0] += face_n[2];
            vtx_nx[i1] += face_n[0]; vtx_ny[i1] += face_n[1]; vtx_nz[i1] += face_n[2];
            vtx_nx[i2] += face_n[0]; vtx_ny[i2] += face_n[1]; vtx_nz[i2] += face_n[2];
            vtx_count[i0]++; vtx_count[i1]++; vtx_count[i2]++;

            float r = detail::pillar_weight_to_color(face_pillar_weights[f][0]);
            float g = detail::pillar_weight_to_color(face_pillar_weights[f][1]);
            float b = detail::pillar_weight_to_color(face_pillar_weights[f][2]);
            float a = detail::pillar_weight_to_color(face_pillar_weights[f][3]);
            vtx_r[i0] += r; vtx_g[i0] += g; vtx_b[i0] += b; vtx_a[i0] += a;
            vtx_r[i1] += r; vtx_g[i1] += g; vtx_b[i1] += b; vtx_a[i1] += a;
            vtx_r[i2] += r; vtx_g[i2] += g; vtx_b[i2] += b; vtx_a[i2] += a;
        }
    }

    for (uint32_t i = 0; i < TRUNC_OCT_VERTICES; i++) {
        if (vtx_count[i] > 0) {
            float inv = 1.0f / static_cast<float>(vtx_count[i]);
            float nl = sqrtf(vtx_nx[i] * vtx_nx[i] + vtx_ny[i] * vtx_ny[i] + vtx_nz[i] * vtx_nz[i]);
            if (nl > 1e-8f) {
                nl = 1.0f / nl;
                out_mesh.vertices[i].nx = vtx_nx[i] * nl;
                out_mesh.vertices[i].ny = vtx_ny[i] * nl;
                out_mesh.vertices[i].nz = vtx_nz[i] * nl;
            } else {
                out_mesh.vertices[i].nx = 0; out_mesh.vertices[i].ny = 1; out_mesh.vertices[i].nz = 0;
            }
            out_mesh.vertices[i].r = vtx_r[i] * inv;
            out_mesh.vertices[i].g = vtx_g[i] * inv;
            out_mesh.vertices[i].b = vtx_b[i] * inv;
            out_mesh.vertices[i].a = vtx_a[i] * inv;
        }
    }
}

inline void generate_voxel_cell_mesh(const VoxelCell& cell,
                                      VoxelMeshData& out_mesh,
                                      bool use_dome = false) {
    vn::fp20_t face_weights[TRUNC_OCT_FACES][NumPillars];
    for (uint32_t f = 0; f < TRUNC_OCT_FACES; f++) {
        for (uint32_t p = 0; p < NumPillars; p++) {
            face_weights[f][p] = cell.pyramids[f].material_composition[p];
        }
    }
    generate_voxel_mesh(cell.geometry, face_weights, out_mesh, use_dome);
}
