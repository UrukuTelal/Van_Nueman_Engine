// test_transform.cpp - Tests for Transform.h (Bloch rotation TRANSFORM)
// Part of the COSMOS 16D Pillar Cosmology implementation
// Phase 1: Core TRANSFORM Algorithm & Bloch Rotation

#include <cassert>
#include <cstdio>
#include <cmath>
#include "../include/Transform.h"

// Test bloch_dot_product
inline void test_bloch_dot_product() {
    printf("Test: Bloch dot product... ");
    PillarStateVector a = create_default_pillar_state(vn::fp20_t(0.5f));
    PillarStateVector b = create_default_pillar_state(vn::fp20_t(0.5f));
    
    // Same PSV → alignment should be high
    float dot = bloch_dot_product(a, b);
    assert(dot > 0.9f);
    
    // Opposite: max vs min
    PillarStateVector high = create_default_pillar_state(vn::fp20_t(1.0f));
    PillarStateVector low = create_default_pillar_state(vn::fp20_t(0.0f));
    float dot2 = bloch_dot_product(high, low);
    // theta=0 → cos=1, theta=pi → cos=-1
    // dot = (1*0 + 1*(-1)) / 16 ... sum over 16 pillars
    // Each pillar: sin(0)*sin(pi) + cos(0)*cos(pi) = 0 + 1*(-1) = -1
    // total = 16 * (-1) / 16 = -1
    assert(dot2 < -0.9f);
    
    printf("PASS (dot=%.3f, opposite=%.3f)\n", dot, dot2);
}

// Test full transform_operation
inline void test_transform_operation() {
    printf("Test: TRANSFORM operation... ");
    PillarStateVector actor = create_default_pillar_state(vn::fp20_t(0.5f));
    PillarStateVector subject = create_default_pillar_state(vn::fp20_t(0.5f));
    
    subject[Integrity] = vn::fp20_t(0.8f);
    subject[Willpower] = vn::fp20_t(0.6f);
    actor[Harm] = vn::fp20_t(0.9f);
    actor[Influence] = vn::fp20_t(0.7f);
    actor[Presence] = vn::fp20_t(0.5f);
    
    float constraint = 0.0f;
    TransformResult r = transform_operation(actor, subject, Harm, Integrity, vn::fp20_t(constraint));
    
    assert(std::abs((r.operator_value - vn::fp20_t(0.9f)).to_float()) < 0.001f);
    assert(std::abs((r.influence - vn::fp20_t(0.7f)).to_float()) < 0.001f);
    assert(std::abs((r.target_pillar_before - vn::fp20_t(0.8f)).to_float()) < 0.001f);
    assert(r.target_pillar_after < 0.8f);  // should decrease
    assert(r.delta_theta_applied > 0.0f);   // should have rotated
    
    printf("PASS (integrity %.3f -> %.3f, constraint=%.3f)\n",
           r.target_pillar_before.to_float(), r.target_pillar_after.to_float(),
           r.constraint_level_after.to_float());
}

// Test apply_harm_transform convenience function
inline void test_apply_harm() {
    printf("Test: Apply Harm transform... ");
    PillarStateVector actor = create_default_pillar_state(vn::fp20_t(0.5f));
    PillarStateVector subject = create_default_pillar_state(vn::fp20_t(0.5f));
    subject[Integrity] = vn::fp20_t(0.8f);
    subject[Willpower] = vn::fp20_t(0.6f);
    actor[Harm] = vn::fp20_t(0.9f);
    actor[Influence] = vn::fp20_t(0.7f);
    
    TransformResult r = apply_harm_transform(actor, subject, Integrity);
    
    assert(r.operator_pillar == Harm);
    assert(vn::fp20_t(subject[Integrity]) != vn::fp20_t(0.8f));  // should have changed
    
    printf("PASS (harm applied, integrity=%.3f)\n", subject[Integrity]);
}

// Test Depth buffer overflow → Constraint increase
inline void test_depth_overflow() {
    printf("Test: Depth overflow -> constraint... ");
    PillarStateVector actor = create_default_pillar_state(vn::fp20_t(0.5f));
    PillarStateVector subject = create_default_pillar_state(vn::fp20_t(0.5f));
    
    subject[Integrity] = vn::fp20_t(0.9f);
    subject[Willpower] = vn::fp20_t(0.2f);  // low willpower
    subject[Depth] = vn::fp20_t(0.1f);       // low depth
    actor[Harm] = vn::fp20_t(1.0f);          // max harm
    actor[Influence] = vn::fp20_t(1.0f);     // max influence
    actor[Presence] = vn::fp20_t(1.0f);
    
    vn::fp20_t constraint(0.0f);
    TransformResult r = transform_operation(actor, subject, Harm, Integrity, constraint);
    
    // Depth should be depleted, constraint should have increased
    assert(r.depth_after < r.depth_before);
    assert(r.constraint_level_after > r.constraint_level_before);
    
    printf("PASS (depth %.3f->%.3f, constraint %.3f->%.3f)\n",
           r.depth_before.to_float(), r.depth_after.to_float(),
           r.constraint_level_before.to_float(), r.constraint_level_after.to_float());
}

// Run all transform tests
inline void run_transform_tests() {
    printf("=== Transform Tests (Bloch Rotation) ===\n");
    test_bloch_dot_product();
    test_transform_operation();
    test_apply_harm();
    test_depth_overflow();
    printf("=== All Transform Tests PASSED ===\n\n");
}
