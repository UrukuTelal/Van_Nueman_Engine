#pragma once

#include "../quantum/wht_tokenizer.h"
#include "../include/Entity.h"
#include <cstdio>
#include <cmath>

namespace vn {
namespace tests {

static int test_vocab_init() {
    int failures = 0;
    vn::quantum::WHTSignalVocabulary vocab;
    vocab.init();

    // Verify tokens are non-zero (at least some elements)
    for (int i = 0; i < vocab.size(); i++) {
        const float* sig = vocab.token_signal(i);
        if (!sig) {
            printf("FAIL vocab_init: token_signal(%d) returned nullptr\n", i);
            ++failures;
            continue;
        }
        float sum = 0.0f;
        for (int j = 0; j < WHT_N; j++) sum += std::fabs(sig[j]);
        if (sum < 1e-6f && i != 58 && i != 63) {
            // Only NULL(58) and RESERVED(63) may be all-zero
            printf("FAIL vocab_init: token %d (%s) is all-zero\n", i, vocab.label(i));
            ++failures;
        }
    }

    // Verify first 16 tokens have non-zero energy at their pillar position
    for (int p = 0; p < NumPillars; p++) {
        const float* sig = vocab.token_signal(p);
        float total_energy = 0.0f;
        for (int j = 0; j < WHT_N; j++) total_energy += std::fabs(sig[j]);
        // The pillar position should have a meaningful coefficient (positive or negative)
        if (std::fabs(sig[p]) < 0.01f && total_energy > 0.01f) {
            // Only fail if the pillar position is near-zero while the signal has energy elsewhere
            printf("FAIL vocab_init: token %d (%s) pillar[%d] = %.4f (expected non-zero)\n",
                   p, vocab.label(p), p, sig[p]);
            ++failures;
        }
    }

    if (failures == 0)
        printf("PASS: vocab_init (%d tokens initialized)\n", vocab.size());
    return failures;
}

static int test_tokenizer_encode_decode() {
    int failures = 0;
    vn::quantum::WHTTokenizer tokenizer;

    // Encode each single-pillar state and verify it decodes to same token
    for (int p = 0; p < NumPillars; p++) {
        PillarStateVector psv;
        std::memset(psv.pillars, 0, sizeof(psv.pillars));
        psv.pillars[p] = 1.0f;

        int token = tokenizer.encode(psv);
        PillarStateVector decoded = tokenizer.decode(token);

        // Should match the original p-th single-pillar token
        // The closest token to (pillar p = 1.0) should be token p
        if (token != p) {
            printf("FAIL encode_decode[%d]: encode returned token %d (%s), expected %d (%s)\n",
                   p, token, tokenizer.vocab.label(token), p, tokenizer.vocab.label(p));
            ++failures;
        }

        // Verify decoded PSV has dominant pillar at position p
        int max_idx = 0;
        for (int i = 1; i < NumPillars; i++)
            if (decoded.pillars[i] > decoded.pillars[max_idx]) max_idx = i;
        if (max_idx != p) {
            printf("FAIL encode_decode[%d]: decoded PSV peak at pillar %d (%.4f), expected %d\n",
                   p, max_idx, decoded.pillars[max_idx], p);
            ++failures;
        }
    }

    if (failures == 0)
        printf("PASS: tokenizer_encode_decode (16 pillars, all correct)\n");
    return failures;
}

static int test_cosine_similarity() {
    int failures = 0;
    vn::quantum::WHTSignalVocabulary vocab;
    // vocab is auto-initialized in constructor

    // Self-similarity of token signals should be 1.0
    for (int i = 0; i < 16; i++) {
        float sim = vn::quantum::WHTTokenizer::cosine_similarity(
            vocab.tokens[i], vocab.tokens[i]);
        if (std::fabs(sim - 1.0f) > 0.001f) {
            printf("FAIL cosine_similarity self[%d]: %.6f (expected 1.0)\n", i, sim);
            ++failures;
        }
    }

    // Different pillar tokens should have lower similarity
    float sim_same = vn::quantum::WHTTokenizer::cosine_similarity(
        vocab.tokens[0], vocab.tokens[0]);
    float sim_diff = vn::quantum::WHTTokenizer::cosine_similarity(
        vocab.tokens[0], vocab.tokens[1]);
    if (sim_diff >= sim_same) {
        printf("FAIL cosine_similarity: cross-token sim (%.4f) >= self-sim (%.4f)\n",
               sim_diff, sim_same);
        ++failures;
    }

    if (failures == 0)
        printf("PASS: cosine_similarity (self=1.0, cross<self)\n");
    return failures;
}

static int test_top_k_encoding() {
    int failures = 0;
    vn::quantum::WHTTokenizer tokenizer;

    PillarStateVector psv;
    psv.pillars[Awareness] = 0.9f;
    psv.pillars[Harm] = 0.1f;
    psv.pillars[Integrity] = 0.5f;
    for (int i = 3; i < NumPillars; i++) psv.pillars[i] = 0.3f;

    int ids[5];
    float scores[5];
    tokenizer.encode_top_k(psv, 5, ids, scores);

    // Top-1 should have positive score
    if (scores[0] <= 0.0f) {
        printf("FAIL top_k_encoding: top-1 score = %.4f (expected positive)\n", scores[0]);
        ++failures;
    }

    // Top-1 score >= top-2 score >= top-3 score
    for (int i = 0; i < 4; i++) {
        if (scores[i] < scores[i + 1]) {
            printf("FAIL top_k_encoding: scores not monotonic [%d]=%.4f < [%d]=%.4f\n",
                   i, scores[i], i + 1, scores[i + 1]);
            ++failures;
        }
    }

    // All returned IDs should be valid
    for (int i = 0; i < 5; i++) {
        if (ids[i] < 0 || ids[i] >= (int)vn::quantum::WHTSignalVocabulary::VOCAB_SIZE) {
            printf("FAIL top_k_encoding: invalid token ID %d at position %d\n", ids[i], i);
            ++failures;
        }
    }

    if (failures == 0) {
        printf("PASS: top_k_encoding (top-1=%-20s score=%.4f)\n",
               tokenizer.vocab.label(ids[0]), scores[0]);
    }
    return failures;
}

static int test_decode_sequence() {
    int failures = 0;
    vn::quantum::WHTTokenizer tokenizer;

    int ids[] = {0, 1, 2};  // Awareness, Willpower, Force
    PillarStateVector avg = tokenizer.decode_sequence(ids, 3);

    // Each of the three decoded pillars should have positive contribution
    if (avg.pillars[0] < 0.01f) {
        printf("FAIL decode_sequence[0]: %.4f (expected >0)\n", avg.pillars[0]);
        ++failures;
    }
    if (avg.pillars[1] < 0.01f) {
        printf("FAIL decode_sequence[1]: %.4f (expected >0)\n", avg.pillars[1]);
        ++failures;
    }
    if (avg.pillars[2] < 0.01f) {
        printf("FAIL decode_sequence[2]: %.4f (expected >0)\n", avg.pillars[2]);
        ++failures;
    }

    // Other pillars should be near zero
    for (int i = 3; i < NumPillars; i++) {
        if (avg.pillars[i] > 0.1f) {
            printf("FAIL decode_sequence[%d]: %.4f (expected near 0)\n", i, avg.pillars[i]);
            ++failures;
        }
    }

    if (failures == 0)
        printf("PASS: decode_sequence (3-token average)\n");
    return failures;
}

static int test_random_input_encoding() {
    int failures = 0;
    vn::quantum::WHTTokenizer tokenizer;

    for (int t = 0; t < 10; t++) {
        PillarStateVector psv;
        for (int i = 0; i < NumPillars; i++)
            psv.pillars[i] = (float)(rand() % 1000) / 1000.0f;

        int token_id = tokenizer.encode(psv);
        if (token_id < 0 || token_id >= (int)vn::quantum::WHTSignalVocabulary::VOCAB_SIZE) {
            printf("FAIL random_encoding[%d]: invalid token %d\n", t, token_id);
            ++failures;
        }

        // Encode should return a valid, labeled token
        const char* label = tokenizer.vocab.label(token_id);
        if (!label || label[0] == '\0') {
            printf("FAIL random_encoding[%d]: empty label for token %d\n", t, token_id);
            ++failures;
        }
    }

    if (failures == 0)
        printf("PASS: random_input_encoding (10 random PSVs, all valid)\n");
    return failures;
}

static int test_wht_tokenizer() {
    int failures = 0;
    failures += test_vocab_init();
    failures += test_tokenizer_encode_decode();
    failures += test_cosine_similarity();
    failures += test_top_k_encoding();
    failures += test_decode_sequence();
    failures += test_random_input_encoding();
    if (failures == 0)
        printf("ALL WHT tokenizer tests PASS\n");
    else
        printf("WHT tokenizer tests: %d FAILURES\n", failures);
    return failures;
}

} // namespace tests
} // namespace vn

inline void run_wht_tokenizer_tests() {
    vn::tests::test_wht_tokenizer();
}
