// Auto-generated from pillars.yaml -- DO NOT EDIT
// Regenerate with: python scripts/generate_pillar_sources.py
#pragma once
#include "PillarEnum.h"

// PID constraint: min/max Bloch values and allowed rotation targets per pillar
struct PIDConstraint {
    const char* name;
    float min_val;
    float max_val;
    int allowed_targets[16];
};

static constexpr int NO_TARGET = -1;

static constexpr PIDConstraint PID_CONSTRAINTS[16] = {
    {"awareness", 0.0f, 1.0f, {Distortion, NO_TARGET}},
    {"willpower", 0.0f, 1.0f, {Force, Resistance, Depth, NO_TARGET}},
    {"force", 0.0f, 1.0f, {Influence, Relation, NO_TARGET}},
    {"influence", 0.0f, 1.0f, {Relation, Attraction, NO_TARGET}},
    {"resistance", 0.0f, 1.0f, {Integrity, Cohesion, NO_TARGET}},
    {"integrity", 0.0f, 1.0f, {Cohesion, NO_TARGET}},
    {"cohesion", 0.0f, 1.0f, {Relation, Presence, NO_TARGET}},
    {"relation", 0.0f, 1.0f, {Cohesion, Attraction, NO_TARGET}},
    {"presence", 0.0f, 1.0f, {Influence, Warmth, NO_TARGET}},
    {"warmth", 0.0f, 1.0f, {Cohesion, Memory, NO_TARGET}},
    {"memory", 0.0f, 1.0f, {Relation, Depth, NO_TARGET}},
    {"attraction", 0.0f, 1.0f, {Force, Relation, NO_TARGET}},
    {"harm", 0.0f, 0.95f, {Integrity, Cohesion, NO_TARGET}},
    {"distortion", 0.0f, 0.85f, {Awareness, Memory, NO_TARGET}},
    {"flux", 0.0f, 1.0f, {Force, Influence, Attraction, Depth, NO_TARGET}},
    {"depth", 0.0f, 1.0f, {Warmth, Memory, NO_TARGET}}
};

static inline bool is_valid_pillar_target(int operator_pillar, int target_pillar) {
    if (operator_pillar < 0 || operator_pillar >= 16) return false;
    if (target_pillar < 0 || target_pillar >= 16) return false;
    for (int i = 0; i < 16; i++) {
        int t = PID_CONSTRAINTS[operator_pillar].allowed_targets[i];
        if (t == NO_TARGET) break;
        if (t == target_pillar) return true;
    }
    return false;
}
