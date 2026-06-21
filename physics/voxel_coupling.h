#pragma once

#include "../vn/ScaledInt.h"
#include "../include/Entity.h"
#include "../voxel/VoxelCell.h"
#include "../voxel/DeformingVoxel.h"
#include "../voxel/BCCIndex.h"
#include "../scene/chunk.h"
#include <cstdint>
#include <unordered_map>

constexpr uint32_t VOXEL_COUPLING_MAX_CELLS = 4096;

// Per-vertex deformation state for one active cell.
struct CellDeformation {
    vn::fp20_t velocity[TRUNC_OCT_VERTICES][3];
    vn::fp20_t force[TRUNC_OCT_VERTICES][3];
    vn::fp20_t mass;
    vn::fp20_t spring_k;
    vn::fp20_t damping;
    bool active;

    CellDeformation()
        : mass(vn::fp20_t(1)), spring_k(vn::fp20_t(10)), damping(vn::fp20_t(2)), active(false)
    {
        for (uint32_t i = 0; i < TRUNC_OCT_VERTICES; i++)
            for (uint32_t j = 0; j < 3; j++)
                velocity[i][j] = force[i][j] = vn::fp20_t(0);
    }
};

using DeformationMap = std::unordered_map<uint64_t, CellDeformation>;

// Ensure a CellDeformation entry exists for the given cell.
inline CellDeformation& get_or_create_deformation(DeformationMap& map, uint64_t key) {
    auto it = map.find(key);
    if (it != map.end()) return it->second;
    CellDeformation& def = map[key];
    def.active = true;
    return def;
}

// Apply entity pillar forces to a single active cell's deforming vertices.
// Maps each pillar through the YieldMatrix to per-vertex force.
inline void apply_entity_forces_to_cell(const Entity& entity, VoxelCell& cell, CellDeformation& def) {
    for (uint32_t v = 0; v < TRUNC_OCT_VERTICES; v++) {
        vn::fp20_t fx(0), fy(0), fz(0);
        for (uint32_t p = 0; p < NumPillars; p++) {
            vn::fp20_t matrix_val = cell.material.pillar_to_vertex[p][v];
            vn::fp20_t pillar_val = vn::fp20_t(entity.pillars[p]);
            vn::fp20_t contrib = pillar_val * matrix_val;
            fx = fx + contrib;
            fy = fy + contrib;
            fz = fz + contrib;
        }
        def.force[v][0] = def.force[v][0] + fx;
        def.force[v][1] = def.force[v][1] + fy;
        def.force[v][2] = def.force[v][2] + fz;
    }
}

// Apply a radial force (explosion/impact) to all vertices of a cell.
inline void apply_radial_force_to_cell(float px, float py, float pz,
                                        vn::fp20_t magnitude, float radius,
                                        VoxelCell& cell, CellDeformation& def) {
    for (uint32_t v = 0; v < TRUNC_OCT_VERTICES; v++) {
        float vx = cell.geometry.vertices[v].x.to_float() + cell.coord.i;
        float vy = cell.geometry.vertices[v].y.to_float() + cell.coord.j;
        float vz = cell.geometry.vertices[v].z.to_float() + cell.coord.k;
        float dx = vx - px;
        float dy = vy - py;
        float dz = vz - pz;
        float dist = sqrtf(dx * dx + dy * dy + dz * dz);
        if (dist < 0.001f) dist = 0.001f;

        vn::fp20_t distance = vn::fp20_t(dist);
        vn::fp20_t falloff = vn::fp20_t(1);
        if (dist < radius) {
            falloff = (vn::fp20_t(radius) - distance) / vn::fp20_t(radius);
        } else {
            falloff = vn::fp20_t(0);
        }
        if (falloff == vn::fp20_t(0)) continue;

        // Inverse-square-like radial force
        vn::fp20_t inv_dist = vn::fp20_t(1.0f) / distance;
        vn::fp20_t force_mag = magnitude * falloff * inv_dist * inv_dist;

        def.force[v][0] = def.force[v][0] + force_mag * vn::fp20_t(dx / dist);
        def.force[v][1] = def.force[v][1] + force_mag * vn::fp20_t(dy / dist);
        def.force[v][2] = def.force[v][2] + force_mag * vn::fp20_t(dz / dist);
    }
}

