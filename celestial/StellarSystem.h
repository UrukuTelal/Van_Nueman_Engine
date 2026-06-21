#pragma once

#include "../voxel/VoxelCell.h"
#include "../voxel/BCCIndex.h"
#include "../include/Entity.h"
#include "../include/Transform.h"
#include "PlanetarySystem.h"
#include <vector>
#include <cmath>
#include <cstdint>

struct FusionConfig {
    float critical_force;
    float critical_warmth;
    float critical_density;
    float ignition_pressure;
    float fusion_energy_per_tick;
    float fuel_consumption_rate;

    FusionConfig()
        : critical_force(0.85f), critical_warmth(0.9f), critical_density(3.0f),
          ignition_pressure(0.8f), fusion_energy_per_tick(100.0f),
          fuel_consumption_rate(0.01f) {}
};

enum class StellarState : uint32_t {
    Protostar = 0,
    MainSequence = 1,
    RedGiant = 2,
    WhiteDwarf = 3,
    NeutronStar = 4,
    Supernova = 5,
    BlackHole = 6
};

inline const char* stellar_state_name(StellarState s) {
    switch (s) {
        case StellarState::Protostar: return "Protostar";
        case StellarState::MainSequence: return "MainSequence";
        case StellarState::RedGiant: return "RedGiant";
        case StellarState::WhiteDwarf: return "WhiteDwarf";
        case StellarState::NeutronStar: return "NeutronStar";
        case StellarState::Supernova: return "Supernova";
        case StellarState::BlackHole: return "BlackHole";
        default: return "Unknown";
    }
}

struct StellarBody {
    BCCCoord center;
    float total_mass;
    float core_temperature;
    float core_pressure;
    float remaining_fuel;
    float age;
    float radius;
    StellarState state;
    FusionConfig config;

    std::vector<VoxelCell> core_cells;
    std::vector<VoxelCell> envelope_cells;

    StellarBody()
        : center({0,0,0,0}), total_mass(0), core_temperature(0),
          core_pressure(0), remaining_fuel(1.0f), age(0),
          radius(1.0f), state(StellarState::Protostar) {}
};

inline float compute_core_force(const std::vector<VoxelCell>& cells) {
    float total_force = 0.0f;
    int count = 0;
    for (const auto& c : cells) {
        if (!c.active) continue;
        for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
            total_force += c.pyramids[f].material_composition[Force].to_float();
        }
        count++;
    }
    return count > 0 ? total_force / (count * VoxelCell::FACE_COUNT) : 0.0f;
}

inline float compute_core_warmth(const std::vector<VoxelCell>& cells) {
    float total = 0.0f;
    int count = 0;
    for (const auto& c : cells) {
        if (!c.active) continue;
        for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
            total += c.pyramids[f].material_composition[Warmth].to_float();
        }
        count++;
    }
    return count > 0 ? total / (count * VoxelCell::FACE_COUNT) : 0.0f;
}

inline float compute_core_density(const std::vector<VoxelCell>& cells) {
    float total = 0.0f;
    int count = 0;
    for (const auto& c : cells) {
        if (!c.active) continue;
        total += voxel_density(c);
        count++;
    }
    return count > 0 ? total / count : 0.0f;
}

inline bool check_ignition(const StellarBody& star) {
    float force = compute_core_force(star.core_cells);
    float warmth = compute_core_warmth(star.core_cells);
    float density = compute_core_density(star.core_cells);
    return force >= star.config.critical_force &&
           warmth >= star.config.critical_warmth &&
           density >= star.config.critical_density;
}

inline float fusion_energy_output(const StellarBody& star) {
    if (star.state != StellarState::MainSequence) return 0.0f;
    float force = compute_core_force(star.core_cells);
    float warmth = compute_core_warmth(star.core_cells);
    float density = compute_core_density(star.core_cells);
    float efficiency = (force + warmth + density / 3.0f) / 3.0f;
    return star.config.fusion_energy_per_tick * efficiency * star.remaining_fuel;
}

inline void stellar_evolution_tick(StellarBody& star, float dt) {
    if (star.core_cells.empty()) return;

    star.age += dt;
    float core_force = compute_core_force(star.core_cells);
    float core_warmth = compute_core_warmth(star.core_cells);
    float core_density = compute_core_density(star.core_cells);
    star.core_temperature = core_warmth * 10000.0f;
    star.core_pressure = core_force * core_density;
    star.total_mass = 0.0f;
    for (const auto& c : star.core_cells) star.total_mass += voxel_mass(c);
    for (const auto& c : star.envelope_cells) star.total_mass += voxel_mass(c);

    switch (star.state) {
        case StellarState::Protostar:
            if (check_ignition(star)) {
                star.state = StellarState::MainSequence;
                star.core_temperature = 15000000.0f;
            }
            break;

        case StellarState::MainSequence:
            star.remaining_fuel -= star.config.fuel_consumption_rate * dt;
            if (star.remaining_fuel <= 0.0f) {
                if (star.total_mass > 5.0f) {
                    star.state = StellarState::Supernova;
                } else if (star.total_mass > 1.5f) {
                    star.state = StellarState::NeutronStar;
                } else {
                    star.state = StellarState::RedGiant;
                }
            }
            if (star.age > 50.0f && star.state == StellarState::MainSequence) {
                star.state = StellarState::RedGiant;
                star.radius *= 10.0f;
            }
            break;

        case StellarState::RedGiant:
            if (star.total_mass > 8.0f) {
                star.state = StellarState::Supernova;
            } else {
                star.state = StellarState::WhiteDwarf;
                star.core_temperature *= 0.1f;
                star.radius *= 0.01f;
            }
            break;

        case StellarState::WhiteDwarf:
            star.core_temperature *= (1.0f - 0.01f * dt);
            break;

        case StellarState::Supernova:
            if (star.total_mass > 10.0f) {
                star.state = StellarState::BlackHole;
                star.radius = 0.0f;
            } else {
                star.state = StellarState::NeutronStar;
                star.radius *= 0.1f;
            }
            break;

        case StellarState::NeutronStar:
        case StellarState::BlackHole:
            break;
    }
}

inline void ignite_star(StellarBody& star) {
    for (auto& cell : star.core_cells) {
        for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
            PillarStateVector subject;
            for (uint32_t p = 0; p < NumPillars; p++)
                subject[p] = cell.pyramids[f].material_composition[p];

            PillarStateVector actor;
            actor.fill(vn::fp20_t(1.0f));
            actor[Harm] = vn::fp20_t(0.0f);

            TransformResult tr = transform_operation(
                actor, subject, Force, Warmth, vn::fp20_t(0.5f));

            for (uint32_t p = 0; p < NumPillars; p++) {
                cell.pyramids[f].material_composition[p] = vn::fp20_t(0.5f + 0.5f * tr.alignment);
            }
            cell.pyramids[f].material_composition[Warmth] = vn::fp20_t(1.0f);
        }
    }
    star.state = StellarState::MainSequence;
}
