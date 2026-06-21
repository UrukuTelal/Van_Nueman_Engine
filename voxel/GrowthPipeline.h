#pragma once

#include "TruncatedOctahedron.h"
#include "BCCIndex.h"
#include "VoxelCell.h"
#include "YieldMatrix.h"
#include "InteriorSubdivision.h"
#include "../vn/ScaledInt.h"
#include "../include/Entity.h"
#include <cstdint>
#include <vector>

struct GrowthEvent {
    uint32_t parent_cell_id;
    BCCCoord parent_coord;
    vn::fp20_t child_size;
    vn::fp20_t growth_energy;
    uint32_t face_index;
    uint32_t daughter_count;
    bool is_differentiation;
};

inline vn::fp20_t _growth_jitter(uint32_t seed_a, uint32_t seed_b, vn::fp20_t strength) {
    uint32_t s = seed_a * 0x9E3779B9u + seed_b;
    s = s * 1103515245u + 12345u;
    uint32_t r = (s >> 16) & 0x7FFFu;
    vn::fp20_t t(static_cast<float>(r) / 32767.0f);
    return (t * vn::fp20_t(2) - vn::fp20_t(1)) * strength;
}

inline vn::fp20_t _fp_exp_neg(vn::fp20_t x) {
    vn::fp20_t one(1);
    vn::fp20_t x2 = x * x;
    vn::fp20_t x3 = x2 * x;
    vn::fp20_t x4 = x2 * x2;
    return one - x + x2 / vn::fp20_t(2) - x3 / vn::fp20_t(6) + x4 / vn::fp20_t(24);
}

inline vn::fp20_t _fp_lerp(vn::fp20_t a, vn::fp20_t b, vn::fp20_t t) {
    return a + (b - a) * t;
}

class GrowthPipeline {
public:
    static const vn::fp20_t MIN_DIVISION_SIZE;
    static const vn::fp20_t DEFAULT_DIVISION_ENERGY_THRESHOLD;
    static const vn::fp20_t MIN_FACE_STRESS_THRESHOLD;
    static const vn::fp20_t YIELD_JITTER_STRENGTH;
    static const vn::fp20_t MORPHOGEN_DECAY_RATE;

    static bool check_division_ready(const VoxelCell& cell,
                                     vn::fp20_t local_warmth,
                                     vn::fp20_t local_flux,
                                     vn::fp20_t local_attraction,
                                     vn::fp20_t energy_threshold)
    {
        if (!cell.active)
            return false;

        vn::fp20_t parent_size = cell.geometry.vertices[0].x.abs();
        if (parent_size < MIN_DIVISION_SIZE)
            return false;

        for (uint32_t i = 0; i < VoxelCell::FACE_COUNT; i++) {
            if (cell.pyramids[i].structural_integrity <= vn::fp20_t(0.8f))
                return false;
        }

        vn::fp20_t total_energy = local_warmth + local_flux + local_attraction;
        return total_energy > energy_threshold;
    }

    static uint32_t determine_division_face(const VoxelCell& cell,
                                             const vn::fp20_t face_stress[VoxelCell::FACE_COUNT])
    {
        uint32_t best_face = 0;
        vn::fp20_t best_stress = face_stress[0];

        for (uint32_t f = 1; f < VoxelCell::FACE_COUNT; f++) {
            if (face_stress[f] > best_stress) {
                best_stress = face_stress[f];
                best_face = f;
            }
        }

        if (best_stress > MIN_FACE_STRESS_THRESHOLD)
            return best_face;

        uint32_t min_face = 0;
        vn::fp20_t min_stress = face_stress[0];
        for (uint32_t f = 1; f < VoxelCell::FACE_COUNT; f++) {
            if (face_stress[f] < min_stress) {
                min_stress = face_stress[f];
                min_face = f;
            }
        }
        return min_face;
    }

