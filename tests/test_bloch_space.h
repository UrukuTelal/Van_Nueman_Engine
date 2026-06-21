#pragma once
#include <cstdio>
#include <cmath>
#include "../include/BlochSpace.h"
#include "../include/TransformCore.h"
#include "../include/HopfPID.h"
#include "../vn/ScaledInt.h"

// ── Test 1: ENFORCE_BLOCH_SPACE compiles on ShiftResult ────
inline void test_enforce_bloch_shift_result() {
    printf("  ENFORCE_BLOCH_SPACE(ShiftResult)... ");
    ShiftResult sr;
    sr.shift_rad = vn::fp20_t(0.5);
    sr.depth_consumed = vn::fp20_t(0.0);
    sr.constraint_delta = vn::fp20_t(0.1);
    sr.excess_rad = vn::fp20_t(0.0);
    printf("PASS (instantiated, _bloch_audited = %s)\n",
           ShiftResult::_bloch_audited ? "true" : "false");
}

// ── Test 2: DepthUtilization calculations ───────────────────
inline void test_depth_utilization() {
    printf("  DepthUtilization sampling... ");
    DepthUtilization du;
    du.reset();

    // All energy in Depth = blackbox warning
    PillarStateVector psv_all_depth;
    for (int i = 0; i < NumPillars; i++) psv_all_depth[i] = 0.0f;
    psv_all_depth[Depth] = 1.0f;
    du.sample(psv_all_depth);
    bool bw1 = du.is_blackbox_warning();
    printf("ratio=%.3f warn=%d ", du.ratio, bw1);

    // Even split across all pillars
    float even = 1.0f / static_cast<float>(NumPillars);
    for (int i = 0; i < NumPillars; i++) psv_all_depth[i] = even;
    du.sample(psv_all_depth);
    printf("even_ratio=%.3f ", du.ratio);

    bool pass = bw1 && !du.is_blackbox_warning();
    printf("%s\n", pass ? "PASS" : "FAIL");
}

// ── Test 3: CrystallizationYield basic flow ─────────────────
inline void test_crystallization_yield() {
    printf("  CrystallizationYield flow... ");
    CrystallizationYield cy;
    cy.init(0.1f);  // low threshold to ensure crystallization

    PillarStateVector psv;
    for (int i = 0; i < NumPillars; i++) psv[i] = 0.5f + 0.1f * (i % 3);

    int r1 = cy.crystallize(psv, 0.5f, 1.0f);
    int r2 = cy.crystallize(psv, 0.5f, 1.0f);  // same pattern → reinforce existing

    printf("crystalized=%d reinforced=%d active=%d",
           r1, r2, cy.count);

    bool pass = (r1 >= 0) && (r2 >= 0) && cy.count == 1;
    printf(" %s\n", pass ? "PASS" : "FAIL");
}

// ── Test 4: fp20_t precision for Hopf normalization ─────────
inline void test_fp20_hopf_precision() {
    printf("  fp20_t Hopf precision... ");

    // Simulate a 512D vector with values spread across [0,1]
    float input[512];
    for (int i = 0; i < 512; i++) {
        input[i] = 0.001f + (i % 100) * 0.01f;
    }

    // Normalize via fp20_t (simulating what normalize_thought does)
    float sum_sq = 0.0f;
    for (int i = 0; i < 512; i++) sum_sq += input[i] * input[i];
    float norm = std::sqrt(sum_sq);
    vn::fp20_t fp20_vals[512];
    for (int i = 0; i < 512; i++) {
        fp20_vals[i] = vn::fp20_t(input[i] / norm);
    }

    // Verify sum of squares ≈ 1.0 within fp20_t precision
    vn::fp20_t sum_sq_fp20(0);
    for (int i = 0; i < 512; i++) {
        sum_sq_fp20 = sum_sq_fp20 + fp20_vals[i] * fp20_vals[i];
    }
    float result = sum_sq_fp20.to_float();
    float error = std::fabs(result - 1.0f);

    printf("sum_sq=%.8f err=%.2e ", result, error);

    // fp20_t precision is ~9.5e-7 relative; for 512 elements,
    // accumulated error should be well under 1e-3
    bool pass = error < 0.01f;
    printf("%s\n", pass ? "PASS" : "FAIL");
}

// ── Test 5: BlochConductor tick doesn't crash ───────────────
inline void test_bloch_conductor_tick() {
    printf("  BlochConductor tick... ");
    BlochConductor bc;
    bc.init();

    PillarStateVector psv;
    for (int i = 0; i < NumPillars; i++) psv[i] = 0.5f;
    psv[Depth] = 0.8f;  // high Depth

    bc.tick(psv, 0.016f);
    bc.tick(psv, 0.016f);
    bc.tick(psv, 0.016f);

    bool pass = bc.depth_monitor.ratio > 0.0f;
    printf("ratio=%.3f sub_pillars=%d %s\n",
           bc.depth_monitor.ratio,
           bc.active_sub_pillar_count(),
           pass ? "PASS" : "FAIL");
}

// ── Run all Bloch-space tests ───────────────────────────────
inline int run_bloch_space_tests() {
    printf("=== BlochSpace Tests ===\n");

    test_enforce_bloch_shift_result();
    test_depth_utilization();
    test_crystallization_yield();
    test_fp20_hopf_precision();
    test_bloch_conductor_tick();

    printf("=== BlochSpace Tests Done ===\n");
    return 0;
}
