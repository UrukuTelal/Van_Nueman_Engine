#include "scale_organism_renderer.h"
#include "../voxel/InteriorSubdivision.h"
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <cstdlib>

namespace Van_Nueman {

ScaleOrganismRenderer::ScaleOrganismRenderer()
    : observer_scale_(SCALE_ORGANISM) {}

float ScaleOrganismRenderer::get_lod_blend_factor(ScaleExponent organism_scale) const {
    ScaleExponent delta = scale_delta(observer_scale_, organism_scale);
    if (delta < 0) delta = -delta;
    if (delta == 0) return 1.0f;
    return 1.0f / static_cast<float>(1 << delta);
}



void ScaleOrganismRenderer::build_lod_cells(const VoxelOrganism& organism,
                                              float blend_factor,
                                              std::vector<LODCell>& out_cells) {
    out_cells.clear();

    auto boundary = organism.find_boundary_cells();
    std::unordered_map<uint64_t, bool> boundary_map;
    for (auto bi : boundary) {
        BCCCoord c = organism.get_cell_coord(bi);
        boundary_map[bcc_hash(c)] = true;
    }

    for (uint32_t i = 0; i < organism.cell_count(); i++) {
        const VoxelCell& cell = organism.get_cell(i);
        if (!cell.active) continue;

        LODCell lod;
        lod.coord = cell.coord;
        lod.size = cell.geometry.vertices[0].x.abs().to_float();
        lod.is_boundary = boundary_map.count(bcc_hash(cell.coord)) > 0;

        bool all_fractured = true;
        for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
            if (!cell.pyramids[f].fractured) { all_fractured = false; break; }
        }
        lod.all_fractured = all_fractured;

        for (uint32_t p = 0; p < NumPillars; p++) {
            vn::fp20_t sum(0);
            uint32_t fcount = 0;
            for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
                if (!cell.pyramids[f].fractured) {
                    sum = sum + cell.pyramids[f].material_composition[p];
                    fcount++;
                }
            }
            lod.avg_pillars[p] = (fcount > 0)
                ? sum / vn::fp20_t(static_cast<float>(fcount))
                : vn::fp20_t(0.5f);
        }

        if (lod.is_boundary && !lod.all_fractured) {
            lod.alpha = 1.0f;
        } else if (lod.all_fractured) {
            float interior_alpha = (1.0f - blend_factor) * 0.8f + 0.2f;
            lod.alpha = interior_alpha;
        } else {
            float fracture_count = 0;
            for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
                if (cell.pyramids[f].fractured) fracture_count += 1.0f;
            }
            float fracture_ratio = fracture_count / static_cast<float>(VoxelCell::FACE_COUNT);
            lod.alpha = 1.0f - fracture_ratio * (1.0f - blend_factor);
        }

        lod.child_count = 0;
        lod.merged_from = 1;
        out_cells.push_back(lod);
    }
}