    static bool subdivide_cell(const VoxelCell& parent,
                                vn::fp20_t growth_energy,
                                uint32_t division_face,
                                VoxelCell& out_daughter,
                                std::vector<GrowthEvent>& out_events)
    {
        if (!parent.active)
            return false;

        BCCCoord daughter_coord = bcc_face_neighbor(parent.coord, division_face);
        vn::fp20_t parent_size = parent.geometry.vertices[0].x.abs();
        vn::fp20_t child_size = parent_size / vn::fp20_t(2);

        uint32_t daughter_id = parent.cell_id * VoxelCell::FACE_COUNT + division_face + 1;

        YieldMatrix daughter_mat;
        for (uint32_t p = 0; p < NumPillars; p++) {
            for (uint32_t v = 0; v < YIELD_VERTICES; v++) {
                vn::fp20_t jitter = _growth_jitter(
                    parent.cell_id, p * YIELD_VERTICES + v, YIELD_JITTER_STRENGTH);
                vn::fp20_t val = parent.material.pillar_to_vertex[p][v] + jitter;
                daughter_mat.pillar_to_vertex[p][v] = vn::clamp(val, vn::fp20_t(0), vn::fp20_t(1));
            }
        }

        out_daughter.init(daughter_coord, child_size, daughter_id, daughter_mat);

        vn::fp20_t avg_composition[NumPillars];
        for (uint32_t p = 0; p < NumPillars; p++) {
            avg_composition[p] = vn::fp20_t(0);
            for (uint32_t i = 0; i < VoxelCell::FACE_COUNT; i++) {
                avg_composition[p] += parent.pyramids[i].material_composition[p];
            }
            avg_composition[p] = avg_composition[p] / vn::fp20_t(VoxelCell::FACE_COUNT);
        }

        for (uint32_t i = 0; i < VoxelCell::FACE_COUNT; i++) {
            for (uint32_t p = 0; p < NumPillars; p++) {
                vn::fp20_t jitter = _growth_jitter(
                    parent.cell_id, i * NumPillars + p + YIELD_VERTICES,
                    YIELD_JITTER_STRENGTH);
                vn::fp20_t val = avg_composition[p] + jitter;
                out_daughter.pyramids[i].material_composition[p] =
                    vn::clamp(val, vn::fp20_t(0), vn::fp20_t(1));
            }
        }

        GrowthEvent ev;
        ev.parent_cell_id = parent.cell_id;
        ev.parent_coord = parent.coord;
        ev.child_size = child_size;
        ev.growth_energy = growth_energy;
        ev.face_index = division_face;
        ev.daughter_count = 1;
        ev.is_differentiation = false;
        out_events.push_back(ev);

        return true;
    }

    static uint32_t fill_interior(const VoxelCell& parent,
                                   uint32_t depth,
                                   std::vector<VoxelCell>& out_interior_cells,
                                   std::vector<GrowthEvent>& out_events)
    {
        std::vector<BCCCoord> children = bcc_interior_children(parent.coord,
            static_cast<int>(depth));
        uint32_t count = static_cast<uint32_t>(children.size());

        vn::fp20_t parent_size = parent.geometry.vertices[0].x.abs();
        vn::fp20_t child_size = parent_size / vn::fp20_t(2);

        for (uint32_t i = 0; i < count; i++) {
            VoxelCell child;
            uint32_t child_id = parent.cell_id * (count + 1) + i + 1;

            child.init(children[i], child_size, child_id, parent.material);

            for (uint32_t p = 0; p < NumPillars; p++) {
                vn::fp20_t avg = vn::fp20_t(0);
                for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
                    avg += parent.pyramids[f].material_composition[p];
                }
                avg = avg / vn::fp20_t(VoxelCell::FACE_COUNT);
                for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
                    child.pyramids[f].material_composition[p] = avg;
                }
            }

            out_interior_cells.push_back(child);

            GrowthEvent ev;
            ev.parent_cell_id = parent.cell_id;
            ev.parent_coord = parent.coord;
            ev.child_size = child_size;
            ev.growth_energy = vn::fp20_t(0);
            ev.face_index = 0;
            ev.daughter_count = 0;
            ev.is_differentiation = true;
            out_events.push_back(ev);
        }

        return count;
    }

    static void apply_morphogen_gradient(VoxelCell& cell,
                                          const BCCCoord& gradient_origin,
                                          const vn::fp20_t gradient_strength[NumPillars])
    {
        int32_t di = cell.coord.i - gradient_origin.i;
        int32_t dj = cell.coord.j - gradient_origin.j;
        int32_t dk = cell.coord.k - gradient_origin.k;

        vn::fp20_t dx(static_cast<float>(di));
        vn::fp20_t dy(static_cast<float>(dj));
        vn::fp20_t dz(static_cast<float>(dk));

        vn::fp20_t dist_sq = dx * dx + dy * dy + dz * dz;
        vn::fp20_t distance = vn::fp_sqrt(dist_sq);
        vn::fp20_t influence = _fp_exp_neg(distance * MORPHOGEN_DECAY_RATE);

        for (uint32_t p = 0; p < NumPillars; p++) {
            for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
                vn::fp20_t current = cell.pyramids[f].material_composition[p];
                cell.pyramids[f].material_composition[p] =
                    _fp_lerp(current, gradient_strength[p], influence);
            }
        }
    }
};

inline const vn::fp20_t GrowthPipeline::MIN_DIVISION_SIZE(0.01f);
inline const vn::fp20_t GrowthPipeline::DEFAULT_DIVISION_ENERGY_THRESHOLD(0.6f);
inline const vn::fp20_t GrowthPipeline::MIN_FACE_STRESS_THRESHOLD(0.1f);
inline const vn::fp20_t GrowthPipeline::YIELD_JITTER_STRENGTH(0.05f);
inline const vn::fp20_t GrowthPipeline::MORPHOGEN_DECAY_RATE(0.1f);
