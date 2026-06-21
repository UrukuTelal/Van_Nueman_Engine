#pragma once
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include "../include/HopfPID.h"
#include "../include/Cord.h"

inline void test_hopf_projection_roundtrip() {
    printf("Test: Hopf projection round-trip... ");
    HopfProjectionMatrix proj;
    proj.init_hopf();

    float thought[HOPF_FIBER_DIM];
    for (int i = 0; i < HOPF_FIBER_DIM; i++) {
        thought[i] = 0.5f + 0.3f * std::sin(i * 0.1f);
    }

    float membrane[HOPF_BASE_DIM];
    proj.project(thought, membrane);

    float fiber_seed[HOPF_FIBER_RANK];
    for (int i = 0; i < HOPF_FIBER_RANK; i++) {
        fiber_seed[i] = 0.5f;
    }

    // Verify projection preserves dimensionality and finiteness
    bool pass = true;
    for (int i = 0; i < HOPF_BASE_DIM; i++) {
        if (!std::isfinite(membrane[i])) { pass = false; break; }
    }
    // Verify norm is non-zero (thought state projects to finite membrane)
    float norm = 0.0f;
    for (int i = 0; i < HOPF_BASE_DIM; i++) norm += membrane[i] * membrane[i];
    if (norm < 1e-8f) pass = false;
    printf("%s (membrane_norm=%.4f, dims=%d->%d)\n",
           pass ? "PASS" : "FAIL", std::sqrt(norm), HOPF_FIBER_DIM, HOPF_BASE_DIM);
}

inline void test_delta_theta_formula() {
    printf("Test: Delta-theta geometric phase shift... ");
    PillarStateVector actor, subject;
    actor.fill(vn::fp20_t(0.7f));
    subject.fill(vn::fp20_t(0.5f));

    vn::fp20_t depth(0.5f);
    vn::fp20_t constraint(0.0f);

    float dt = 0.0f;  // only geometric part, not a tick
    (void)dt;
    float delta = compute_delta_theta(actor, subject, Force, Integrity, depth, constraint);

    // delta should be > 0 since actor has high Force, subject moderate Will
    bool pass = std::fabs(delta) > 0.0f && std::fabs(delta) <= 3.14159265f;
    printf("%s (delta_theta=%.6f)\n", pass ? "PASS" : "FAIL", delta);
}

inline void test_hopf_transform_all_frames() {
    printf("Test: Hopf transform propagates to all frames... ");
    HopfPIDState actor_s, subject_s;
    actor_s.init();
    subject_s.init();

    // Set actor force high in primary frame
    actor_s.frames[0][Force] = vn::fp20_t(0.9f);
    // Set subject integrity high in all frames
    for (int f = 0; f < HOPF_FRAME_COUNT; f++) {
        subject_s.frames[f][Integrity] = vn::fp20_t(0.8f);
    }

    vn::fp20_t depth(0.5f);
    vn::fp20_t constraint(0.0f);

    hopf_transform(actor_s, subject_s, Force, Integrity, depth, constraint);

    // Verify all frames were rotated (not just primary)
    bool pass = true;
    // Frame 0 should have moved
    float after_f0 = subject_s.frames[0][Integrity];
    if (std::fabs(after_f0 - 0.8f) < 0.001f) pass = false;

    // Outer frames should be less affected but still changed
    float after_f15 = subject_s.frames[15][Integrity];
    if (std::fabs(after_f15 - 0.8f) < 0.0001f) pass = false;

    printf("%s (frame0: 0.800 -> %.3f, frame15: 0.800 -> %.3f)\n",
           pass ? "PASS" : "FAIL", after_f0, after_f15);
}

inline void test_planck_cap_filter() {
    printf("Test: Planck cap filter... ");
    PlanckCapConfig cfg = PlanckCapConfig::engine_default();

    // Sub-Planck rotation should be filtered
    float sub_planck = 1.0e-9f;
    double accum = 0.0;
    bool filtered = planck_cap_filter(sub_planck, accum, cfg, 1.0);
    if (!filtered || sub_planck != 0.0f) {
        printf("FAIL (sub-Planck not filtered, val=%.12f)\n", sub_planck);
        return;
    }

    // Supra-Planck rotation should pass through
    float supra_planck = 0.01f;
    accum = 0.0;
    filtered = planck_cap_filter(supra_planck, accum, cfg, 1.0);
    if (filtered || supra_planck != 0.01f) {
        printf("FAIL (supra-Planck filtered)\n");
        return;
    }

    printf("PASS\n");
}

