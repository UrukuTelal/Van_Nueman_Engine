#pragma once

#include "TruncatedOctahedron.h"
#include "BCCIndex.h"
#include "DeformingVoxel.h"
#include "VoxelCell.h"
#include "../scale/ScaleExponent.h"
#include "../scale/AttentionEvaluator.h"
#include "../scale/InfluenceBuffer.h"
#include "../audio/wht_scaled.h"
#include <cstdint>
#include <vector>

struct FractureEvent {
    uint32_t parent_cell_id;
    uint32_t face_index;
    vn::fp20_t fracture_energy;
    bool spawns_discrete_children;
    BCCCoord parent_coord;
    vn::fp20_t child_size;
};

class FracturePipeline {
public:
    static bool check_yield(const VoxelCell& cell,
                            const vn::fp20_t applied_force[NumPillars],
                            uint32_t& out_fractured_face)
    {
        for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
            const TetradSubCell& pyr = cell.pyramids[f];
            if (pyr.fractured) continue;

            vn::fp20_t applied_stress = cell.material.compute_yield_threshold(
                cell.geometry.faces[f].vertex_indices[0], applied_force);

            if (pyr.structural_integrity < applied_stress) {
                out_fractured_face = f;
                return true;
            }
        }
        return false;
    }

    static uint32_t splinter_pyramid(const VoxelCell& parent,
                                      uint32_t face_idx,
                                      std::vector<BCCCoord>& out_children,
                                      std::vector<FractureEvent>& out_events)
    {
        const TruncOctFace& face = parent.geometry.faces[face_idx];
        uint32_t spawned = 0;

        for (uint32_t i = 0; i < face.vertex_count; i++) {
            const TruncOctVertex& v = parent.geometry.vertices[face.vertex_indices[i]];
            BCCCoord child_coord = bcc_child(parent.coord, 1);
            child_coord.i += static_cast<int32_t>(v.x.raw() >> 4);
            child_coord.j += static_cast<int32_t>(v.y.raw() >> 4);
            child_coord.k += static_cast<int32_t>(v.z.raw() >> 4);
            child_coord.l = (child_coord.i + child_coord.j + child_coord.k) & 1;

            out_children.push_back(child_coord);

            FractureEvent ev;
            ev.parent_cell_id = parent.cell_id;
            ev.face_index = face_idx;
            ev.fracture_energy = parent.pyramids[face_idx].strain_accumulator;
            ev.spawns_discrete_children = true;
            ev.parent_coord = parent.coord;
            ev.child_size = vn::fp20_t(1) >> 1;
            out_events.push_back(ev);
            spawned++;
        }
        return spawned;
    }

    static bool evaluate_attention_gate(ScaleExponent observer_scale,
                                         vn::fp20_t observer_focus,
                                         const Entity& target)
    {
        vn::fp20_t dummy[16];
        return AttentionEvaluator::evaluate_presence(
            observer_scale, observer_focus, target, dummy);
    }

    static void collapse_to_influence(const VoxelCell& cell,
                                       vn::fp20_t out_wht_coeffs[NumPillars])
    {
        vn::fp20_t integrity_values[NumPillars];
        for (uint32_t i = 0; i < NumPillars; i++) {
            if (i < VoxelCell::FACE_COUNT)
                integrity_values[i] = cell.pyramids[i].structural_integrity;
            else
                integrity_values[i] = vn::fp20_t(0);
        }
        fwht_fp(integrity_values, 4, true);
        for (int i = 0; i < NumPillars; i++)
            out_wht_coeffs[i] = integrity_values[i];
    }

    static void process_fracture(VoxelCell& cell,
                                  uint32_t face_idx,
                                  InfluenceBuffer& buffer,
                                  std::vector<BCCCoord>& spawned_children,
                                  std::vector<FractureEvent>& events)
    {
        TetradSubCell& pyramid = cell.pyramids[face_idx];
        pyramid.fractured = true;

        vn::fp20_t wht_coeffs[NumPillars];
        collapse_to_influence(cell, wht_coeffs);
        buffer.accumulate(cell.coord.i, cell.coord.j, cell.coord.k, wht_coeffs, vn::fp20_t(1));

        splinter_pyramid(cell, face_idx, spawned_children, events);
    }
};
