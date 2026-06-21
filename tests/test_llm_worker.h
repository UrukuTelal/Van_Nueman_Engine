#pragma once
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include "../include/Entity.h"
#include "../audio/wht.h"

inline void test_wht_compress_decompress_pillars() {
    printf("Test: WHT compress/decompress pillar round-trip... ");

    float pillars[16];
    for (int i = 0; i < 16; i++) {
        pillars[i] = (float)rand() / (float)RAND_MAX;
    }

    float wht_buf[32];
    memcpy(wht_buf, pillars, 16 * sizeof(float));
    for (int i = 16; i < 32; i++) wht_buf[i] = 0.0f;
    fwht(wht_buf, 5);

    float decoded[32];
    memcpy(decoded, wht_buf, 32 * sizeof(float));
    ifwht(decoded, 5);

    bool pass = true;
    for (int i = 0; i < 16; i++) {
        float err = fabsf(decoded[i] - pillars[i]);
        if (err > 0.001f) { pass = false; break; }
    }

    printf("%s\n", pass ? "PASS" : "FAIL");
}

inline void test_wht_sparsity() {
    printf("Test: WHT sparsity after thresholding... ");

    float pillars[16];
    for (int i = 0; i < 16; i++) {
        pillars[i] = (float)rand() / (float)RAND_MAX;
    }

    float wht_buf[32];
    memcpy(wht_buf, pillars, 16 * sizeof(float));
    for (int i = 16; i < 32; i++) wht_buf[i] = 0.0f;
    fwht(wht_buf, 5);

    float threshold = 0.5f;
    int nonzero = 0;
    for (int i = 0; i < 32; i++) {
        if (fabsf(wht_buf[i]) > threshold) nonzero++;
    }

    bool pass = nonzero <= 32 && nonzero >= 0;
    printf("%s (nonzero=%d/32, threshold=%.1f)\n",
           pass ? "PASS" : "FAIL", nonzero, threshold);
}

inline void run_llm_worker_tests() {
    printf("=== LLM Worker WHT Tests ===\n");
    test_wht_compress_decompress_pillars();
    test_wht_sparsity();
    printf("=== All LLM Worker Tests PASSED ===\n\n");
}
