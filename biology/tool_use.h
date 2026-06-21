#pragma once

#include "../voxel/VoxelCell.h"
#include "../voxel/BCCIndex.h"
#include "../voxel/YieldMatrix.h"
#include "../include/Entity.h"
#include "../include/Transform.h"
#include "voxel_organism.h"
#include <vector>
#include <unordered_map>
#include <cmath>
#include <cstdint>

struct ToolAction {
    enum Type {
        PUSH = 0,
        PULL = 1,
        BREAK = 2,
        JOIN = 3,
        SHAPE = 4,
        COUNT = 5
    };
    Type type;
    float force_applied;
    BCCCoord target_coord;

    ToolAction() : type(PUSH), force_applied(0.0f), target_coord({0,0,0,0}) {}
};

struct ToolResult {
    bool success;
    float energy_cost;
    float pillar_change;
    BCCCoord new_position;
    bool bond_broken;
    bool bond_formed;

    ToolResult() : success(false), energy_cost(0), pillar_change(0),
                   new_position({0,0,0,0}), bond_broken(false), bond_formed(false) {}
};

inline float force_available(const VoxelOrganism& org) {
    const auto& psv = org.pillar_state();
    return psv[Force] +
           psv[Willpower] * 0.5f;
}

inline float energy_cost_for_action(ToolAction::Type type, float force) {
    switch (type) {
        case ToolAction::PUSH:   return force * 0.3f;
        case ToolAction::PULL:   return force * 0.35f;
        case ToolAction::BREAK:  return force * 0.8f;
        case ToolAction::JOIN:   return force * 0.6f;
        case ToolAction::SHAPE:  return force * 0.5f;
        default: return force;
    }
}

inline bool can_perform_action(const VoxelOrganism& org, const ToolAction& action) {
    float available = force_available(org);
    float cost = energy_cost_for_action(action.type, action.force_applied);
    return available >= cost;
}

inline ToolResult push_voxel(VoxelCell& target, BCCCoord direction,
                              float force, const VoxelOrganism& org) {
    ToolResult result;
    const auto& psv = org.pillar_state();
    float available = force_available(org);
    if (available < force) { result.success = false; return result; }

    float attraction = psv[Attraction];
    BCCCoord dir = direction;
    float dist = sqrtf(static_cast<float>(dir.i * dir.i + dir.j * dir.j + dir.k * dir.k));
    if (dist < 0.5f) { result.success = true; return result; }

    float inv_dist = 1.0f / dist;
    BCCCoord step;
    step.i = static_cast<int32_t>(dir.i * inv_dist + 0.5f);
    step.j = static_cast<int32_t>(dir.j * inv_dist + 0.5f);
    step.k = static_cast<int32_t>(dir.k * inv_dist + 0.5f);
    step.l = (step.i + step.j + step.k) & 1;

    float resistance = 0.0f;
    for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
        resistance += target.pyramids[f].material_composition[Resistance].to_float();
    }
    resistance /= VoxelCell::FACE_COUNT;

    if (force * attraction > resistance) {
        target.coord.i += step.i;
        target.coord.j += step.j;
        target.coord.k += step.k;
        target.coord.l = (target.coord.i + target.coord.j + target.coord.k) & 1;
        result.success = true;
        result.new_position = target.coord;
    }

    result.energy_cost = energy_cost_for_action(ToolAction::PULL, force);
    return result;
}

inline ToolResult break_bond(VoxelCell& a, VoxelCell& b,
                              float force, const VoxelOrganism& org) {
    ToolResult result;
    const auto& psv = org.pillar_state();
    float available = force_available(org);
    if (available < force) { result.success = false; return result; }

    float bond_integrity = 0.0f;
    for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
        bond_integrity += a.pyramids[f].material_composition[Integrity].to_float() +
                          b.pyramids[f].material_composition[Cohesion].to_float();
    }
    bond_integrity /= VoxelCell::FACE_COUNT * 2;

    float harm_applied = psv[Harm];
    float break_force = force * (1.0f + harm_applied);

    if (break_force > bond_integrity * 2.0f) {
        a.active = false;
        b.active = false;
        result.success = true;
        result.bond_broken = true;
        result.pillar_change = -bond_integrity * 0.3f;
    }

    result.energy_cost = energy_cost_for_action(ToolAction::BREAK, force);
    return result;
}

inline ToolResult join_voxels(VoxelCell& a, VoxelCell& b,
                               float force, const VoxelOrganism& org) {
    ToolResult result;
    const auto& psv = org.pillar_state();
    float available = force_available(org);
    if (available < force) { result.success = false; return result; }

    float attraction = psv[Attraction];
    float cohesion = psv[Cohesion];
    float join_potential = (attraction + cohesion) * 0.5f;

    float existing_separation = 0.0f;
    int32_t di = a.coord.i - b.coord.i;
    int32_t dj = a.coord.j - b.coord.j;
    int32_t dk = a.coord.k - b.coord.k;
    existing_separation = sqrtf(static_cast<float>(di*di + dj*dj + dk*dk));

    if (existing_separation > 1.5f && join_potential > 0.3f) {
        BCCCoord midpoint;
        midpoint.i = (a.coord.i + b.coord.i) / 2;
        midpoint.j = (a.coord.j + b.coord.j) / 2;
        midpoint.k = (a.coord.k + b.coord.k) / 2;
        midpoint.l = (midpoint.i + midpoint.j + midpoint.k) & 1;
        a.coord = midpoint;
        result.success = true;
        result.bond_formed = true;
    } else if (existing_separation <= 1.5f && join_potential > 0.5f) {
        for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
            for (uint32_t p = 0; p < NumPillars; p++) {
                float avg = (a.pyramids[f].material_composition[p].to_float() +
                             b.pyramids[f].material_composition[p].to_float()) * 0.5f;
                a.pyramids[f].material_composition[p] = vn::fp20_t(avg);
                b.pyramids[f].material_composition[p] = vn::fp20_t(avg);
            }
        }
        result.success = true;
        result.bond_formed = true;
    }

    result.energy_cost = energy_cost_for_action(ToolAction::JOIN, force);
    return result;
}

inline ToolResult shape_voxel(VoxelCell& target, const YieldMatrix& target_material,
                               float force, const VoxelOrganism& org) {
    ToolResult result;
    const auto& psv = org.pillar_state();
    float available = force_available(org);
    if (available < force) { result.success = false; return result; }

    float shaping_power = force * psv[Willpower];
    float current_integrity = 0.0f;
    for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
        for (uint32_t p = 0; p < NumPillars; p++) {
            current_integrity += target.pyramids[f].material_composition[p].to_float();
        }
    }
    current_integrity /= VoxelCell::FACE_COUNT * NumPillars;

    if (shaping_power > current_integrity) {
        for (uint32_t f = 0; f < VoxelCell::FACE_COUNT; f++) {
            for (uint32_t p = 0; p < NumPillars; p++) {
                float blend = (target.pyramids[f].material_composition[p].to_float() * 0.3f +
                               target_material.pillar_to_vertex[p][0].to_float() * 0.7f);
                target.pyramids[f].material_composition[p] = vn::fp20_t(blend);
            }
        }
        result.success = true;
        result.pillar_change = -force * 0.2f;
    }

    result.energy_cost = energy_cost_for_action(ToolAction::SHAPE, force);
    return result;
}
