#pragma once

#include "sdf_primitives.h"
#include "variant_registry.h"
#include "variant_sdf.h"
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <vector>

// ── Dynamic mesh type ─────────────────────────────────────
// Holds vertex positions and triangle indices. Grows as
// needed during Marching Cubes extraction.

struct VNESMesh {
    std::vector<vnes_vec3> vertices;
    std::vector<vnes_vec3> normals;
    std::vector<uint32_t> indices;  // 3 per triangle

    void clear() {
        vertices.clear();
        normals.clear();
        indices.clear();
    }

    uint32_t add_vertex(vnes_vec3 v) {
        uint32_t idx = (uint32_t)vertices.size();
        vertices.push_back(v);
        normals.push_back({0,0,0});
        return idx;
    }

    void add_triangle(uint32_t i0, uint32_t i1, uint32_t i2) {
        indices.push_back(i0);
        indices.push_back(i1);
        indices.push_back(i2);
    }

    uint32_t triangle_count() const {
        return (uint32_t)indices.size() / 3;
    }
};

// ── Grid3D: uniform SDF sample grid ───────────────────────
// Stores signed distance values at each grid point.

struct VNESGrid3D {
    uint32_t nx, ny, nz;
    float origin_x, origin_y, origin_z;
    float spacing;
    std::vector<float> values;

    VNESGrid3D() : nx(0), ny(0), nz(0), origin_x(0), origin_y(0), origin_z(0), spacing(1.0f) {}

    VNESGrid3D(uint32_t nx_, uint32_t ny_, uint32_t nz_,
                float ox, float oy, float oz, float sp)
        : nx(nx_), ny(ny_), nz(nz_),
          origin_x(ox), origin_y(oy), origin_z(oz),
          spacing(sp)
    {
        values.resize(nx * ny * nz, 0.0f);
    }

    float& at(uint32_t ix, uint32_t iy, uint32_t iz) {
        return values[(iz * ny + iy) * nx + ix];
    }

    float at(uint32_t ix, uint32_t iy, uint32_t iz) const {
        return values[(iz * ny + iy) * nx + ix];
    }

    vnes_vec3 grid_to_world(uint32_t ix, uint32_t iy, uint32_t iz) const {
        return {
            origin_x + ix * spacing,
            origin_y + iy * spacing,
            origin_z + iz * spacing
        };
    }
};

// ── Sample SDF onto grid ──────────────────────────────────

inline void sample_sdf_to_grid(VNESGrid3D& grid,
                                float (*sdf_fn)(vnes_vec3)) {
    for (uint32_t iz = 0; iz < grid.nz; iz++) {
        for (uint32_t iy = 0; iy < grid.ny; iy++) {
            for (uint32_t ix = 0; ix < grid.nx; ix++) {
                vnes_vec3 p = grid.grid_to_world(ix, iy, iz);
                grid.at(ix, iy, iz) = sdf_fn(p);
            }
        }
    }
}

template<typename F>
inline void sample_sdf_to_grid(VNESGrid3D& grid, F sdf_fn) {
    for (uint32_t iz = 0; iz < grid.nz; iz++) {
        for (uint32_t iy = 0; iy < grid.ny; iy++) {
            for (uint32_t ix = 0; ix < grid.nx; ix++) {
                vnes_vec3 p = grid.grid_to_world(ix, iy, iz);
                grid.at(ix, iy, iz) = sdf_fn(p);
            }
        }
    }
}

// ── Marching cubes lookup tables ──────────────────────────
// Tables defined in vnes_library.cpp to avoid MSVC initializer limits.

namespace mc {

extern const uint16_t edge_table[256];
const int8_t* tri_table(); // returns pointer to flat 1D table [4096]
extern const uint8_t edge_verts[12][2];

} // namespace mc

// ── Marching Cubes main function ──────────────────────────
// Extracts the zero level set from the SDF grid into a mesh.

inline void marching_cubes(const VNESGrid3D& grid, VNESMesh& mesh) {
    mesh.clear();

    for (uint32_t iz = 0; iz < grid.nz - 1; iz++) {
        for (uint32_t iy = 0; iy < grid.ny - 1; iy++) {
            for (uint32_t ix = 0; ix < grid.nx - 1; ix++) {
                // Corner values (standard MC indexing)
                float val[8] = {
                    grid.at(ix,   iy,   iz  ),
                    grid.at(ix+1, iy,   iz  ),
                    grid.at(ix+1, iy,   iz+1),
                    grid.at(ix,   iy,   iz+1),
                    grid.at(ix,   iy+1, iz  ),
                    grid.at(ix+1, iy+1, iz  ),
                    grid.at(ix+1, iy+1, iz+1),
                    grid.at(ix,   iy+1, iz+1),
                };

                // Determine cube index
                uint8_t cube_idx = 0;
                for (int i = 0; i < 8; i++) {
                    if (val[i] < 0) cube_idx |= (1 << i);
                }

                if (cube_idx == 0 || cube_idx == 255)
                    continue;  // cube entirely inside or outside

                // Edge intersections
                uint16_t edges = mc::edge_table[cube_idx];
                if (edges == 0) continue;

                // Interpolate vertex for each intersected edge
                vnes_vec3 edge_verts[12];
                for (int e = 0; e < 12; e++) {
                    if (edges & (1 << e)) {
                        uint8_t i0 = mc::edge_verts[e][0];
                        uint8_t i1 = mc::edge_verts[e][1];
                        float v0 = val[i0];
                        float v1 = val[i1];
                        float t = v0 / (v0 - v1);  // linear interpolation

                        vnes_vec3 p0 = grid.grid_to_world(
                            ix + (i0 & 1),
                            iy + ((i0 >> 1) & 1),
                            iz + ((i0 >> 2) & 1));
                        vnes_vec3 p1 = grid.grid_to_world(
                            ix + (i1 & 1),
                            iy + ((i1 >> 1) & 1),
                            iz + ((i1 >> 2) & 1));

                        edge_verts[e] = p0 + (p1 - p0) * t;
                    }
                }

                // Generate triangles
                const int8_t* tri = mc::tri_table() + (cube_idx * 16);
                for (int i = 0; tri[i] != -1; i += 3) {
                    uint32_t i0 = mesh.add_vertex(edge_verts[tri[i]]);
                    uint32_t i1 = mesh.add_vertex(edge_verts[tri[i+1]]);
                    uint32_t i2 = mesh.add_vertex(edge_verts[tri[i+2]]);
                    mesh.add_triangle(i0, i1, i2);
                }
            }
        }
    }

    // Compute normals via cross product
    for (uint32_t i = 0; i < mesh.triangle_count(); i++) {
        uint32_t i0 = mesh.indices[i * 3];
        uint32_t i1 = mesh.indices[i * 3 + 1];
        uint32_t i2 = mesh.indices[i * 3 + 2];
        vnes_vec3 e1 = mesh.vertices[i1] - mesh.vertices[i0];
        vnes_vec3 e2 = mesh.vertices[i2] - mesh.vertices[i0];
        vnes_vec3 n = normalize(cross(e1, e2));

        mesh.normals[i0] = mesh.normals[i0] + n;
        mesh.normals[i1] = mesh.normals[i1] + n;
        mesh.normals[i2] = mesh.normals[i2] + n;
    }

    for (uint32_t i = 0; i < (uint32_t)mesh.normals.size(); i++) {
        float len = length(mesh.normals[i]);
        if (len > 1e-8f) mesh.normals[i] = mesh.normals[i] / len;
    }
}