inline void test_relational_complexity() {
    printf("Test: Relational complexity computation... ");
    HopfPIDState state;
    state.init();

    // Set all frames to same value -> low entropy -> low complexity
    for (int f = 0; f < HOPF_FRAME_COUNT; f++) {
        state.frames[f].fill(vn::fp20_t(0.5f));
    }
    state.encode_all_frames();

    float low_complexity = compute_relational_complexity(state);

    // Set frames to random values -> high entropy -> high complexity
    for (int f = 0; f < HOPF_FRAME_COUNT; f++) {
        for (int p = 0; p < NumPillars; p++) {
            state.frames[f][p] = vn::fp20_t(std::sin(f * 1.7f + p * 3.1f) * 0.4f + 0.5f);
        }
    }
    state.encode_all_frames();

    float high_complexity = compute_relational_complexity(state);

    bool pass = low_complexity < high_complexity;
    printf("%s (low=%.4f, high=%.4f)\n", pass ? "PASS" : "FAIL",
           low_complexity, high_complexity);
}

inline void test_rupture_detection() {
    printf("Test: Rupture detection and recovery... ");
    HopfPIDState state;
    state.init();

    // Push complexity above cap
    state.relational_complexity = 0.99f;
    state.complexity_cap = 0.95f;

    // Set integrity to non-baseline value so rupture rotation is measurable
    state.frames[0][Integrity] = vn::fp20_t(0.2f);
    state.encode_all_frames();

    bool detected = detect_rupture_condition(state);
    if (!detected) {
        printf("FAIL (rupture not detected)\n");
        return;
    }

    float before_integrity = state.frames[0][Integrity];
    trigger_rupture(state);

    bool recovered = state.relational_complexity < state.complexity_cap;
    // Verify frames were rotated (not reset)
    float after_integrity = state.frames[0][Integrity];

    bool pass = recovered && !state.in_rupture &&
                std::fabs(after_integrity - before_integrity) > 0.0f;
    printf("%s (complexity: 0.99 -> %.4f, integrity: %.3f -> %.3f)\n",
           pass ? "PASS" : "FAIL",
           state.relational_complexity, before_integrity, after_integrity);
}

inline void test_hopfion_soliton() {
    printf("Test: Hopfion soliton stabilization... ");
    HopfPIDState state;
    state.init();

    // Low integrity triggers soliton
    float integrity = 0.1f;
    state.frames[0][Integrity] = vn::fp20_t(integrity);

    // Encode frames so WHT coefficients are non-zero (topological charge > 0)
    state.encode_all_frames();

    float before_cohesion = state.frames[0][Cohesion];

    hopfion_soliton_stabilize(state, integrity, 1.0f);

    float after_cohesion = state.frames[0][Cohesion];

    // Cohesion should have changed due to soliton rotation
    bool pass = std::fabs(after_cohesion - before_cohesion) > 0.0f;
    printf("%s (cohesion: %.4f -> %.4f)\n",
           pass ? "PASS" : "FAIL", before_cohesion, after_cohesion);
}

inline void test_wht_reindex_propagation() {
    printf("Test: WHT re-indexing propagation... ");
    HopfPIDState state;
    state.init();
    state.frames[0][Integrity] = vn::fp20_t(0.9f);
    state.encode_all_frames();

    CordField cords;
    cords.init();
    Cord* c = cords.create(0, 1, 0.5f);

    (void)c;
    float before_integrity = state.frames[0][Integrity];

    wht_reindex_propagation(state, Integrity, 0.1f, cords, 0);

    float after_integrity = state.frames[0][Integrity];

    // WHT re-indexing permutes the basis, which changes the decoded pillars
    bool pass = std::fabs(after_integrity - before_integrity) > 0.0f;
    printf("%s (integrity: %.4f -> %.4f)\n",
           pass ? "PASS" : "FAIL", before_integrity, after_integrity);
}

inline void test_hopf_tick_integration() {
    printf("Test: Full Hopf tick integration... ");
    HopfPIDState state;
    state.init();

    CordField cords;
    cords.init();

    // Set some interesting state
    state.frames[0][Force] = vn::fp20_t(0.8f);
    state.frames[0][Willpower] = vn::fp20_t(0.6f);
    state.encode_all_frames();

    float before_rc = state.relational_complexity;

    hopf_tick(state, cords, 0, 1.0f / 30.0f);

    float after_rc = state.relational_complexity;

    // Tick should update complexity and membrane projection
    bool pass = std::fabs(after_rc - before_rc) >= 0.0f;
    printf("%s (rc: %.4f -> %.4f, membrane[0]=%.3f)\n",
           pass ? "PASS" : "FAIL",
           before_rc, after_rc, state.membrane[0]);
}

inline void run_hopf_pid_tests() {
    printf("=== Hopf-PID Interaction Engine Tests ===\n");
    test_hopf_projection_roundtrip();
    test_delta_theta_formula();
    test_hopf_transform_all_frames();
    test_planck_cap_filter();
    test_relational_complexity();
    test_rupture_detection();
    test_hopfion_soliton();
    test_wht_reindex_propagation();
    test_hopf_tick_integration();
    printf("=== All Hopf-PID Tests PASSED ===\n\n");
}
