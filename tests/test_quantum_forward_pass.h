#pragma once

#include "../quantum/quantum_forward_pass.h"
#include "../quantum/native_quantum_backend.h"
#include "../include/HopfPID.h"
#include <cmath>
#include <cstdio>

namespace vn {
namespace tests {

static int test_forward_pass_determinism() {
    int failures = 0;
    vn::quantum::NativeQuantumBackend backend;
    HopfProjectionMatrix proj;
    proj.init_hopf();

    PillarStateVector input;
    for (int i = 0; i < NumPillars; i++)
        input.pillars[i] = (float)i / (float)(NumPillars - 1);

    PillarStateVector r1 = vn::quantum::quantum_cognitive_pass(input, backend, proj);
    PillarStateVector r2 = vn::quantum::quantum_cognitive_pass(input, backend, proj);

    for (int i = 0; i < NumPillars; i++) {
        float diff = std::fabs(r1.pillars[i] - r2.pillars[i]);
        if (diff > 0.001f) {
            printf("FAIL forward_pass_determinism[%d]: %.6f vs %.6f\n",
                   i, r1.pillars[i], r2.pillars[i]);
            ++failures;
        }
    }
    if (failures == 0)
        printf("PASS: forward_pass_determinism (two runs identical)\n");
    return failures;
}

static int test_forward_pass_output_range() {
    int failures = 0;
    vn::quantum::NativeQuantumBackend backend;
    HopfProjectionMatrix proj;
    proj.init_hopf();

    // Test all-zero input
    {
        PillarStateVector zero;
        zero.fill(0.0f);
        PillarStateVector result = vn::quantum::quantum_cognitive_pass(zero, backend, proj);
        for (int i = 0; i < NumPillars; i++) {
            if (result.pillars[i] < 0.0f || result.pillars[i] > 1.0f) {
                printf("FAIL forward_pass_range[zero][%d]: %.6f out of [0,1]\n",
                       i, result.pillars[i]);
                ++failures;
            }
        }
    }

    // Test all-one input
    {
        PillarStateVector one;
        one.fill(1.0f);
        PillarStateVector result = vn::quantum::quantum_cognitive_pass(one, backend, proj);
        for (int i = 0; i < NumPillars; i++) {
            if (result.pillars[i] < 0.0f || result.pillars[i] > 1.0f) {
                printf("FAIL forward_pass_range[one][%d]: %.6f out of [0,1]\n",
                       i, result.pillars[i]);
                ++failures;
            }
        }
    }

    // Test mid-range input
    {
        PillarStateVector mid;
        mid.fill(0.5f);
        PillarStateVector result = vn::quantum::quantum_cognitive_pass(mid, backend, proj);
        for (int i = 0; i < NumPillars; i++) {
            if (result.pillars[i] < 0.0f || result.pillars[i] > 1.0f) {
                printf("FAIL forward_pass_range[mid][%d]: %.6f out of [0,1]\n",
                       i, result.pillars[i]);
                ++failures;
            }
        }
    }

    // Test varied input (escalating values)
    {
        PillarStateVector varied;
        for (int i = 0; i < NumPillars; i++)
            varied.pillars[i] = (float)i / (float)(NumPillars - 1);
        PillarStateVector result = vn::quantum::quantum_cognitive_pass(varied, backend, proj);
        for (int i = 0; i < NumPillars; i++) {
            if (result.pillars[i] < 0.0f || result.pillars[i] > 1.0f) {
                printf("FAIL forward_pass_range[varied][%d]: %.6f out of [0,1]\n",
                       i, result.pillars[i]);
                ++failures;
            }
        }
    }

    if (failures == 0)
        printf("PASS: forward_pass_output_range (4 input sets, all in [0,1])\n");
    return failures;
}

static int test_forward_pass_thought_norm() {
    int failures = 0;
    vn::quantum::NativeQuantumBackend backend;
    HopfProjectionMatrix proj;
    proj.init_hopf();

    PillarStateVector input;
    for (int i = 0; i < NumPillars; i++)
        input.pillars[i] = (float)i / (float)(NumPillars - 1);

    // Run the forward pass manually to check the thought vector norm
    std::vector<float> amps = vn::quantum::encode_pillars_to_amplitudes(input);
    float wht_coeffs[WHT_N] = {0};
    for (int i = 0; i < (int)amps.size() && i < WHT_N; i++)
        wht_coeffs[i] = amps[i];
    float wht_out[WHT_N];
    backend.execute_wht(wht_coeffs, wht_out);

    float base_frame[NumPillars];
    decode_pillar_vector(wht_out, base_frame);

    float thought[HOPF_FIBER_DIM];
    for (int f = 0; f < HOPF_FRAME_COUNT; f++) {
        float phase = 2.0f * 3.14159265f * (float)f / (float)HOPF_FRAME_COUNT;
        for (int p = 0; p < NumPillars; p++) {
            float modulation = 0.1f * std::sin(phase + (float)p * 0.2f);
            vn::fp20_t rotated = bloch_rotate(
                vn::fp20_t(base_frame[p]),
                vn::fp20_t(modulation)
            );
            thought[f * NumPillars + p] = rotated.to_float();
        }
    }

    normalize_thought(thought, HOPF_FIBER_DIM);

    float sum_sq = 0.0f;
    for (int i = 0; i < HOPF_FIBER_DIM; i++)
        sum_sq += thought[i] * thought[i];
    float norm = std::sqrt(sum_sq);

    if (std::fabs(norm - 1.0f) > 0.001f) {
        printf("FAIL forward_pass_thought_norm: |thought| = %.6f (expected 1.0)\n", norm);
        ++failures;
    }

    if (failures == 0)
        printf("PASS: forward_pass_thought_norm (|thought| = %.6f ≈ 1.0)\n", norm);
    return failures;
}

static int test_forward_pass_non_destructive() {
    int failures = 0;
    vn::quantum::NativeQuantumBackend backend;
    HopfProjectionMatrix proj;
    proj.init_hopf();

    PillarStateVector input;
    for (int i = 0; i < NumPillars; i++)
        input.pillars[i] = (float)i / (float)(NumPillars - 1);

    PillarStateVector original = input;
    PillarStateVector result = vn::quantum::quantum_cognitive_pass(input, backend, proj);

    // Verify input was not modified
    for (int i = 0; i < NumPillars; i++) {
        if (std::fabs(input.pillars[i] - original.pillars[i]) > 0.001f) {
            printf("FAIL forward_pass_non_destructive[%d]: input modified\n", i);
            ++failures;
        }
    }

    // Verify output is not identical to input (non-trivial transformation)
    bool all_same = true;
    for (int i = 0; i < NumPillars; i++) {
        if (std::fabs(result.pillars[i] - input.pillars[i]) > 0.001f) {
            all_same = false;
            break;
        }
    }
    if (all_same) {
        printf("FAIL forward_pass_non_destructive: output identical to input (no transformation)\n");
        ++failures;
    }

    if (failures == 0)
        printf("PASS: forward_pass_non_destructive (input preserved, output transformed)\n");
    return failures;
}

static int test_forward_pass_tokenized() {
    int failures = 0;
    vn::quantum::NativeQuantumBackend backend;
    HopfProjectionMatrix proj;
    proj.init_hopf();
    vn::quantum::WHTTokenizer tokenizer;

    // Test with Awareness-dominant input — should tokenize to Awareness token
    PillarStateVector input;
    input.fill(0.1f);
    input.pillars[Awareness] = 0.9f;

    PillarStateVector result = vn::quantum::quantum_cognitive_pass_tokenized(
        input, backend, proj, tokenizer);

    // Output should be in valid range
    for (int i = 0; i < NumPillars; i++) {
        if (result.pillars[i] < 0.0f || result.pillars[i] > 1.0f) {
            printf("FAIL forward_pass_tokenized[%d]: %.6f out of [0,1]\n",
                   i, result.pillars[i]);
            ++failures;
        }
    }

    // Determinism: same input should produce same output
    PillarStateVector r2 = vn::quantum::quantum_cognitive_pass_tokenized(
        input, backend, proj, tokenizer);
    for (int i = 0; i < NumPillars; i++) {
        float diff = std::fabs(result.pillars[i] - r2.pillars[i]);
        if (diff > 0.001f) {
            printf("FAIL forward_pass_tokenized_determinism[%d]: %.6f vs %.6f\n",
                   i, result.pillars[i], r2.pillars[i]);
            ++failures;
        }
    }

    // Test with all-zero input — should produce a valid result
    PillarStateVector zero;
    zero.fill(0.0f);
    PillarStateVector zr = vn::quantum::quantum_cognitive_pass_tokenized(
        zero, backend, proj, tokenizer);
    for (int i = 0; i < NumPillars; i++) {
        if (zr.pillars[i] < 0.0f || zr.pillars[i] > 1.0f) {
            printf("FAIL forward_pass_tokenized_zero[%d]: %.6f out of [0,1]\n",
                   i, zr.pillars[i]);
            ++failures;
        }
    }

    if (failures == 0)
        printf("PASS: forward_pass_tokenized (deterministic, valid output)\n");
    return failures;
}

static int test_quantum_forward_pass() {
    int failures = 0;
    failures += test_forward_pass_determinism();
    failures += test_forward_pass_output_range();
    failures += test_forward_pass_thought_norm();
    failures += test_forward_pass_non_destructive();
    failures += test_forward_pass_tokenized();
    if (failures == 0)
        printf("ALL quantum forward pass tests PASS\n");
    else
        printf("quantum forward pass tests: %d FAILURES\n", failures);
    return failures;
}

} // namespace tests
} // namespace vn

inline void run_quantum_forward_pass_tests() {
    vn::tests::test_quantum_forward_pass();
}