// Run one tick of velocity Verlet integration on a cell's deformation state.
// Reads forces from CellDeformation, updates velocities, then updates geometry vertices.
inline void tick_cell_deformation(VoxelCell& cell, CellDeformation& def, vn::fp20_t dt) {
    for (uint32_t v = 0; v < TRUNC_OCT_VERTICES; v++) {
        DeformingVertex dv;
        dv.x = cell.geometry.vertices[v].x;
        dv.y = cell.geometry.vertices[v].y;
        dv.z = cell.geometry.vertices[v].z;
        dv.rest_x = cell.geometry.vertices[v].rest_x;
        dv.rest_y = cell.geometry.vertices[v].rest_y;
        dv.rest_z = cell.geometry.vertices[v].rest_z;
        dv.velocity_x = def.velocity[v][0];
        dv.velocity_y = def.velocity[v][1];
        dv.velocity_z = def.velocity[v][2];
        dv.force_x = def.force[v][0];
        dv.force_y = def.force[v][1];
        dv.force_z = def.force[v][2];
        dv.mass = def.mass;
        dv.spring_constant = def.spring_k;
        dv.damping = def.damping;

        // Add spring restoring force
        vn::fp20_t sfx, sfy, sfz;
        compute_spring_force(dv, sfx, sfy, sfz);
        dv.force_x = dv.force_x + sfx;
        dv.force_y = dv.force_y + sfy;
        dv.force_z = dv.force_z + sfz;

        // Verlet integration
        update_vertex_verlet(dv, dt);

        // Write back
        cell.geometry.vertices[v].x = dv.x;
        cell.geometry.vertices[v].y = dv.y;
        cell.geometry.vertices[v].z = dv.z;
        def.velocity[v][0] = dv.velocity_x;
        def.velocity[v][1] = dv.velocity_y;
        def.velocity[v][2] = dv.velocity_z;
        // force is consumed by Verlet (zeroed inside update_vertex_verlet)
        def.force[v][0] = dv.force_x;
        def.force[v][1] = dv.force_y;
        def.force[v][2] = dv.force_z;

        // Accumulate strain from displacement
        for (uint32_t f = 0; f < TRUNC_OCT_FACES; f++) {
            const TruncOctFace& face = cell.geometry.faces[f];
            for (uint32_t fi = 0; fi < face.vertex_count; fi++) {
                if (face.vertex_indices[fi] == v) {
                    vn::fp20_t strain = compute_strain(dv);
                    cell.pyramids[f].strain_accumulator =
                        cell.pyramids[f].strain_accumulator + strain;
                    break;
                }
            }
        }
    }
    cell.mark_dirty();
}

// High-level: apply all entity→voxel coupling for a single chunk.
// Entities use 2D (pos_x, pos_y); the chunk provides spatial locality.
inline void tick_entity_voxel_coupling(const std::vector<Entity>& entities,
                                        Chunk& chunk,
                                        DeformationMap& deformations,
                                        vn::fp20_t dt) {
    if (chunk.active_cell_count() == 0 || entities.empty()) return;

    int cx = chunk.get_chunk_x();
    int cy = chunk.get_chunk_y();

    // Phase 1: accumulate entity forces from entities in this chunk
    for (const Entity& entity : entities) {
        int ex = static_cast<int>(entity.pos_x / CHUNK_SIZE);
        int ey = static_cast<int>(entity.pos_y / CHUNK_SIZE);
        if (ex != cx || ey != cy) continue;

        // Collect keys for safe iteration
        std::vector<uint64_t> keys;
        for (const auto& [k, c] : chunk.get_active_cells())
            if (c.active) keys.push_back(k);

        for (uint64_t key : keys) {
            VoxelCell* cell = chunk.get_active_cell(key);
            if (!cell || !cell->active) continue;
            CellDeformation& def = get_or_create_deformation(deformations, key);
            apply_entity_forces_to_cell(entity, *cell, def);
        }
    }

    // Phase 2: tick deformation physics for all cells with deformation state
    std::vector<uint64_t> keys;
    for (const auto& [k, c] : chunk.get_active_cells())
        if (c.active) keys.push_back(k);

    for (uint64_t key : keys) {
        auto it = deformations.find(key);
        if (it == deformations.end()) continue;
        VoxelCell* cell = chunk.get_active_cell(key);
        if (cell && cell->active) {
            tick_cell_deformation(*cell, it->second, dt);
            cell->clear_dirty();
        }
    }
}
