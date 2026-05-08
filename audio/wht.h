#pragma once
#include <cstdint>

#define WHT_N 32
#define WHT_LOG2_N 5  // log₂(32)

// Fast Walsh-Hadamard Transform (in-place, O(n log n))
// Uses only addition and subtraction (no multiplies!)
void fwht(float* x, int log2n);  // Forward: x → Hadamard domain
void ifwht(float* x, int log2n); // Inverse: Hadamard → time domain

// Precompute: Transform weights matrix to Hadamard domain
void precompute_wht_weights(float weights[WHT_N][WHT_N], 
                           float wht_weights[WHT_N][WHT_N]);
