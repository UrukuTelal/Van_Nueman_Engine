#pragma once

#include "../voxel/VoxelCell.h"
#include "../voxel/BCCIndex.h"
#include "../include/Entity.h"
#include <cstdint>
#include <cmath>

enum class BondType : uint32_t {
    None = 0,
    Ionic = 1,
    Covalent = 2,
    Metallic = 3,
    Hydrogen = 4,
    VanDerWaals = 5,
    PolarCovalent = 6
};

inline const char* bond_type_name(BondType t) {
    switch (t) {
        case BondType::None: return "None";
        case BondType::Ionic: return "Ionic";
        case BondType::Covalent: return "Covalent";
        case BondType::Metallic: return "Metallic";
        case BondType::Hydrogen: return "Hydrogen";
        case BondType::VanDerWaals: return "VanDerWaals";
        case BondType::PolarCovalent: return "PolarCovalent";
    }
    return "Unknown";
}

struct ChemicalBond {
    uint64_t atom_a_key;
    uint64_t atom_b_key;
    BondType type;
    float strength;
    float bond_length;
    float energy;

    bool involves(uint64_t key) const { return atom_a_key == key || atom_b_key == key; }
    uint64_t partner(uint64_t key) const { return (atom_a_key == key) ? atom_b_key : atom_a_key; }
};

inline BondType detect_bond_type(const vn::fp20_t a_pillars[NumPillars],
                                  const vn::fp20_t b_pillars[NumPillars]) {
    float a_attract = a_pillars[Attraction].to_float();
    float b_attract = b_pillars[Attraction].to_float();
    float a_flux = a_pillars[Flux].to_float();
    float b_flux = b_pillars[Flux].to_float();
    float a_warmth = a_pillars[Warmth].to_float();
    float b_warmth = b_pillars[Warmth].to_float();
    float a_integrity = a_pillars[Integrity].to_float();
    float b_integrity = b_pillars[Integrity].to_float();
    float a_cohesion = a_pillars[Cohesion].to_float();
    float b_cohesion = b_pillars[Cohesion].to_float();

    float attract_mean = (a_attract + b_attract) * 0.5f;
    float attract_diff = fabsf(a_attract - b_attract);
    float flux_diff = fabsf(a_flux - b_flux);
    float warmth_mean = (a_warmth + b_warmth) * 0.5f;
    float integrity_mean = (a_integrity + b_integrity) * 0.5f;
    float cohesion_mean = (a_cohesion + b_cohesion) * 0.5f;

    if (attract_mean > 0.7f && attract_diff > 0.3f) return BondType::Ionic;
    if (attract_mean > 0.6f && attract_diff < 0.2f && flux_diff > 0.3f) return BondType::PolarCovalent;
    if (attract_mean > 0.5f && integrity_mean > 0.5f) return BondType::Covalent;
    if (cohesion_mean > 0.6f && integrity_mean > 0.4f) return BondType::Metallic;
    if (warmth_mean > 0.6f && attract_mean > 0.3f) return BondType::Hydrogen;
    if (warmth_mean > 0.3f) return BondType::VanDerWaals;

    return BondType::None;
}

inline float bond_strength(BondType type, const vn::fp20_t a_pillars[NumPillars],
                            const vn::fp20_t b_pillars[NumPillars]) {
    float base = 0.0f;
    switch (type) {
        case BondType::Covalent: base = 0.9f; break;
        case BondType::Ionic: base = 0.8f; break;
        case BondType::PolarCovalent: base = 0.7f; break;
        case BondType::Metallic: base = 0.6f; break;
        case BondType::Hydrogen: base = 0.3f; break;
        case BondType::VanDerWaals: base = 0.1f; break;
        default: return 0.0f;
    }

    float cohesion = (a_pillars[Cohesion].to_float() + b_pillars[Cohesion].to_float()) * 0.5f;
    float integrity = (a_pillars[Integrity].to_float() + b_pillars[Integrity].to_float()) * 0.5f;
    return base * (cohesion * 0.6f + integrity * 0.4f);
}

inline float bond_energy(BondType type) {
    switch (type) {
        case BondType::Covalent: return 400.0f;
        case BondType::Ionic: return 300.0f;
        case BondType::PolarCovalent: return 250.0f;
        case BondType::Metallic: return 150.0f;
        case BondType::Hydrogen: return 20.0f;
        case BondType::VanDerWaals: return 2.0f;
        default: return 0.0f;
    }
}

inline ChemicalBond detect_bond(uint64_t key_a, uint64_t key_b,
                                 const vn::fp20_t a_pillars[NumPillars],
                                 const vn::fp20_t b_pillars[NumPillars]) {
    ChemicalBond bond{};
    bond.atom_a_key = key_a;
    bond.atom_b_key = key_b;
    bond.type = detect_bond_type(a_pillars, b_pillars);
    if (bond.type == BondType::None) return bond;
    bond.strength = bond_strength(bond.type, a_pillars, b_pillars);
    bond.bond_length = 0.5f;
    bond.energy = bond_energy(bond.type) * bond.strength;
    return bond;
}

inline bool can_form_bond(const vn::fp20_t a_pillars[NumPillars],
                           const vn::fp20_t b_pillars[NumPillars]) {
    float attract = (a_pillars[Attraction].to_float() + b_pillars[Attraction].to_float()) * 0.5f;
    float cohesion = (a_pillars[Cohesion].to_float() + b_pillars[Cohesion].to_float()) * 0.5f;
    float harm = (a_pillars[Harm].to_float() + b_pillars[Harm].to_float()) * 0.5f;
    return attract > 0.3f && cohesion > 0.2f && harm < 0.7f;
}
