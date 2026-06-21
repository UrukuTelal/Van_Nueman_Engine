// test_entity.h - Unit tests for Entity.h (Bloch rotation based)
// Part of the COSMOS 16D Pillar Cosmology implementation
// Phase 1: Core TRANSFORM Algorithm & Bloch Rotation

#pragma once

#include <cassert>
#include <cstdio>
#include <cmath>
#include "../include/Entity.h"
#include "../include/TransformCore.h"

// Test PillarStateVector creation
inline void test_pillar_state_vector() {
    printf("Test: PillarStateVector creation... ");
    PillarStateVector psv = create_default_pillar_state(vn::fp20_t(0.5f));
    for (int i = 0; i < NumPillars; i++) {
        assert(std::abs(vn::fp20_t(psv[i]).to_float() - 0.5f) < 0.001f);
    }
    printf("PASS\n");
}

// Test Bloch conversion helpers
inline void test_bloch_conversion() {
    printf("Test: Bloch value<->theta conversion... ");
    vn::fp20_t val(0.5f);
    vn::fp20_t theta = pillar_value_to_theta(val);
    assert(std::abs(theta.to_float() - 1.57079633f) < 0.001f);  // pi/2
    vn::fp20_t back = theta_to_pillar_value(theta);
    assert(std::abs(back.to_float() - 0.5f) < 0.001f);
    
    val = vn::fp20_t(1.0f);
    theta = pillar_value_to_theta(val);
    assert(std::abs(theta.to_float()) < 0.001f);  // north pole = 0
    back = theta_to_pillar_value(theta);
    assert(std::abs(back.to_float() - 1.0f) < 0.001f);
    
    val = vn::fp20_t(0.0f);
    theta = pillar_value_to_theta(val);
    assert(std::abs(theta.to_float() - 3.14159265f) < 0.001f);  // south pole = pi
    back = theta_to_pillar_value(theta);
    assert(std::abs(back.to_float() - 0.0f) < 0.001f);
    
    printf("PASS\n");
}

// Test Bloch rotation
inline void test_bloch_rotation() {
    printf("Test: Bloch rotation... ");
    // Rotating from 0.5 (equator, theta=pi/2) by +0.1 rad
    vn::fp20_t result = apply_bloch_rotation(vn::fp20_t(0.5f), vn::fp20_t(0.1f));
    float expected_theta = 1.57079633f + 0.1f;
    float expected = (std::cos(expected_theta) + 1.0f) * 0.5f;
    assert(std::abs(result.to_float() - expected) < 0.001f);
    // Should be less than 0.5 (rotated toward south pole)
    assert(result < vn::fp20_t(0.5f));
    
    // Rotating toward north pole (negative delta_theta)
    result = apply_bloch_rotation(vn::fp20_t(0.5f), vn::fp20_t(-0.1f));
    assert(result > vn::fp20_t(0.5f));
    printf("PASS\n");
}

// Test Harm angular shift (Bloch rotation, not scalar subtraction)
inline void test_harm_angular_shift() {
    printf("Test: Harm angular shift (Bloch)... ");
    vn::fp20_t incoming(1.0f);
    vn::fp20_t harm(0.5f);
    vn::fp20_t willpower(0.5f);
    vn::fp20_t depth(0.5f);
    
    vn::fp20_t shift = compute_harm_angular_shift(incoming, harm, willpower, depth);
    // Δθ = (1.0 * 0.5) / (0.5 * 0.5) = 0.5 / 0.25 = 2.0
    assert(std::abs(shift.to_float() - 2.0f) < 0.001f);
    printf("PASS (shift=%.2f rad)\n", shift.to_float());
}

// Test Distortion torsion (phi twist, not scalar dampening)
inline void test_distortion_torsion() {
    printf("Test: Distortion torsion (Bloch rotation)... ");
    vn::fp20_t awareness(0.8f);
    vn::fp20_t distortion(0.3f);
    vn::fp20_t result = apply_distortion_torsion(awareness, distortion);
    // theta = acos(2*0.8 - 1) = acos(0.6) ≈ 0.9273
    // twist = 0.3 * pi ≈ 0.9425
    // new_theta = 0.9273 + 0.9425 = 1.8698 (< pi, no wrap)
    // result = (cos(1.8698) + 1) / 2 ≈ ( -0.295 + 1 ) / 2 ≈ 0.353
    assert(std::abs(result.to_float() - 0.353f) < 0.01f);
    // Result should NOT equal the old scalar dampener (0.56)
    assert(std::abs(result.to_float() - 0.56f) > 0.1f);
    printf("PASS (eff=%.3f, Bloch rotation)\n", result.to_float());
}

// Test full TRANSFORM with Depth buffer
inline void test_transform_bloch() {
    printf("Test: TRANSFORM with Depth buffer... ");
    PillarStateVector subject = create_default_pillar_state(vn::fp20_t(0.5f));
    subject[Integrity] = vn::fp20_t(0.8f);
    subject[Willpower] = vn::fp20_t(0.6f);
    
    vn::fp20_t depth(0.5f);
    vn::fp20_t constraint(0.0f);
    
    vn::fp20_t applied = transform_bloch(subject, vn::fp20_t(0.9f), vn::fp20_t(0.7f), Integrity, depth, constraint);
    
    // willpower_theta = acos(2*0.6-1) ≈ 1.369 rad
    // angular_capacity = pi - willpower_theta ≈ 1.772 rad
    // shift_abs = 2.1 > 1.772, so applied = angular_capacity = 1.772
    // excess = 2.1 - 1.772 = 0.328, depth_cost = 0.328 * 0.5 = 0.164
    // depth = 0.5 - 0.164 = 0.336
    assert(std::abs(applied.to_float() - 1.772f) < 0.02f);
    assert(std::abs(depth.to_float() - 0.336f) < 0.02f);
    assert(std::abs(constraint.to_float() - 0.0f) < 0.001f);
    
    // Integrity should have been rotated by ~1.77 rad
    assert(vn::fp20_t(subject[Integrity]) < vn::fp20_t(0.8f));
    printf("PASS (integrity=%.3f, depth=%.3f, constraint=%.3f)\n", 
           subject[Integrity], depth.to_float(), constraint.to_float());
}

// Run all entity tests
inline void run_entity_tests() {
    printf("=== Entity Tests (Bloch Rotation) ===\n");
    test_pillar_state_vector();
    test_bloch_conversion();
    test_bloch_rotation();
    test_harm_angular_shift();
    test_distortion_torsion();
    test_transform_bloch();
    printf("=== All Entity Tests PASSED ===\n\n");
}
