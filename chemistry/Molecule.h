#pragma once

#include "ChemicalBond.h"
#include "../voxel/VoxelCell.h"
#include "../voxel/BCCIndex.h"
#include "../include/Entity.h"
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <queue>

struct Molecule {
    std::vector<VoxelCell> atoms;
    std::vector<ChemicalBond> bonds;
    uint64_t molecule_id;

    Molecule() : molecule_id(0) {}

    void add_atom(const VoxelCell& atom) { atoms.push_back(atom); }

    uint64_t atom_key(uint32_t index) const {
        return bcc_hash(atoms[index].coord);
    }

    void add_bond(const ChemicalBond& bond) {
        for (const auto& b : bonds) {
            if (b.atom_a_key == bond.atom_a_key && b.atom_b_key == bond.atom_b_key) return;
            if (b.atom_b_key == bond.atom_a_key && b.atom_a_key == bond.atom_b_key) return;
        }
        bonds.push_back(bond);
    }

    std::vector<uint32_t> get_bonded_atoms(uint32_t atom_index) const {
        std::vector<uint32_t> result;
        uint64_t key = atom_key(atom_index);
        for (size_t i = 0; i < bonds.size(); i++) {
            if (bonds[i].atom_a_key == key) {
                for (size_t j = 0; j < atoms.size(); j++) {
                    if (atom_key(j) == bonds[i].atom_b_key) {
                        result.push_back(static_cast<uint32_t>(j));
                        break;
                    }
                }
            } else if (bonds[i].atom_b_key == key) {
                for (size_t j = 0; j < atoms.size(); j++) {
                    if (atom_key(j) == bonds[i].atom_a_key) {
                        result.push_back(static_cast<uint32_t>(j));
                        break;
                    }
                }
            }
        }
        return result;
    }

    bool is_connected() const {
        if (atoms.empty()) return false;
        if (atoms.size() == 1) return true;

        std::unordered_set<uint32_t> visited;
        std::queue<uint32_t> q;
        q.push(0);
        visited.insert(0);

        while (!q.empty()) {
            uint32_t current = q.front(); q.pop();
            auto neighbors = get_bonded_atoms(current);
            for (auto n : neighbors) {
                if (visited.count(n) == 0) {
                    visited.insert(n);
                    q.push(n);
                }
            }
        }
        return visited.size() == atoms.size();
    }

    uint32_t atom_count() const { return static_cast<uint32_t>(atoms.size()); }
    uint32_t bond_count() const { return static_cast<uint32_t>(bonds.size()); }

    float total_bond_energy() const {
        float total = 0.0f;
        for (const auto& b : bonds) total += b.energy;
        return total;
    }

    float polarity() const {
        float total = 0.0f;
        for (const auto& b : bonds) {
            if (b.type == BondType::PolarCovalent || b.type == BondType::Ionic) {
                total += b.strength;
            }
        }
        return total / static_cast<float>(bonds.size() > 0 ? bonds.size() : 1);
    }

    vn::fp20_t avg_pillar(uint32_t pillar_index) const {
        vn::fp20_t sum(0);
        for (const auto& atom : atoms) {
            for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
                sum = sum + atom.pyramids[f].material_composition[pillar_index];
            }
        }
        float count = static_cast<float>(atoms.size() * VoxelCell::FACE_COUNT);
        return count > 0 ? sum / vn::fp20_t(count) : vn::fp20_t(0.5f);
    }

    void clear() { atoms.clear(); bonds.clear(); molecule_id = 0; }
};
