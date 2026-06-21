#pragma once

#include "Entity.h"
#include "BlochMath.h"
#include "BlochSpace.h"
#include <cmath>
#include <cstring>

// ── Angular shift computation ────────────────────────────────
// All parameters in fp20_t (deterministic fixed-point).
// No float conversion — keeps the simulation deterministic.

struct ShiftResult {
    static constexpr bool _bloch_audited = true;

    vn::fp20_t shift_rad;
    vn::fp20_t depth_consumed;
    vn::fp20_t constraint_delta;
    vn::fp20_t excess_rad;
};
static_assert(std::is_standard_layout<ShiftResult>::value,
    "ShiftResult must be standard layout for Bloch-space determinism");
static_assert(ShiftResult::_bloch_audited,
    "ERROR: ShiftResult is not Bloch-space audited. "
    "Define 'static constexpr bool _bloch_audited = true;' to confirm "
    "all members use vn::fp20_t instead of float/double.");

inline ShiftResult compute_angular_shift(
    vn::fp20_t operator_value,
    vn::fp20_t influence,
    vn::fp20_t alignment,
    vn::fp20_t willpower,
    vn::fp20_t depth_buffer
) {
    ShiftResult r = {};
    vn::fp20_t zero;
    vn::fp20_t one_half(0.5f);
    vn::fp20_t pi(3.14159265f);
    vn::fp20_t neg_pi = zero - pi;
    vn::fp20_t epsilon(1.0e-8f);

    vn::fp20_t effective_alignment = (alignment + vn::fp20_t(1.0f)) * one_half;
    vn::fp20_t operator_mag = operator_value * influence * effective_alignment;
    vn::fp20_t resistance = willpower * depth_buffer;
    if (resistance < epsilon) resistance = epsilon;

    vn::fp20_t raw_shift = operator_mag / resistance;
    if (pi < raw_shift) raw_shift = pi;
    if (raw_shift < neg_pi) raw_shift = neg_pi;

    vn::fp20_t shift_abs = raw_shift < zero ? zero - raw_shift : raw_shift;
    vn::fp20_t willpower_theta = bloch_value_to_theta(willpower);
    vn::fp20_t angular_capacity = pi - willpower_theta;

    r.shift_rad = raw_shift;

    if (angular_capacity < shift_abs) {
        r.shift_rad = raw_shift < zero ? zero - angular_capacity : angular_capacity;
        vn::fp20_t excess = shift_abs - angular_capacity;
        r.excess_rad = excess;

        vn::fp20_t depth_cost = excess * one_half;

        if (depth_buffer - depth_cost <= zero) {
            vn::fp20_t remaining = depth_cost - depth_buffer;
            r.depth_consumed = depth_buffer;
            r.constraint_delta = remaining;
        } else {
            r.depth_consumed = depth_cost;
            r.constraint_delta = zero;
        }
    }

    return r;
}

// ── Apply Bloch rotation with willpower half-damping ─────────

// ── Thin wrappers for backward compatibility ──
// These match the old Entity.h signatures (deleted from Entity.h lines 172-247)
// and delegate to the canonical compute_angular_shift.

inline vn::fp20_t compute_harm_angular_shift(
    vn::fp20_t incoming_force,
    vn::fp20_t operator_magnitude,
    vn::fp20_t willpower,
    vn::fp20_t depth_buffer
) {
    // Legacy formula: no alignment factor, no angular capacity check.
    // Used specifically for Harm/Deformation calculations.
    vn::fp20_t zero;
    vn::fp20_t epsilon(1.0e-8f);
    vn::fp20_t pi(3.14159265f);
    vn::fp20_t neg_pi = zero - pi;

    vn::fp20_t resistance = willpower * depth_buffer;
    if (resistance < epsilon) resistance = epsilon;

    vn::fp20_t raw_shift = (incoming_force * operator_magnitude) / resistance;
    if (pi < raw_shift) raw_shift = pi;
    if (raw_shift < neg_pi) raw_shift = neg_pi;

    return raw_shift;
}

inline vn::fp20_t apply_harm_rotation(
    PillarStateVector& subject,
    vn::fp20_t incoming_force,
    vn::fp20_t& depth_buffer,
    vn::fp20_t& constraint_level,
    uint32_t target_idx
) {
    vn::fp20_t harm = vn::fp20_t(subject[Harm]);
    vn::fp20_t willpower = vn::fp20_t(subject[Willpower]);
    vn::fp20_t shift = compute_harm_angular_shift(incoming_force, harm, willpower, depth_buffer);
    vn::fp20_t eps(1e-8f);
    if (!(constraint_level < eps)) shift = shift * vn::fp20_t(0.5f);
    subject[target_idx] = bloch_rotate(vn::fp20_t(subject[target_idx]), shift);
    return shift;
}

// ── transform_bloch overloads ──
// Applies angular shift to a target pillar with depth/constraint management.
// 7-arg version: cross-entity transform with explicit alignment.
// 6-arg version: self-transform (alignment = 1.0, entity acting on itself).
// NOTE: 7-arg must be defined before 6-arg since 6-arg delegates to it.

inline vn::fp20_t transform_bloch(
    PillarStateVector& subject,
    vn::fp20_t operator_value,
    vn::fp20_t influence,
    uint32_t target_idx,
    vn::fp20_t& depth_buffer,
    vn::fp20_t& constraint_level,
    vn::fp20_t alignment
) {
    vn::fp20_t willpower = vn::fp20_t(subject[Willpower]);
    ShiftResult result = compute_angular_shift(operator_value, influence, alignment, willpower, depth_buffer);
    depth_buffer = depth_buffer - result.depth_consumed;
    constraint_level = constraint_level + result.constraint_delta;
    subject[target_idx] = bloch_rotate(vn::fp20_t(subject[target_idx]), result.shift_rad);
    return result.shift_rad;
}

inline vn::fp20_t transform_bloch(
    PillarStateVector& subject,
    vn::fp20_t operator_value,
    vn::fp20_t influence,
    uint32_t target_idx,
    vn::fp20_t& depth_buffer,
    vn::fp20_t& constraint_level
) {
    // Self-transform: alignment = 1.0 (entity acting on itself implies perfect alignment)
    return transform_bloch(subject, operator_value, influence, target_idx, depth_buffer, constraint_level, vn::fp20_t(1.0f));
}

inline vn::fp20_t apply_transform_rotation(
    vn::fp20_t current_value,
    vn::fp20_t shift_rad,
    bool is_willpower_target
) {
    vn::fp20_t apply = is_willpower_target ? shift_rad * vn::fp20_t(0.5f) : shift_rad;
    return bloch_rotate(current_value, apply);
}
