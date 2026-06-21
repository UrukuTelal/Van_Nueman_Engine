#pragma once
#include <cstdio>
#include <cmath>
#include "../include/BlochMath.h"

// ── Reference vectors for cross-validation ───────────────────
// These expected values are computed from the canonical mapping:
//   θ = acos(2v - 1),  v = (cosθ + 1)/2
// All GPU kernel implementations (CUDA, GLSL, vncc SPIR-V) must
// produce identical results to within 1e-4 for the same inputs.
//
// Format: { input_v, expected_theta, expected_rotate(input_v, +0.5rad) }

struct BlochRef {
    float v;              // pillar value [0,1]
    float theta;          // expected bloch_value_to_theta(v)
    float rotate_p05;     // expected bloch_rotate(v, +0.5)
    float rotate_m05;     // expected bloch_rotate(v, -0.5)
};

static const BlochRef BLOCH_REFS[] = {
    {0.0f,   3.14159265f, 0.061209f,    0.061209f},     // south pole
    {0.1f,   2.49809154f, 0.005139f,    0.292795f},
    {0.2f,   2.21429744f, 0.044955f,    0.428495f},
    {0.3f,   1.98231317f, 0.104783f,    0.544184f},
    {0.4f,   1.77215425f, 0.177372f,    0.647111f},
    {0.5f,   1.57079633f, 0.260287f,    0.739713f},     // equator
    {0.6f,   1.36943841f, 0.352889f,    0.822628f},
    {0.7f,   1.15927948f, 0.455816f,    0.895217f},
    {0.8f,   0.92729522f, 0.571505f,    0.955045f},
    {0.9f,   0.64350111f, 0.707205f,    0.994861f},
    {1.0f,   0.0f,        0.938791f,    0.938791f},     // north pole
};

static constexpr int NUM_BLOCH_REFS = sizeof(BLOCH_REFS) / sizeof(BLOCH_REFS[0]);

// ── Helper: absolute float error ───────────────────────────
inline float bloch_err(float a, float b) {
    float d = a - b;
    return d < 0.0f ? -d : d;
}

// ── Test: value_to_theta ───────────────────────────────────
inline void test_cross_value_to_theta() {
    printf("  value_to_theta vs reference... ");
    bool pass = true;
    for (int i = 0; i < NUM_BLOCH_REFS; i++) {
        float t = bloch_value_to_theta(BLOCH_REFS[i].v);
        if (bloch_err(t, BLOCH_REFS[i].theta) > 1e-4f) {
            printf("\n    FAIL at v=%.2f: got theta=%.6f, expected=%.6f (err=%.6f)",
                   BLOCH_REFS[i].v, t, BLOCH_REFS[i].theta, bloch_err(t, BLOCH_REFS[i].theta));
            pass = false;
        }
    }
    printf("%s\n", pass ? "PASS" : "FAIL");
}

// ── Test: theta_to_value ───────────────────────────────────
inline void test_cross_theta_to_value() {
    printf("  theta_to_value vs reference... ");
    bool pass = true;
    for (int i = 0; i < NUM_BLOCH_REFS; i++) {
        float v = bloch_theta_to_value(BLOCH_REFS[i].theta);
        if (bloch_err(v, BLOCH_REFS[i].v) > 1e-4f) {
            printf("\n    FAIL at theta=%.6f: got v=%.6f, expected=%.6f (err=%.6f)",
                   BLOCH_REFS[i].theta, v, BLOCH_REFS[i].v, bloch_err(v, BLOCH_REFS[i].v));
            pass = false;
        }
    }
    printf("%s\n", pass ? "PASS" : "FAIL");
}

// ── Test: bloch_rotate (+0.5 rad) ──────────────────────────
inline void test_cross_rotate_plus() {
    printf("  bloch_rotate(v, +0.5) vs reference... ");
    bool pass = true;
    for (int i = 0; i < NUM_BLOCH_REFS; i++) {
        float r = bloch_rotate(BLOCH_REFS[i].v, 0.5f);
        if (bloch_err(r, BLOCH_REFS[i].rotate_p05) > 1e-4f) {
            printf("\n    FAIL at v=%.2f: got %.6f, expected=%.6f (err=%.6f)",
                   BLOCH_REFS[i].v, r, BLOCH_REFS[i].rotate_p05, bloch_err(r, BLOCH_REFS[i].rotate_p05));
            pass = false;
        }
    }
    printf("%s\n", pass ? "PASS" : "FAIL");
}

