#pragma once

#include "../chemistry/ChemicalBond.h"
#include "../chemistry/Molecule.h"
#include "../voxel/VoxelCell.h"
#include "../voxel/BCCIndex.h"
#include "../voxel/YieldMatrix.h"
#include "../include/Entity.h"
#include <cmath>
#include <vector>
#include <cstdint>
#include <unordered_map>

struct BulkMaterialProperties {
    float tensile_strength;
    float hardness;
    float melting_point;
    float conductivity;
    float density;
    float optical_transparency;
    float flexibility;

    BulkMaterialProperties()
        : tensile_strength(0.5f), hardness(0.5f), melting_point(500.0f),
          conductivity(0.3f), density(1.0f), optical_transparency(0.5f),
          flexibility(0.3f) {}
};

struct ArrangedMaterial {
    uint64_t arrangement_hash;
    std::vector<uint64_t> molecule_keys;
    BulkMaterialProperties properties;
    uint32_t usage_count;

    ArrangedMaterial() : arrangement_hash(0), usage_count(0) {}
};

inline BulkMaterialProperties compute_bulk_properties(const Molecule& molecule) {
    BulkMaterialProperties props;

    if (molecule.atom_count() == 0) return props;

    float avg_integrity = 0.0f;
    float avg_cohesion = 0.0f;
    float avg_flux = 0.0f;
    float avg_warmth = 0.0f;
    float avg_attraction = 0.0f;

    for (const auto& atom : molecule.atoms) {
        for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
            avg_integrity += atom.pyramids[f].material_composition[Integrity].to_float();
            avg_cohesion += atom.pyramids[f].material_composition[Cohesion].to_float();
            avg_flux += atom.pyramids[f].material_composition[Flux].to_float();
            avg_warmth += atom.pyramids[f].material_composition[Warmth].to_float();
            avg_attraction += atom.pyramids[f].material_composition[Attraction].to_float();
        }
    }
    float count = static_cast<float>(molecule.atom_count() * VoxelCell::FACE_COUNT);
    avg_integrity /= count;
    avg_cohesion /= count;
    avg_flux /= count;
    avg_warmth /= count;
    avg_attraction /= count;

    props.tensile_strength = avg_integrity * 100.0f + avg_cohesion * 50.0f;
    props.hardness = avg_integrity * 200.0f + avg_cohesion * 100.0f;
    props.melting_point = 200.0f + avg_integrity * 400.0f + avg_cohesion * 300.0f;
    props.conductivity = avg_flux * 0.8f + avg_warmth * 0.2f;
    props.density = 0.5f + avg_cohesion * 1.5f + avg_integrity * 0.5f;
    props.optical_transparency = 1.0f - (avg_integrity * 0.3f + avg_cohesion * 0.2f);
    props.flexibility = 1.0f - avg_integrity * 0.5f;

    if (molecule.bond_count() > 0) {
        float bond_type_factor = 0.0f;
        for (const auto& bond : molecule.bonds) {
            switch (bond.type) {
                case BondType::Covalent: bond_type_factor += 1.0f; break;
                case BondType::Ionic: bond_type_factor += 0.8f; break;
                case BondType::Metallic: bond_type_factor += 0.6f; break;
                case BondType::PolarCovalent: bond_type_factor += 0.7f; break;
                case BondType::Hydrogen: bond_type_factor += 0.2f; break;
                case BondType::VanDerWaals: bond_type_factor += 0.1f; break;
                default: break;
            }
        }
        bond_type_factor /= molecule.bond_count();
        props.tensile_strength *= (0.5f + bond_type_factor * 0.5f);
        props.hardness *= (0.3f + bond_type_factor * 0.7f);
    }

    return props;
}

inline std::string material_category(const BulkMaterialProperties& props) {
    if (props.hardness > 200.0f && props.tensile_strength > 80.0f) return "Structural";
    if (props.conductivity > 0.6f) return "Conductive";
    if (props.optical_transparency > 0.6f) return "Optical";
    if (props.flexibility > 0.6f && props.tensile_strength > 30.0f) return "Elastic";
    if (props.melting_point > 700.0f) return "Refractory";
    if (props.density > 2.0f) return "Dense";
    return "Generic";
}

inline YieldMatrix material_to_yield_matrix(const BulkMaterialProperties& props) {
    YieldMatrix mat = YieldMatrix::default_rock();
    for (uint32_t v = 0; v < YIELD_VERTICES; v++) {
        mat.pillar_to_vertex[Integrity][v] = vn::fp20_t(props.hardness / 300.0f);
        mat.pillar_to_vertex[Cohesion][v] = vn::fp20_t(props.tensile_strength / 150.0f);
        mat.pillar_to_vertex[Flux][v] = vn::fp20_t(props.conductivity);
        mat.pillar_to_vertex[Warmth][v] = vn::fp20_t(props.melting_point / 1000.0f);
    }
    return mat;
}

struct MaterialDatabase {
    std::unordered_map<uint64_t, ArrangedMaterial> known_materials;

    void learn_material(const Molecule& molecule) {
        uint64_t hash = 0;
        for (const auto& atom : molecule.atoms) {
            hash ^= bcc_hash(atom.coord);
            hash *= 1099511628211ULL;
        }

        if (known_materials.find(hash) != known_materials.end()) {
            known_materials[hash].usage_count++;
            return;
        }

        ArrangedMaterial mat;
        mat.arrangement_hash = hash;
        mat.properties = compute_bulk_properties(molecule);
        mat.usage_count = 1;

        for (const auto& atom : molecule.atoms) {
            mat.molecule_keys.push_back(bcc_hash(atom.coord));
        }

        known_materials[hash] = mat;
    }

    const BulkMaterialProperties* lookup(uint64_t arrangement_hash) const {
        auto it = known_materials.find(arrangement_hash);
        if (it != known_materials.end()) return &it->second.properties;
        return nullptr;
    }

    BulkMaterialProperties find_best(float min_strength, float min_hardness) const {
        BulkMaterialProperties best;
        float best_score = 0.0f;
        for (const auto& [hash, mat] : known_materials) {
            (void)hash;
            float score = mat.properties.tensile_strength * (mat.properties.tensile_strength >= min_strength ? 1.0f : 0.0f) +
                          mat.properties.hardness * (mat.properties.hardness >= min_hardness ? 1.0f : 0.0f);
            if (score > best_score) {
                best = mat.properties;
                best_score = score;
            }
        }
        return best;
    }
};
