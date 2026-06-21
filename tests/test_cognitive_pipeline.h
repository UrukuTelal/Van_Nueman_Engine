#pragma once

#include "../quantum/cognitive_pipeline.h"
#include <cstdio>
#include <cmath>

namespace vn {
namespace tests {

static int test_pipeline_default_process() {
    int failures = 0;
    vn::quantum::NativeCognitivePipeline pipeline;

    PillarStateVector input;
    for (int i = 0; i < NumPillars; i++)
        input.pillars[i] = (float)i / (float)(NumPillars - 1);

    PillarStateVector output = pipeline.process(input);

    // Output must be in [0, 1]
    for (int i = 0; i < NumPillars; i++) {
        if (output.pillars[i] < 0.0f || output.pillars[i] > 1.0f) {
            printf("FAIL pipeline_default_process[%d]: %.6f out of [0,1]\n",
                   i, output.pillars[i]);
            ++failures;
        }
    }

    // Output should differ from input (cognitive processing changes state)
    bool changed = false;
    for (int i = 0; i < NumPillars; i++) {
        if (std::fabs(output.pillars[i] - input.pillars[i]) > 0.001f) {
            changed = true;
            break;
        }
    }
    if (!changed) {
        printf("FAIL pipeline_default_process: output identical to input\n");
        ++failures;
    }

    if (failures == 0)
        printf("PASS: pipeline_default_process\n");
    return failures;
}

static int test_pipeline_reason_ticks() {
    int failures = 0;
    vn::quantum::NativeCognitivePipeline pipeline(0.3f, 0.7f);

    PillarStateVector input;
    input.fill(0.5f);

    // 1 tick
    PillarStateVector out1 = pipeline.reason(input, 1);
    for (int i = 0; i < NumPillars; i++) {
        if (out1.pillars[i] < 0.0f || out1.pillars[i] > 1.0f) {
            printf("FAIL pipeline_reason_ticks[1t][%d]: %.6f out of [0,1]\n",
                   i, out1.pillars[i]);
            ++failures;
        }
    }

    // 5 ticks with higher creativity
    pipeline.creativity = 0.5f;
    PillarStateVector out5 = pipeline.reason(input, 5);
    for (int i = 0; i < NumPillars; i++) {
        if (out5.pillars[i] < 0.0f || out5.pillars[i] > 1.0f) {
            printf("FAIL pipeline_reason_ticks[5t][%d]: %.6f out of [0,1]\n",
                   i, out5.pillars[i]);
            ++failures;
        }
    }

    // Multi-tick should diverge more than single tick
    float diff1 = 0.0f, diff5 = 0.0f;
    for (int i = 0; i < NumPillars; i++) {
        diff1 += std::fabs(out1.pillars[i] - 0.5f);
        diff5 += std::fabs(out5.pillars[i] - 0.5f);
    }
    if (diff5 <= diff1) {
        // May occasionally not hold with random divergence; accept near-equal
        if (diff5 + 0.01f < diff1) {
            printf("FAIL pipeline_reason_ticks: 5t divergence (%.4f) <= 1t (%.4f)\n",
                   diff5, diff1);
            ++failures;
        }
    }

    if (failures == 0)
        printf("PASS: pipeline_reason_ticks (1t_diff=%.4f, 5t_diff=%.4f)\n", diff1, diff5);
    return failures;
}

static int test_pipeline_tokenized_path() {
    int failures = 0;
    vn::quantum::NativeCognitivePipeline pipeline(0.2f, 0.8f);

    PillarStateVector input;
    for (int i = 0; i < NumPillars; i++)
        input.pillars[i] = 0.5f + 0.3f * std::sin((float)i * 0.5f);

    srand(42);
    PillarStateVector continuous = pipeline.reason(input, 3, 0.016f, false);
    srand(42);
    PillarStateVector tokenized = pipeline.reason(input, 3, 0.016f, true);

    // Both paths should produce valid outputs
    for (int i = 0; i < NumPillars; i++) {
        if (continuous.pillars[i] < 0.0f || continuous.pillars[i] > 1.0f) {
            printf("FAIL pipeline_tokenized continuous[%d]: %.6f out of [0,1]\n",
                   i, continuous.pillars[i]);
            ++failures;
        }
        if (tokenized.pillars[i] < 0.0f || tokenized.pillars[i] > 1.0f) {
            printf("FAIL pipeline_tokenized tokenized[%d]: %.6f out of [0,1]\n",
                   i, tokenized.pillars[i]);
            ++failures;
        }
    }

    // Continuous and tokenized should differ (tokenized discretizes through vocabulary)
    bool different = false;
    for (int i = 0; i < NumPillars; i++) {
        if (std::fabs(continuous.pillars[i] - tokenized.pillars[i]) > 0.01f) {
            different = true;
            break;
        }
    }
    if (!different) {
        printf("FAIL pipeline_tokenized: continuous == tokenized\n");
        ++failures;
    }

    if (failures == 0)
        printf("PASS: pipeline_tokenized_path\n");
    return failures;
}

static int test_pipeline_determinism() {
    int failures = 0;
    vn::quantum::NativeCognitivePipeline pipeline(0.0f, 1.0f);  // no creativity, full coherence

    PillarStateVector input;
    input.fill(0.5f);

    // With creativity=0, same input should give same output
    srand(42);
    PillarStateVector out_a = pipeline.process(input);
    srand(42);
    PillarStateVector out_b = pipeline.process(input);

    for (int i = 0; i < NumPillars; i++) {
        float diff = std::fabs(out_a.pillars[i] - out_b.pillars[i]);
        if (diff > 0.001f) {
            printf("FAIL pipeline_determinism[%d]: %.6f vs %.6f\n",
                   i, out_a.pillars[i], out_b.pillars[i]);
            ++failures;
        }
    }

    if (failures == 0)
        printf("PASS: pipeline_determinism (creativity=0)\n");
    return failures;
}

static int test_cognitive_pipeline() {
    int failures = 0;
    failures += test_pipeline_default_process();
    failures += test_pipeline_reason_ticks();
    failures += test_pipeline_tokenized_path();
    failures += test_pipeline_determinism();
    if (failures == 0)
        printf("ALL cognitive pipeline tests PASS\n");
    else
        printf("cognitive pipeline tests: %d FAILURES\n", failures);
    return failures;
}

} // namespace tests
} // namespace vn

inline void run_cognitive_pipeline_tests() {
    vn::tests::test_cognitive_pipeline();
}
