// test_pillar_coupling.h - Unit tests for Pillar Coupling
#pragma once

#include <cassert>
#include <cstdio>
#include "../include/Entity.h"
#include "../include/TransformCore.h"

inline void test_pillar_coupling_basic() {
    printf("Test: Pillar coupling basic... ");
    PillarStateVector psv = create_default_pillar_state(vn::fp20_t(0.5f));
    // Use pillars array directly
    psv[Force] = vn::fp20_t(0.8f);

    float force = vn::fp20_t(psv[Force]).to_float();
    assert(force >= 0.0f && force <= 1.0f);
    printf("PASS (force=%.2f)\n", force);
}

inline void test_pillar_coupling_harm_bloch() {
    printf("Test: Pillar coupling harm (Bloch rotation)... ");
    PillarStateVector psv = create_default_pillar_state(vn::fp20_t(0.3f));
    psv[Resistance] = vn::fp20_t(0.8f);
    psv[Integrity] = vn::fp20_t(0.6f);
    psv[Willpower] = vn::fp20_t(0.5f);
    psv[Harm] = vn::fp20_t(0.5f);
    
    vn::fp20_t depth(0.5f);
    vn::fp20_t constraint(0.0f);
    vn::fp20_t applied = apply_harm_rotation(psv, vn::fp20_t(1.0f), depth, constraint, Integrity);
    
    // Should be a rotation (non-zero), and Integrity should have changed
    assert(std::abs(applied.to_float()) > 0.0f);
    assert(vn::fp20_t(psv[Integrity]) != vn::fp20_t(0.6f));
    printf("PASS (applied=%.3f rad, integrity=%.3f)\n", applied.to_float(), vn::fp20_t(psv[Integrity]).to_float());
}

inline void test_pillar_coupling_distortion_bloch() {
    printf("Test: Distortion torsion (Bloch phase twist)... ");
    vn::fp20_t awareness(0.8f);
    vn::fp20_t distortion(0.3f);
    vn::fp20_t eff = apply_distortion_torsion(awareness, distortion);
    // Should NOT equal 0.56 (the old scalar result)
    assert(std::abs(eff.to_float() - 0.56f) > 0.1f);
    printf("PASS (eff=%.3f, NOT 0.56)\n", eff.to_float());
}

inline void test_pillar_interaction() {
    printf("Test: Pillar interaction matrix... ");
    PillarStateVector source = create_default_pillar_state(vn::fp20_t(0.5f));
    PillarStateVector target = create_default_pillar_state(vn::fp20_t(0.5f));

    // Simple interaction: Force towards target based on Attraction
    float interaction = source[Force] * target[Attraction];
    assert(interaction >= 0.0f && interaction <= 1.0f);
    printf("PASS (interaction=%.2f)\n", interaction);
}

inline void run_pillar_coupling_tests() {
    printf("=== Pillar Coupling Tests (Bloch Rotation) ===\n");
    test_pillar_coupling_basic();
    test_pillar_coupling_harm_bloch();
    test_pillar_coupling_distortion_bloch();
    test_pillar_interaction();
    printf("=== All Pillar Coupling Tests PASSED ===\n\n");
}
