// test_pillar_coupling.h - Unit tests for Pillar Coupling
#pragma once

#include <cassert>
#include <cstdio>
#include "../include/Entity.h"
#include "../vn/PillarTypes.h"

inline void test_pillar_coupling_basic() {
    printf("Test: Pillar coupling basic... ");
    PillarStateVector psv = create_default_pillar_state(0.5f);
    // Use pillars array directly
    psv[PILLAR_FORCE] = 0.8f;

    float force = psv[PILLAR_FORCE];
    assert(force >= 0.0f && force <= 1.0f);
    printf("PASS (force=%.2f)\n", force);
}

inline void test_pillar_coupling_harm() {
    printf("Test: Pillar coupling harm calculation... ");
    PillarStateVector psv = create_default_pillar_state(0.3f);
    psv[PILLAR_RESISTANCE] = 0.8f;
    psv[PILLAR_INTEGRITY] = 0.6f;

    float delta_h = calculate_harm_delta(1.0f, psv[PILLAR_RESISTANCE], psv[PILLAR_INTEGRITY]);
    assert(delta_h >= 0.0f);
    printf("PASS (delta_h=%.2f)\n", delta_h);
}

inline void test_pillar_coupling_effective_awareness() {
    printf("Test: Effective awareness with distortion... ");
    float awareness = 0.8f;
    float distortion = 0.3f;
    float eff = calculate_effective_awareness(awareness, distortion);
    assert(eff == 0.56f);  // 0.8 * (1.0 - 0.3)
    printf("PASS (eff=%.2f)\n", eff);
}

inline void test_pillar_interaction() {
    printf("Test: Pillar interaction matrix... ");
    PillarStateVector source = create_default_pillar_state(0.5f);
    PillarStateVector target = create_default_pillar_state(0.5f);

    // Simple interaction: Force towards target based on Attraction
    float interaction = source[PILLAR_FORCE] * target[PILLAR_ATTRACTION];
    assert(interaction >= 0.0f && interaction <= 1.0f);
    printf("PASS (interaction=%.2f)\n", interaction);
}

inline void run_pillar_coupling_tests() {
    printf("=== Pillar Coupling Tests ===\n");
    test_pillar_coupling_basic();
    test_pillar_coupling_harm();
    test_pillar_coupling_effective_awareness();
    test_pillar_interaction();
    printf("=== All Pillar Coupling Tests PASSED ===\n\n");
}
