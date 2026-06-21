#pragma once

#include "ChemicalBond.h"
#include "Molecule.h"
#include "../include/Entity.h"
#include "../include/Transform.h"
#include "../voxel/VoxelCell.h"
#include "../voxel/BCCIndex.h"
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <cmath>
#include <cstdlib>

struct CollisionEvent {
    uint64_t molecule_a_id;
    uint64_t molecule_b_id;
    float approach_energy;
    bool reacted;
    float bond_energy_released;
    uint32_t bonds_broken;
};

struct ReactionResult {
    Molecule product;
    float energy_released;
    float energy_absorbed;
    uint32_t bonds_formed;
    uint32_t bonds_broken;
    bool catalyst_used;
};

inline bool is_catalyst(const vn::fp20_t pillars[NumPillars], float threshold_awareness = 0.7f) {
    float awareness = pillars[Awareness].to_float();
    float harm = pillars[Harm].to_float();
    return awareness > threshold_awareness && harm < 0.3f;
}

inline float reaction_threshold(const Molecule& a, const Molecule& b) {
    float a_stability = a.avg_pillar(Integrity).to_float() +
                        a.avg_pillar(Cohesion).to_float();
    float b_stability = b.avg_pillar(Integrity).to_float() +
                        b.avg_pillar(Cohesion).to_float();
    float avg_stability = (a_stability + b_stability) * 0.5f;
    return 1.0f - avg_stability * 0.5f;
}

inline bool can_collide(const Molecule& a, const Molecule& b) {
    if (a.atoms.empty() || b.atoms.empty()) return false;

    for (const auto& atom_a : a.atoms) {
        if (!atom_a.active) continue;
        for (const auto& atom_b : b.atoms) {
            if (!atom_b.active) continue;
            int32_t di = atom_a.coord.i - atom_b.coord.i;
            int32_t dj = atom_a.coord.j - atom_b.coord.j;
            int32_t dk = atom_a.coord.k - atom_b.coord.k;
            float dist = sqrtf(static_cast<float>(di * di + dj * dj + dk * dk));
            if (dist <= 1.5f) return true;
        }
    }
    return false;
}

inline float catalyst_boost(const std::vector<VoxelCell>& environment) {
    float boost = 1.0f;
    for (const auto& cell : environment) {
        if (!cell.active) continue;
        vn::fp20_t avg_pillars[NumPillars];
        for (uint32_t p = 0; p < NumPillars; p++) {
            vn::fp20_t sum(0);
            for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
                sum = sum + cell.pyramids[f].material_composition[p];
            }
            avg_pillars[p] = sum / vn::fp20_t(static_cast<float>(VoxelCell::FACE_COUNT));
        }
        if (is_catalyst(avg_pillars, 0.6f)) {
            boost *= 0.7f;
        }
    }
    return boost;
}

inline ReactionResult react(Molecule& a, Molecule& b,
                             const std::vector<VoxelCell>* environment = nullptr) {
    ReactionResult result{};
    result.energy_released = 0.0f;
    result.energy_absorbed = 0.0f;
    result.bonds_formed = 0;
    result.bonds_broken = 0;
    result.catalyst_used = false;

    float threshold = reaction_threshold(a, b);
    float total_energy = a.total_bond_energy() + b.total_bond_energy();

    if (environment) {
        float cboost = catalyst_boost(*environment);
        threshold *= cboost;
        if (cboost < 1.0f) result.catalyst_used = true;
    }

    float roll = static_cast<float>(std::rand()) / RAND_MAX;
    if (roll > threshold) return result;

    for (auto& atom_a : a.atoms) {
        if (!atom_a.active) continue;
        for (auto& atom_b : b.atoms) {
            if (!atom_b.active) continue;
            int32_t di = atom_a.coord.i - atom_b.coord.i;
            int32_t dj = atom_a.coord.j - atom_b.coord.j;
            int32_t dk = atom_a.coord.k - atom_b.coord.k;
            if (di * di + dj * dj + dk * dk > 2) continue;

            vn::fp20_t a_avg[NumPillars], b_avg[NumPillars];
            for (uint32_t p = 0; p < NumPillars; p++) {
                vn::fp20_t a_sum(0), b_sum(0);
                for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
                    a_sum = a_sum + atom_a.pyramids[f].material_composition[p];
                    b_sum = b_sum + atom_b.pyramids[f].material_composition[p];
                }
                a_avg[p] = a_sum / vn::fp20_t(static_cast<float>(VoxelCell::FACE_COUNT));
                b_avg[p] = b_sum / vn::fp20_t(static_cast<float>(VoxelCell::FACE_COUNT));
            }

            ChemicalBond bond = detect_bond(
                bcc_hash(atom_a.coord), bcc_hash(atom_b.coord), a_avg, b_avg);

            if (bond.type != BondType::None && can_form_bond(a_avg, b_avg)) {
                uint64_t ak = bcc_hash(atom_a.coord);
                uint64_t bk = bcc_hash(atom_b.coord);
                bond.atom_a_key = ak;
                bond.atom_b_key = bk;

                bool exists = false;
                for (const auto& existing : a.bonds) {
                    if ((existing.atom_a_key == ak && existing.atom_b_key == bk) ||
                        (existing.atom_a_key == bk && existing.atom_b_key == ak)) {
                        exists = true;
                        break;
                    }
                }
                for (const auto& existing : b.bonds) {
                    if ((existing.atom_a_key == ak && existing.atom_b_key == bk) ||
                        (existing.atom_a_key == bk && existing.atom_b_key == ak)) {
                        exists = true;
                        break;
                    }
                }

                if (!exists) {
                    PillarStateVector subject;
                    for (uint32_t p = 0; p < NumPillars; p++)
                        subject[p] = b_avg[p];

                    PillarStateVector actor;
                    for (uint32_t p = 0; p < NumPillars; p++)
                        actor[p] = a_avg[p];

                    TransformResult tr = transform_operation(
                        actor, subject, Attraction, Cohesion, vn::fp20_t(0.0f));

                    float energy = bond.energy * tr.alignment;
                    if (energy > 0) {
                        result.energy_released += energy;
                    } else {
                        result.energy_absorbed -= energy;
                    }

                    a.bonds.push_back(bond);
                    result.bonds_formed++;
                }
            }
        }
    }

    return result;
}

inline CollisionEvent collide(Molecule& a, Molecule& b,
                               const std::vector<VoxelCell>* environment = nullptr) {
    CollisionEvent event{};
    event.molecule_a_id = a.molecule_id;
    event.molecule_b_id = b.molecule_id;
    event.approach_energy = a.total_bond_energy() + b.total_bond_energy();
    event.reacted = false;

    if (!can_collide(a, b)) return event;

    ReactionResult rr = react(a, b, environment);
    event.reacted = (rr.bonds_formed > 0);
    event.bond_energy_released = rr.energy_released;

    if (event.reacted) {
        for (auto& bond : b.bonds) {
            bool still_valid = false;
            for (const auto& ba : a.atoms) {
                uint64_t ak = bcc_hash(ba.coord);
                if (bond.atom_a_key == ak || bond.atom_b_key == ak) {
                    still_valid = true;
                    break;
                }
            }
            if (still_valid) {
                a.bonds.push_back(bond);
            } else {
                event.bond_energy_released -= bond.energy * 0.5f;
                event.bonds_broken++;
            }
        }
    }

    return event;
}