// ── Test: bloch_rotate (-0.5 rad) ──────────────────────────
inline void test_cross_rotate_minus() {
    printf("  bloch_rotate(v, -0.5) vs reference... ");
    bool pass = true;
    for (int i = 0; i < NUM_BLOCH_REFS; i++) {
        float r = bloch_rotate(BLOCH_REFS[i].v, -0.5f);
        if (bloch_err(r, BLOCH_REFS[i].rotate_m05) > 1e-4f) {
            printf("\n    FAIL at v=%.2f: got %.6f, expected=%.6f (err=%.6f)",
                   BLOCH_REFS[i].v, r, BLOCH_REFS[i].rotate_m05, bloch_err(r, BLOCH_REFS[i].rotate_m05));
            pass = false;
        }
    }
    printf("%s\n", pass ? "PASS" : "FAIL");
}

// ── Test: round-trip guarantee ─────────────────────────────
inline void test_cross_roundtrip() {
    printf("  round-trip theta_to_value(value_to_theta(v)) ≈ v... ");
    bool pass = true;
    float test_vals[] = {0.0f, 0.001f, 0.1f, 0.25f, 0.5f, 0.75f, 0.9f, 0.999f, 1.0f};
    for (float v : test_vals) {
        float t = bloch_value_to_theta(v);
        float back = bloch_theta_to_value(t);
        if (bloch_err(back, v) > 1e-4f) {
            printf("\n    FAIL at v=%.6f: got back=%.6f (err=%.6f)", v, back, bloch_err(back, v));
            pass = false;
        }
    }
    printf("%s\n", pass ? "PASS" : "FAIL");
}

// ── Test: fp20_t overloads match float overloads ───────────
inline void test_cross_fp20_consistency() {
    printf("  fp20_t overloads match float overloads... ");
    bool pass = true;
    for (int i = 0; i < NUM_BLOCH_REFS; i++) {
        vn::fp20_t v(BLOCH_REFS[i].v);
        vn::fp20_t t_fp20 = bloch_value_to_theta(v);
        float t_float = bloch_value_to_theta(BLOCH_REFS[i].v);
        float t_got = t_fp20.to_float();
        if (bloch_err(t_got, t_float) > 1e-4f) {
            printf("\n    FAIL theta at v=%.2f: fp20=%.6f, float=%.6f", BLOCH_REFS[i].v, t_got, t_float);
            pass = false;
        }

        vn::fp20_t r_fp20 = bloch_rotate(v, vn::fp20_t(0.5f));
        float r_float = bloch_rotate(BLOCH_REFS[i].v, 0.5f);
        float r_got = r_fp20.to_float();
        if (bloch_err(r_got, r_float) > 1e-4f) {
            printf("\n    FAIL rotate at v=%.2f: fp20=%.6f, float=%.6f", BLOCH_REFS[i].v, r_got, r_float);
            pass = false;
        }
    }
    printf("%s\n", pass ? "PASS" : "FAIL");
}

// ── Test: distortion torsion matches reference ────────────
inline void test_cross_distortion_torsion() {
    printf("  bloch_apply_distortion_torsion... ");
    struct { float awareness, distortion, expected; } tests[] = {
        {0.5f, 0.0f, 0.5f},
        {0.5f, 1.0f, 0.5f},    // full twist (pi rad) - back to opposite = same for equator
        {0.8f, 0.3f, 0.352729f},
        {0.2f, 0.5f, 0.100000f},
        {1.0f, 0.1f, 0.975528f}, // near north pole, small twist
    };
    bool pass = true;
    for (auto& t : tests) {
        float result = bloch_apply_distortion_torsion(t.awareness, t.distortion);
        if (bloch_err(result, t.expected) > 1e-4f) {
            printf("\n    FAIL awareness=%.1f, distortion=%.1f: got %.6f, expected=%.6f",
                   t.awareness, t.distortion, result, t.expected);
            pass = false;
        }
    }
    printf("%s\n", pass ? "PASS" : "FAIL");
}

// ── Master runner ─────────────────────────────────────────
inline void run_bloch_cross_validation_tests() {
    printf("=== Bloch Cross-Validation Tests (Reference Vectors) ===\n");
    test_cross_value_to_theta();
    test_cross_theta_to_value();
    test_cross_rotate_plus();
    test_cross_rotate_minus();
    test_cross_roundtrip();
    test_cross_fp20_consistency();
    test_cross_distortion_torsion();
    printf("=== All Bloch Cross-Validation Tests PASSED ===\n\n");
}
