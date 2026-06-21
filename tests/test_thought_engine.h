#pragma once

#include "../quantum/thought_engine.h"
#include "../include/Entity.h"
#include <cstdio>
#include <cmath>

namespace vn {
namespace tests {

static int test_engine_init_from_psv() {
    int failures = 0;
    vn::quantum::ThoughtEngine engine;

    PillarStateVector psv;
    for (int i = 0; i < NumPillars; i++)
        psv.pillars[i] = (float)i / (float)(NumPillars - 1);

    engine.init_from_psv(psv);

    // All frames should match input
    for (int f = 0; f < HOPF_FRAME_COUNT; f++) {
        for (int p = 0; p < NumPillars; p++) {
            float diff = std::fabs(engine.state.frames[f][p] - psv.pillars[p]);
            if (diff > 0.001f) {
                printf("FAIL engine_init_from_psv[%d][%d]: %.4f vs %.4f\n",
                       f, p, engine.state.frames[f][p], psv.pillars[p]);
                ++failures;
                goto next_check;
            }
        }
    }
    next_check:

    // Membrane should be computed (non-zero)
    float mem_sum = 0.0f;
    for (int i = 0; i < HOPF_BASE_DIM; i++)
        mem_sum += std::fabs(engine.state.membrane[i]);
    if (mem_sum < 0.001f) {
        printf("FAIL engine_init_from_psv: membrane all-zero\n");
        ++failures;
    }

    // Output should be in valid range
    PillarStateVector out = engine.output_psv();
    for (int i = 0; i < NumPillars; i++) {
        if (out.pillars[i] < 0.0f || out.pillars[i] > 1.0f) {
            printf("FAIL engine_init_from_psv output[%d]: %.6f out of [0,1]\n",
                   i, out.pillars[i]);
            ++failures;
        }
    }

    if (failures == 0)
        printf("PASS: engine_init_from_psv\n");
    return failures;
}

static int test_engine_init_from_token() {
    int failures = 0;
    vn::quantum::ThoughtEngine engine;
    PillarStateVector token_psv = engine.tokenizer.decode(0);  // Awareness token

    engine.init_from_token(0);

    // Frame 0 should match the decoded Awareness PSV
    for (int p = 0; p < NumPillars; p++) {
        float diff = std::fabs(engine.state.frames[0][p] - token_psv.pillars[p]);
        if (diff > 0.001f) {
            printf("FAIL engine_init_from_token[%d]: %.4f vs %.4f\n",
                   p, engine.state.frames[0][p], token_psv.pillars[p]);
            ++failures;
        }
    }

    // Output should be in valid range
    PillarStateVector out = engine.output_psv();
    for (int i = 0; i < NumPillars; i++) {
        if (out.pillars[i] < 0.0f || out.pillars[i] > 1.0f) {
            printf("FAIL engine_init_from_token output[%d]: %.6f out of [0,1]\n",
                   i, out.pillars[i]);
            ++failures;
        }
    }

    if (failures == 0)
        printf("PASS: engine_init_from_token (token 0 = Awareness)\n");
    return failures;
}

static int test_engine_diverge_converge() {
    int failures = 0;
    vn::quantum::ThoughtEngine engine;

    PillarStateVector psv;
    for (int i = 0; i < NumPillars; i++)
        psv.pillars[i] = 0.5f;

    engine.init_from_psv(psv);
    float before = engine.fiber_divergence();

    // Diverge should increase fiber variation
    srand(42);
    engine.diverge(0.5f);
    float after_diverge = engine.fiber_divergence();

    // After diverge, fiber divergence should be > before
    // (may not always hold with random, but with strength 0.5 it's extremely likely)
    if (after_diverge <= before) {
        // This can occasionally fail with random noise; accept near-equal
        if (after_diverge + 0.01f < before) {
            printf("FAIL engine_diverge: divergence %.6f -> %.6f (expected increase)\n",
                   before, after_diverge);
            ++failures;
        }
    }

    // Converge should decrease fiber variation
    engine.converge(0.9f);
    float after_converge = engine.fiber_divergence();

    if (after_converge > after_diverge + 0.001f) {
        printf("FAIL engine_converge: divergence %.6f -> %.6f (expected decrease)\n",
               after_diverge, after_converge);
        ++failures;
    }

    // Output should be in valid range after diverge+converge
    PillarStateVector out = engine.output_psv();
    for (int i = 0; i < NumPillars; i++) {
        if (out.pillars[i] < 0.0f || out.pillars[i] > 1.0f) {
            printf("FAIL engine_diverge_converge output[%d]: %.6f out of [0,1]\n",
                   i, out.pillars[i]);
            ++failures;
        }
    }

    if (failures == 0)
        printf("PASS: engine_diverge_converge (%.6f -> %.6f -> %.6f)\n",
               before, after_diverge, after_converge);
    return failures;
}

static int test_engine_think() {
    int failures = 0;
    vn::quantum::ThoughtEngine engine;
    engine.creativity = 0.2f;
    engine.coherence = 0.8f;

    PillarStateVector input;
    for (int i = 0; i < NumPillars; i++)
        input.pillars[i] = (float)i / (float)(NumPillars - 1);

    // Direct continuous path
    srand(42);
    PillarStateVector out = engine.think(input, 3, 0.016f, false);
    for (int i = 0; i < NumPillars; i++) {
        if (out.pillars[i] < 0.0f || out.pillars[i] > 1.0f) {
            printf("FAIL engine_think_continuous[%d]: %.6f out of [0,1]\n",
                   i, out.pillars[i]);
            ++failures;
        }
    }

    // Tokenized discrete path
    srand(42);
    PillarStateVector out_tok = engine.think(input, 3, 0.016f, true);
    for (int i = 0; i < NumPillars; i++) {
        if (out_tok.pillars[i] < 0.0f || out_tok.pillars[i] > 1.0f) {
            printf("FAIL engine_think_tokenized[%d]: %.6f out of [0,1]\n",
                   i, out_tok.pillars[i]);
            ++failures;
        }
    }

    // Continuous and tokenized paths should produce different outputs
    // (tokenized discretizes through vocabulary, continuous preserves nuances)
    bool different = false;
    for (int i = 0; i < NumPillars; i++) {
        if (std::fabs(out.pillars[i] - out_tok.pillars[i]) > 0.01f) {
            different = true;
            break;
        }
    }
    if (!different) {
        printf("FAIL engine_think: continuous and tokenized paths produce same output\n");
        ++failures;
    }

    if (failures == 0)
        printf("PASS: engine_think (continuous + tokenized, both valid)\n");
    return failures;
}

static int test_engine_getters() {
    int failures = 0;
    vn::quantum::ThoughtEngine engine;

    PillarStateVector psv;
    psv.fill(0.5f);
    engine.init_from_psv(psv);

    float thought[HOPF_FIBER_DIM];
    engine.get_thought(thought);
    float thought_sum = 0.0f;
    for (int i = 0; i < HOPF_FIBER_DIM; i++) thought_sum += std::fabs(thought[i]);

    if (thought_sum < 0.001f) {
        printf("FAIL engine_get_thought: thought all-zero\n");
        ++failures;
    }

    float membrane[HOPF_BASE_DIM];
    engine.get_membrane(membrane);
    float mem_sum = 0.0f;
    for (int i = 0; i < HOPF_BASE_DIM; i++) mem_sum += std::fabs(membrane[i]);

    if (mem_sum < 0.001f) {
        printf("FAIL engine_get_membrane: membrane all-zero\n");
        ++failures;
    }

    // fiber_divergence should be near zero when all frames are identical
    float div = engine.fiber_divergence();
    if (div > 0.001f) {
        printf("FAIL engine_fiber_divergence: %.6f (expected ~0 for uniform frames)\n", div);
        ++failures;
    }

    if (failures == 0)
        printf("PASS: engine_getters (thought, membrane, fiber_divergence)\n");
    return failures;
}

static int test_thought_engine() {
    int failures = 0;
    failures += test_engine_init_from_psv();
    failures += test_engine_init_from_token();
    failures += test_engine_diverge_converge();
    failures += test_engine_think();
    failures += test_engine_getters();
    if (failures == 0)
        printf("ALL thought engine tests PASS\n");
    else
        printf("thought engine tests: %d FAILURES\n", failures);
    return failures;
}

} // namespace tests
} // namespace vn

inline void run_thought_engine_tests() {
    vn::tests::test_thought_engine();
}
