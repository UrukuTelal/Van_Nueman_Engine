#pragma once

#include "Molecule.h"
#include "../voxel/VoxelCell.h"
#include "../voxel/BCCIndex.h"
#include <cmath>
#include <vector>
#include <unordered_map>
#include <cstdint>

struct DiffusionRate {
    float d_flux;        // diffusion coefficient for Flux-aligned molecules
    float d_temp;        // temperature dependence (Warmth)
    float d_barrier;     // barrier penetration (Resistance)

    DiffusionRate() : d_flux(0.1f), d_temp(0.01f), d_barrier(0.5f) {}
};

struct DiffusionConfig {
    DiffusionRate default_rate;
    std::unordered_map<uint64_t, DiffusionRate> per_molecule_rates;
    float dt;
    float grid_spacing;

    DiffusionConfig() : dt(0.1f), grid_spacing(1.0f) {}
};

struct ConcentrationField {
    std::unordered_map<uint64_t, float> concentrations;
    BCCCoord coord;
};

inline float concentration_gradient(float conc_here, float conc_there,
                                     float distance) {
    if (distance < 1e-6f) return 0.0f;
    return (conc_there - conc_here) / distance;
}

inline float diffusion_flux(float gradient, const DiffusionRate& rate,
                             const vn::fp20_t env_pillars[NumPillars]) {
    float warmth = env_pillars[Warmth].to_float();
    float resistance = env_pillars[Resistance].to_float();
    float flux_factor = env_pillars[Flux].to_float();

    float d_eff = rate.d_flux * (1.0f + warmth * rate.d_temp) *
                  (1.0f - resistance * rate.d_barrier * 0.5f);
    if (d_eff < 0.0f) d_eff = 0.0f;
    float flux = -d_eff * gradient * (0.5f + flux_factor * 0.5f);
    return flux;
}

inline void diffuse_molecule(Molecule& mol,
                              const std::vector<Molecule>& all_molecules,
                              const ConcentrationField& cf,
                              const vn::fp20_t env_pillars[NumPillars],
                              const DiffusionRate& rate,
                              float dt) {
    if (mol.atoms.empty()) return;

    float total_flux = 0.0f;
    for (size_t i = 0; i < mol.atoms.size(); i++) {
        if (!mol.atoms[i].active) continue;
        BCCCoord coord = mol.atoms[i].coord;

        for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
            BCCCoord neighbor = bcc_face_neighbor(coord, f);
            uint64_t nkey = bcc_hash(neighbor);

            float conc_here = 1.0f;
            auto it = cf.concentrations.find(nkey);
            float conc_there = (it != cf.concentrations.end()) ? it->second : 0.0f;

            float grad = concentration_gradient(conc_here, conc_there, 1.0f);
            float flux = diffusion_flux(grad, rate, env_pillars);
            total_flux += flux;
        }
    }

    float displacement = total_flux * dt / static_cast<float>(mol.atoms.size());
    if (fabsf(displacement) > 0.01f) {
        for (auto& atom : mol.atoms) {
            if (!atom.active) continue;
            int32_t di = static_cast<int32_t>(displacement * 10.0f);
            atom.coord.i += di;
            atom.coord.l = (atom.coord.i + atom.coord.j + atom.coord.k) & 1;
        }
    }
}

inline void diffusion_tick(std::vector<Molecule>& molecules,
                            const std::vector<ConcentrationField>& fields,
                            const vn::fp20_t env_pillars[NumPillars],
                            const DiffusionConfig& config) {
    for (size_t i = 0; i < molecules.size(); i++) {
        uint64_t mol_key = molecules[i].molecule_id;
        DiffusionRate rate = config.default_rate;
        auto it = config.per_molecule_rates.find(mol_key);
        if (it != config.per_molecule_rates.end()) {
            rate = it->second;
        }

        ConcentrationField cf;
        cf.coord = {0, 0, 0, 0};
        if (i < fields.size()) {
            cf = fields[i];
        }

        diffuse_molecule(molecules[i], molecules, cf, env_pillars, rate, config.dt);
    }
}

inline float fickian_flux(float conc_left, float conc_right,
                           const vn::fp20_t env_pillars[NumPillars],
                           float dx, float D_base = 0.1f) {
    float warmth = env_pillars[Warmth].to_float();
    float d_eff = D_base * (1.0f + warmth * 0.5f);
    return -d_eff * (conc_right - conc_left) / dx;
}

inline void build_concentration_fields(
    const std::vector<Molecule>& molecules,
    std::vector<ConcentrationField>& fields_out) {
    fields_out.clear();
    for (const auto& mol : molecules) {
        ConcentrationField cf;
        for (const auto& atom : mol.atoms) {
            if (!atom.active) continue;
            uint64_t key = bcc_hash(atom.coord);
            cf.concentrations[key] = 1.0f;
            cf.coord = atom.coord;
        }
        fields_out.push_back(cf);
    }
}
