#pragma once

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include "../audio/wht.h"

// Verify Parseval's theorem: fwht(x) · fwht(y) = x · y
inline void test_wht_parseval() {
    printf("Test: WHT Parseval (dot product preservation)... ");
    float x[32], y[32];
    for (int i = 0; i < 32; i++) {
        x[i] = (float)(rand() % 1000) / 100.0f;
        y[i] = (float)(rand() % 1000) / 100.0f;
    }

    float dot_orig = 0.0f;
    for (int i = 0; i < 32; i++) dot_orig += x[i] * y[i];

    float fx[32], fy[32];
    memcpy(fx, x, sizeof(fx));
    memcpy(fy, y, sizeof(fy));
    fwht(fx, WHT_LOG2_N);
    fwht(fy, WHT_LOG2_N);

    float dot_wht = 0.0f;
    for (int i = 0; i < 32; i++) dot_wht += fx[i] * fy[i];

    bool pass = fabsf(dot_orig - dot_wht) < 0.001f;
    printf("%s (orig=%f wht=%f)\n", pass ? "PASS" : "FAIL", dot_orig, dot_wht);
    if (!pass) { printf("  ERROR: |%f - %f| >= 0.001\n", dot_orig, dot_wht); exit(1); }
}

// Verify fwht(ifwht(x)) == x (round-trip)
inline void test_wht_float_roundtrip() {
    printf("Test: WHT float round-trip... ");
    float x[32], orig[32];
    for (int i = 0; i < 32; i++) {
        x[i] = (float)(rand() % 2000 - 1000) / 100.0f;
        orig[i] = x[i];
    }

    fwht(x, WHT_LOG2_N);
    ifwht(x, WHT_LOG2_N);

    bool pass = true;
    for (int i = 0; i < 32; i++) {
        float diff = fabsf(x[i] - orig[i]);
        if (diff > 0.001f) { pass = false; break; }
    }
    printf("%s\n", pass ? "PASS" : "FAIL");
    if (!pass) exit(1);
}

// Verify mixing matrix identity: fwht(input) · fwht(weights[out]) == input · weights[out]
inline void test_wht_mixing_matrix() {
    printf("Test: WHT mixing matrix (dot-product identity)... ");
    float input[32];
    float weights[32][32];
    for (int i = 0; i < 32; i++) input[i] = (float)(rand() % 2000 - 1000) / 100.0f;
    for (int r = 0; r < 32; r++)
        for (int c = 0; c < 32; c++)
            weights[r][c] = (float)(rand() % 2000 - 1000) / 100.0f;

    // precompute WHT weights
    float wht_weights[32][32];
    precompute_wht_weights(weights, wht_weights);

    // WHT-domain mixing
    float fx[32];
    memcpy(fx, input, sizeof(fx));
    fwht(fx, WHT_LOG2_N);

    float output_wht[32];
    for (int out = 0; out < 32; out++) {
        float sum = 0.0f;
        for (int i = 0; i < 32; i++) sum += fx[i] * wht_weights[out][i];
        output_wht[out] = sum;
    }

    // Direct mixing (time domain)
    float output_direct[32];
    for (int out = 0; out < 32; out++) {
        float sum = 0.0f;
        for (int i = 0; i < 32; i++) sum += input[i] * weights[out][i];
        output_direct[out] = sum;
    }

    bool pass = true;
    for (int i = 0; i < 32; i++) {
        float diff = fabsf(output_wht[i] - output_direct[i]);
        if (diff > 0.001f) { pass = false; break; }
    }
    printf("%s\n", pass ? "PASS" : "FAIL");
    if (!pass) exit(1);
}

inline void run_wht_audio_tests() {
    test_wht_parseval();
    test_wht_float_roundtrip();
    test_wht_mixing_matrix();
}
