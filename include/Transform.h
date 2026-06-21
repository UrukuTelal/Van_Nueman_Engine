// Transform.h - Full TRANSFORM algorithm with Bloch rotation
// Part of the COSMOS 16D Pillar Cosmology implementation
// Phase 1: Core TRANSFORM Algorithm & Bloch Rotation
//
// Mirrors: DeveloperConsole/backend/models/transform.py
// Provides the C++ equivalent of the TRANSFORM algorithm with proper
// TransformResult struct, alignment calculation, Depth buffer handling,
// and vortex generation.

#pragma once

#include "Entity.h"
#include "TransformCore.h"
#include <cmath>
#include <cstring>
#include <cstdio>

// ── Transform Result ─────────────────────────────────────────
// Mirrors Python TransformResult dataclass.

struct TransformResult {
    vn::fp20_t alignment;
    int operator_pillar;
    int target_pillar;
    vn::fp20_t operator_value;
    vn::fp20_t influence;
    vn::fp20_t raw_angular_shift;
    vn::fp20_t willpower;
    vn::fp20_t depth_before;
    vn::fp20_t depth_after;
    vn::fp20_t delta_theta_applied;
    vn::fp20_t delta_theta_absorbed;
    vn::fp20_t target_pillar_before;
    vn::fp20_t target_pillar_after;
    vn::fp20_t constraint_level_before;
    vn::fp20_t constraint_level_after;
    vn::fp20_t vortex_presence_delta;
    vn::fp20_t vortex_influence_delta;
    bool is_constrained;
    
    void print() const {
        printf("TRANSFORM: alignment=%.3f op=%.3f inf=%.3f raw=%.3f "
               "will=%.3f depth=%.3f->%.3f applied=%.3f absorbed=%.3f "
               "target=%.3f->%.3f constraint=%.3f->%.3f vortex_p=%.4f vortex_i=%.4f %s\n",
               alignment.to_float(), operator_value.to_float(), influence.to_float(), raw_angular_shift.to_float(),
               willpower.to_float(), depth_before.to_float(), depth_after.to_float(),
               delta_theta_applied.to_float(), delta_theta_absorbed.to_float(),
               target_pillar_before.to_float(), target_pillar_after.to_float(),
               constraint_level_before.to_float(), constraint_level_after.to_float(),
               vortex_presence_delta.to_float(), vortex_influence_delta.to_float(),
               is_constrained ? "CONSTRAINED" : "");
    }
};

// ── Bloch Vector Dot Product ─────────────────────────────────
// Compute the 16D Bloch vector dot product between two PSVs.
// Returns normalized to [-1, 1] as fp20_t.

inline vn::fp20_t bloch_dot_product(const PillarStateVector& a, const PillarStateVector& b) {
    double total = 0.0;
    for (int i = 0; i < NumPillars; i++) {
        double theta_a = pillar_value_to_theta(vn::fp20_t(a[i])).to_double();
        double theta_b = pillar_value_to_theta(vn::fp20_t(b[i])).to_double();
        total += std::cos(theta_a - theta_b);
    }
    return vn::fp20_t(total / (double)NumPillars);
}

// ── TRANSFORM: Full Algorithm ────────────────────────────────
// Apply operator_pillar of actor to target_pillar of subject.
// Returns a full TransformResult with all intermediate values.

inline TransformResult transform_operation(
    const PillarStateVector& actor,
    PillarStateVector& subject,
    int operator_pillar,
    int target_pillar,
    vn::fp20_t constraint_level = vn::fp20_t(0.0f)
) {
    TransformResult result;
    std::memset(&result, 0, sizeof(result));
    result.operator_pillar = operator_pillar;
    result.target_pillar = target_pillar;
    
    result.alignment = bloch_dot_product(actor, subject);
    
    result.operator_value = vn::fp20_t(actor[operator_pillar]);
    result.influence = vn::fp20_t(actor[Influence]);
    result.willpower = vn::fp20_t(subject[Willpower]);
    result.depth_before = vn::fp20_t(subject[Depth]);
    result.target_pillar_before = vn::fp20_t(subject[target_pillar]);
    result.constraint_level_before = constraint_level;
    result.constraint_level_after = constraint_level;
    
    ShiftResult shift = compute_angular_shift(
        result.operator_value,
        result.influence,
        result.alignment,
        result.willpower,
        result.depth_before
    );
    
    result.raw_angular_shift = shift.shift_rad;
    result.delta_theta_applied = shift.shift_rad;
    result.delta_theta_absorbed = shift.excess_rad;
    result.depth_after = result.depth_before - shift.depth_consumed;
    result.constraint_level_after = constraint_level + shift.constraint_delta;
    vn::fp20_t one(1.0f);
    if (one < result.constraint_level_after) result.constraint_level_after = one;
    
    subject[target_pillar] = apply_transform_rotation(
        vn::fp20_t(subject[target_pillar]), shift.shift_rad, target_pillar == Willpower
    );
    result.target_pillar_after = subject[target_pillar];
    
    subject[Depth] = result.depth_after;
    
    vn::fp20_t presence = vn::fp20_t(actor[Presence]);
    vn::fp20_t applied = (target_pillar == Willpower)
        ? shift.shift_rad * vn::fp20_t(0.5f)
        : shift.shift_rad;
    vn::fp20_t apply_abs = applied < vn::fp20_t(0) ? vn::fp20_t() - applied : applied;
    result.vortex_presence_delta = apply_abs * presence * vn::fp20_t(0.1f);
    result.vortex_influence_delta = apply_abs * result.influence * vn::fp20_t(0.1f);
    
    result.is_constrained = (one <= result.constraint_level_after);
    
    return result;
}

// ── Convenience: apply_harm ──────────────────────────────────
// Actor's Harm (pillar 12) applied to Subject's Integrity (pillar 5).

inline TransformResult apply_harm_transform(
    const PillarStateVector& actor,
    PillarStateVector& subject,
    int target_pillar = Integrity,
    vn::fp20_t constraint_level = vn::fp20_t(0.0f)
) {
    return transform_operation(actor, subject, Harm, target_pillar, constraint_level);
}

inline TransformResult apply_distortion_transform(
    const PillarStateVector& actor,
    PillarStateVector& subject,
    int target_pillar = Awareness,
    vn::fp20_t constraint_level = vn::fp20_t(0.0f)
) {
    return transform_operation(actor, subject, Distortion, target_pillar, constraint_level);
}

inline TransformResult apply_restoration_transform(
    const PillarStateVector& actor,
    PillarStateVector& subject,
    int target_pillar = Integrity,
    vn::fp20_t constraint_level = vn::fp20_t(0.0f)
) {
    return transform_operation(actor, subject, Cohesion, target_pillar, constraint_level);
}