void ScaleOrganismRenderer::merge_adjacent_cells(const std::vector<LODCell>& input_cells,
                                                   float merge_threshold,
                                                   std::vector<LODCell>& out_merged) {
    out_merged.clear();
    if (input_cells.empty()) return;

    if (merge_threshold <= 0.0f) {
        out_merged = input_cells;
        return;
    }

    std::unordered_map<uint64_t, const LODCell*> cell_map;
    for (const auto& c : input_cells) {
        cell_map[bcc_hash(c.coord)] = &c;
    }

    std::vector<bool> consumed(input_cells.size(), false);

    for (size_t i = 0; i < input_cells.size(); i++) {
        if (consumed[i]) continue;
        consumed[i] = true;

        const LODCell& base = input_cells[i];

        std::vector<const LODCell*> group;
        group.push_back(&base);

        for (size_t j = i + 1; j < input_cells.size(); j++) {
            if (consumed[j]) continue;
            const LODCell& candidate = input_cells[j];

            for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
                BCCCoord n = bcc_face_neighbor(base.coord, f);
                if (bcc_equal(n, candidate.coord)) {
                    group.push_back(&candidate);
                    consumed[j] = true;
                    break;
                }
            }
        }

        if (group.size() <= 1) {
            LODCell single = base;
            single.child_count = 0;
            single.merged_from = 1;
            out_merged.push_back(single);
        } else {
            LODCell merged;
            merged.coord = base.coord;
            merged.size = base.size * std::pow(static_cast<float>(group.size()), 1.0f / 3.0f);
            merged.is_boundary = false;
            merged.all_fractured = true;
            merged.child_count = static_cast<uint32_t>(group.size());
            merged.merged_from = static_cast<uint32_t>(group.size());
            merged.alpha = 0.0f;

            for (uint32_t p = 0; p < NumPillars; p++) {
                vn::fp20_t sum(0);
                float alpha_sum = 0;
                for (auto* c : group) {
                    sum = sum + c->avg_pillars[p] * vn::fp20_t(c->alpha);
                    alpha_sum += c->alpha;
                    if (c->is_boundary) merged.is_boundary = true;
                    if (!c->all_fractured) merged.all_fractured = false;
                }
                merged.avg_pillars[p] = (alpha_sum > 0)
                    ? sum / vn::fp20_t(alpha_sum)
                    : vn::fp20_t(0.5f);
            }

            for (auto* c : group) {
                if (c->alpha > merged.alpha) merged.alpha = c->alpha;
            }

            out_merged.push_back(merged);
        }
    }
}

void ScaleOrganismRenderer::generate_lod_mesh(const std::vector<LODCell>& lod_cells,
                                                float blend_factor,
                                                std::vector<VoxelMeshData>& out_meshes) {
    out_meshes.clear();
    (void)blend_factor;

    for (const auto& lod : lod_cells) {
        VoxelCell temp;
        YieldMatrix default_mat;
        temp.init(lod.coord, vn::fp20_t(lod.size), 0, default_mat);
        temp.active = true;

        vn::fp20_t face_weights[TRUNC_OCT_FACES][NumPillars];
        for (uint32_t f = 0; f < TRUNC_OCT_FACES; f++) {
            for (uint32_t p = 0; p < NumPillars; p++) {
                face_weights[f][p] = lod.avg_pillars[p];
            }
        }

        VoxelMeshData mesh;
        generate_voxel_mesh(temp.geometry, face_weights, mesh, false);

        for (uint32_t v = 0; v < mesh.vertex_count; v++) {
            mesh.vertices[v].r = detail::pillar_weight_to_color(lod.avg_pillars[Integrity]) * 0.8f + 0.2f;
            mesh.vertices[v].g = detail::pillar_weight_to_color(lod.avg_pillars[Warmth]) * 0.7f + 0.1f;
            mesh.vertices[v].b = detail::pillar_weight_to_color(lod.avg_pillars[Flux]) * 0.6f + 0.1f;
            mesh.vertices[v].a = lod.alpha;
        }

        out_meshes.push_back(mesh);
    }
}

void ScaleOrganismRenderer::generate_organism_mesh(const VoxelOrganism& organism,
                                                     std::vector<VoxelMeshData>& out_meshes) {
    out_meshes.clear();

    if (!organism.is_alive() || organism.cell_count() == 0) return;

    ScaleExponent organism_scale = SCALE_ORGANISM;
    float blend_factor = get_lod_blend_factor(organism_scale);
    if (blend_factor < 1e-6f) return;

    std::vector<LODCell> cells;
    build_lod_cells(organism, blend_factor, cells);

    if (blend_factor < 0.3f) {
        float merge_threshold = (0.3f - blend_factor) / 0.3f;
        std::vector<LODCell> merged;
        merge_adjacent_cells(cells, merge_threshold, merged);
        generate_lod_mesh(merged, blend_factor, out_meshes);
    } else {
        generate_lod_mesh(cells, blend_factor, out_meshes);
    }
}

}
