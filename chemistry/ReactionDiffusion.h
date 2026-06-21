#pragma once

#include "EnvironmentalChemistry.h"
#include "DiffusionSystem.h"
#include "ReactionSystem.h"
#include "../voxel/VoxelCell.h"
#include "../voxel/BCCIndex.h"
#include <cmath>
#include <vector>
#include <unordered_map>
#include <cstdint>

struct ReactionDiffusionConfig {
    float dt;
    float diffusion_weight;
    float reaction_weight;
    float grid_spacing;
    uint32_t substeps;

    ReactionDiffusionConfig()
        : dt(0.05f), diffusion_weight(0.5f), reaction_weight(1.0f),
          grid_spacing(1.0f), substeps(4) {}
};

struct EnvironmentCell {
    BCCCoord coord;
    bool active;
    vn::fp20_t pillars[NumPillars];
    float concentration[NumPillars];

    EnvironmentCell() : active(false) {
        for (uint32_t p = 0; p < NumPillars; p++) {
            pillars[p] = vn::fp20_t(0.5f);
            concentration[p] = 0.0f;
        }
    }
};

inline void apply_turing_pattern(
    float& u_conc, float& v_conc, float du, float dv, float feed, float kill, float dt) {
    float uvv = u_conc * v_conc * v_conc;
    float du_dt = du * 0.0f - uvv + feed * (1.0f - u_conc);
    float dv_dt = dv * 0.0f + uvv - (feed + kill) * v_conc;
    u_conc += du_dt * dt;
    v_conc += dv_dt * dt;
    if (u_conc < 0.0f) u_conc = 0.0f;
    if (v_conc < 0.0f) v_conc = 0.0f;
}

inline void laplacian_3d(const EnvironmentCell& center,
                          const std::vector<EnvironmentCell>& neighbors,
                          float& laplacian_u, float& laplacian_v,
                          float dx) {
    laplacian_u = 0.0f;
    laplacian_v = 0.0f;
    float inv_dx2 = 1.0f / (dx * dx);
    for (const auto& n : neighbors) {
        laplacian_u += (n.concentration[Flux] - center.concentration[Flux]) * inv_dx2;
        laplacian_v += (n.concentration[Cohesion] - center.concentration[Cohesion]) * inv_dx2;
    }
}

inline void gray_scott_rd_tick(
    std::vector<EnvironmentCell>& grid,
    std::unordered_map<uint64_t, std::vector<uint32_t>>& adjacency,
    float du, float dv, float feed, float kill, float dt) {

    std::vector<float> new_u(grid.size()), new_v(grid.size());

    for (size_t i = 0; i < grid.size(); i++) {
        if (!grid[i].active) continue;
        float u = grid[i].concentration[Flux];
        float v = grid[i].concentration[Cohesion];

        float lap_u = 0.0f, lap_v = 0.0f;
        uint64_t key = bcc_hash(grid[i].coord);
        auto adj_it = adjacency.find(key);
        if (adj_it != adjacency.end()) {
            float dx = 1.0f;
            float inv_dx2 = 1.0f / (dx * dx);
            for (uint32_t ni : adj_it->second) {
                lap_u += (grid[ni].concentration[Flux] - u) * inv_dx2;
                lap_v += (grid[ni].concentration[Cohesion] - v) * inv_dx2;
            }
        }

        float uvv = u * v * v;
        new_u[i] = u + (du * lap_u - uvv + feed * (1.0f - u)) * dt;
        new_v[i] = v + (dv * lap_v + uvv - (feed + kill) * v) * dt;
    }

    for (size_t i = 0; i < grid.size(); i++) {
        if (!grid[i].active) continue;
        grid[i].concentration[Flux] = new_u[i] < 0.0f ? 0.0f : new_u[i];
        grid[i].concentration[Cohesion] = new_v[i] < 0.0f ? 0.0f : new_v[i];
    }
}

inline void reaction_diffusion_tick(
    std::vector<Molecule>& molecules,
    std::vector<EnvironmentCell>& env_grid,
    const vn::fp20_t env_pillars[NumPillars],
    const ReactionDiffusionConfig& config) {

    float sub_dt = config.dt / static_cast<float>(config.substeps);

    for (uint32_t s = 0; s < config.substeps; s++) {
        DiffusionConfig dconfig;
        dconfig.dt = sub_dt * config.diffusion_weight;

        std::vector<ConcentrationField> fields;
        build_concentration_fields(molecules, fields);
        diffusion_tick(molecules, fields, env_pillars, dconfig);

        for (auto& mol_a : molecules) {
            for (auto& mol_b : molecules) {
                if (mol_a.molecule_id >= mol_b.molecule_id) continue;
                std::vector<VoxelCell> env_cells;
                for (const auto& ec : env_grid) {
                    if (!ec.active) continue;
                    VoxelCell cell;
                    cell.init(ec.coord, vn::fp20_t(0.5f), 0, YieldMatrix());
                    cell.active = true;
                    for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
                        for (uint32_t p = 0; p < NumPillars; p++) {
                            cell.pyramids[f].material_composition[p] = ec.pillars[p];
                        }
                    }
                    env_cells.push_back(cell);
                }
                collide(mol_a, mol_b, &env_cells);
            }
        }

        float ph = compute_ph(env_pillars);
        for (auto& mol : molecules) {
            GradientField gf = sample_environment(env_pillars, 0.05f, 0.05f);
            (void)gf;
            pKaValue pka;
            pka.pKa = 7.0f;
            float protonated = protonation_state(ph, pka.pKa);
            for (auto& atom : mol.atoms) {
                if (!atom.active) continue;
                for (uint32_t f = 0; f < VoxelCell::FACE_COUNT / 2; f++) {
                    for (uint32_t p = 0; p < NumPillars; p++) {
                        float shift = (protonated - 0.5f) * 0.01f;
                        float val = atom.pyramids[f].material_composition[p].to_float() + shift;
                        if (val < 0.0f) val = 0.0f;
                        if (val > 1.0f) val = 1.0f;
                        atom.pyramids[f].material_composition[p] = vn::fp20_t(val);
                    }
                }
            }
        }
    }
}

inline void build_env_adjacency(
    const std::vector<EnvironmentCell>& grid,
    std::unordered_map<uint64_t, std::vector<uint32_t>>& adjacency) {
    adjacency.clear();
    for (size_t i = 0; i < grid.size(); i++) {
        if (!grid[i].active) continue;
        uint64_t key = bcc_hash(grid[i].coord);
        for (size_t j = 0; j < grid.size(); j++) {
            if (i == j || !grid[j].active) continue;
            int32_t di = grid[i].coord.i - grid[j].coord.i;
            int32_t dj = grid[i].coord.j - grid[j].coord.j;
            int32_t dk = grid[i].coord.k - grid[j].coord.k;
            float dist = sqrtf(static_cast<float>(di * di + dj * dj + dk * dk));
            if (dist <= 1.5f) {
                adjacency[key].push_back(static_cast<uint32_t>(j));
            }
        }
    }
}
