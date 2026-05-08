// test_entity.h - Unit tests for Entity.h

#pragma once

#include <cassert>
#include <cstdio>
#include "../include/Entity.h"

// Test PillarStateVector creation
inline void test_pillar_state_vector() {
    printf("Test: PillarStateVector creation... ");
    PillarStateVector psv = create_default_pillar_state(0.5f);
    for (int i = 0; i < NUM_PILLARS; i++) {
        assert(psv[i] == 0.5f);
    }
    printf("PASS\n");
}

// Test harm delta calculation
inline void test_harm_delta() {
    printf("Test: Harm delta calculation... ");
    float delta = calculate_harm_delta(1.0f, 0.3f, 0.2f);
    assert(delta == 0.5f);  // 1.0 - 0.3 - 0.2 = 0.5
    printf("PASS (delta=%.2f)\n", delta);
}

// Test effective awareness
inline void test_effective_awareness() {
    printf("Test: Effective awareness... ");
    float eff = calculate_effective_awareness(0.8f, 0.3f);
    assert(eff == 0.56f);  // 0.8 * (1.0 - 0.3) = 0.56
    printf("PASS (eff=%.2f)\n", eff);
}

// Run all entity tests
inline void run_entity_tests() {
    printf("=== Entity Tests ===\n");
    test_pillar_state_vector();
    test_harm_delta();
    test_effective_awareness();
    printf("=== All Entity Tests PASSED ===\n\n");
}
