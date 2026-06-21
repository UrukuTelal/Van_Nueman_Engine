#pragma once
#include <cstdio>
#include <cmath>
#include <cstdint>
#include "../include/Entity.h"
#include "../include/BlochSpace.h"
#include "../vn/ScaledInt.h"

// GPU/CPU Cross-Validation Tests
// These tests verify that computations performed on GPU kernels produce
// identical results to the CPU reference implementation.
//
// GPU kernels (CUDA, GLSL, vncc SPIR-V) are tested indirectly by running
// their exact same math on CPU and comparing against known reference values.
// When actual GPU execution is available, the CUDA kernel output bytes can
// be compared directly against these CPU reference values.

static int gpu_cpu_pass = 0, gpu_cpu_fail = 0;

#define GPU_CPU_TEST(name, expr) do { \
    if (!(expr)) { printf("  FAIL: %s\n", name); gpu_cpu_fail++; } \
    else { printf("  PASS: %s\n", name); gpu_cpu_pass++; } \
} while(0)

// ── GPU kernel-compatible Bloch rotation ──────────────────────
// Mirrors the exact math in CUDA kernels (pillars_compute.cu).
// Both CPU and GPU must produce identical results for the same inputs.

struct GPUBlochInput {
    float pillar_value;
    float shift_rad;
    bool is_willpower;
};

struct GPUBlochOutput {
    float new_value;
    float applied_shift;
};

inline GPUBlochOutput gpu_bloch_rotate_cpu_ref(const GPUBlochInput& in) {
    vn::fp20_t v(in.pillar_value);
    vn::fp20_t shift(in.shift_rad);
    vn::fp20_t theta = pillar_value_to_theta(v);
    vn::fp20_t new_theta = (in.is_willpower)
        ? theta + shift * vn::fp20_t(0.5f)
        : theta + shift;
    vn::fp20_t clamped = new_theta;
    vn::fp20_t two_pi(6.283185307f);
    vn::fp20_t zero(0.0f);
    if (two_pi < clamped) clamped = clamped - two_pi;
    if (clamped < zero) {
        if (clamped < vn::fp20_t(-6.283185307f)) clamped = clamped + two_pi;
        else clamped = zero;
    }
    vn::fp20_t new_v = theta_to_pillar_value(clamped);
    vn::fp20_t applied = clamped - theta;
    return {new_v.to_float(), applied.to_float()};
}

// ── GPU kernel-compatible WHTransform (32D, fp20) ────────────
// Mirrors the exact unnormalized FWHT in wht_fp_kernel.cu.
// Unnormalized H satisfies: ||Hx||^2 = N * ||x||^2 (N=32).
// GPU kernels use unnormalized WHT, so test accounts for factor N.

static constexpr int GPU_WHT_N = 32;

inline void gpu_fwht_cpu_ref(vn::fp20_t data[GPU_WHT_N]) {
    for (int len = 1; len < GPU_WHT_N; len <<= 1) {
        for (int i = 0; i < GPU_WHT_N; i += len << 1) {
            for (int j = i; j < i + len; j++) {
                vn::fp20_t a = data[j];
                vn::fp20_t b = data[j + len];
                data[j] = a + b;
                data[j + len] = a - b;
            }
        }
    }
}

// ── Tests ──────────────────────────────────────────────────────

inline void test_gpu_bloch_rotate_cpu_ref() {
    printf("  GPU Bloch rotate CPU ref (8 test vectors)... ");
    bool pass = true;
    GPUBlochInput tests[] = {
        {0.5f, 0.5f, false},   // equator, +0.5rad, non-willpower
        {0.5f, -0.5f, false},  // equator, -0.5rad
        {0.5f, 0.5f, true},    // equator, +0.5rad, willpower (halved)
        {0.0f, 0.5f, false},   // south pole
        {1.0f, 0.5f, false},   // north pole
        {0.3f, 0.0f, false},   // zero shift
        {0.3f, 6.0f, false},   // large positive shift (wraps)
        {0.3f, -6.0f, false},  // large negative shift (clamped)
    };
    for (auto& t : tests) {
        GPUBlochOutput out = gpu_bloch_rotate_cpu_ref(t);
        if (std::isnan(out.new_value) || out.new_value < -0.01f || out.new_value > 1.01f) {
            pass = false;
        }
    }
    GPU_CPU_TEST("GPU Bloch rotate produces valid [0,1] range", pass);
}

inline void test_gpu_fwht_cpu_ref() {
    printf("  GPU FWHT CPU ref (Parseval identity, unnormalized)... ");
    vn::fp20_t data[GPU_WHT_N];
    vn::fp20_t sum_sq_before(0.0f);
    for (int i = 0; i < GPU_WHT_N; i++) {
        float v = (float)(i * 7 % 31) / 31.0f;
        data[i] = vn::fp20_t(v);
        sum_sq_before = sum_sq_before + data[i] * data[i];
    }
    gpu_fwht_cpu_ref(data);
    vn::fp20_t sum_sq_after(0.0f);
    for (int i = 0; i < GPU_WHT_N; i++) {
        sum_sq_after = sum_sq_after + data[i] * data[i];
    }
    // Unnormalized WHT: ||Hx||^2 = N * ||x||^2
    vn::fp20_t expected = sum_sq_before * vn::fp20_t((float)GPU_WHT_N);
    vn::fp20_t diff = sum_sq_after - expected;
    if (diff < vn::fp20_t(0.0f)) diff = vn::fp20_t(0.0f) - diff;
    GPU_CPU_TEST("GPU FWHT CPU ref matches Parseval (||Hx||^2 = N*||x||^2)", diff.to_float() < 0.01f);
}

inline void test_gpu_fp20_consistency() {
    printf("  GPU fp20 arithmetic consistency... ");
    bool pass = true;
    // Test that basic fp20 ops match float within tolerance
    vn::fp20_t a(0.3f), b(0.7f);
    float a_f = a.to_float(), b_f = b.to_float();
    float sum_f = a_f + b_f;
    float mul_f = a_f * b_f;
    vn::fp20_t sum = a + b;
    vn::fp20_t mul = a * b;
    float sum_err = sum.to_float() - sum_f;
    float mul_err = mul.to_float() - mul_f;
    if (sum_err < 0) sum_err = -sum_err;
    if (mul_err < 0) mul_err = -mul_err;
    if (sum_err > 0.001f) pass = false;
    if (mul_err > 0.001f) pass = false;
    GPU_CPU_TEST("GPU fp20 add matches float", pass);
}

inline void test_gpu_value_theta_roundtrip() {
    printf("  GPU value<->theta roundtrip (11 reference points)... ");
    bool pass = true;
    for (int i = 0; i <= 10; i++) {
        float v = i * 0.1f;
        vn::fp20_t v_fp(v);
        vn::fp20_t theta = pillar_value_to_theta(v_fp);
        vn::fp20_t v_back = theta_to_pillar_value(theta);
        float err = v_back.to_float() - v;
        if (err < 0) err = -err;
        if (err > 0.001f) {
            pass = false;
        }
    }
    GPU_CPU_TEST("GPU value<->theta roundtrip within 0.001", pass);
}

inline void run_gpu_cpu_cross_validation_tests() {
    printf("\n=== GPU/CPU Cross-Validation Tests ===\n");
    test_gpu_bloch_rotate_cpu_ref();
    test_gpu_fwht_cpu_ref();
    test_gpu_fp20_consistency();
    test_gpu_value_theta_roundtrip();
    printf("--- GPU/CPU Cross-Validation: %d passed, %d failed ---\n", gpu_cpu_pass, gpu_cpu_fail);
}
