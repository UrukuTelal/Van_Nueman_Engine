#include "wht.h"
#include <cmath>

// Fast Walsh-Hadamard Transform (butterfly algorithm)
// In-place: x is transformed in place
// log2n: log2 of array size (e.g., 5 for 32 elements)
void fwht(float* x, int log2n) {
    int n = 1 << log2n;
    float temp;
    
    // Butterfly operations
    for (int len = 1; len < n; len <<= 1) {
        for (int i = 0; i < n; i += (len << 1)) {
            for (int j = 0; j < len; j++) {
                temp = x[i + j];
                x[i + j] = temp + x[i + j + len];
                x[i + j + len] = temp - x[i + j + len];
            }
        }
    }
    
    // Normalize by 1/sqrt(n) for unitary transform
    float norm = 1.0f / sqrtf((float)n);
    for (int i = 0; i < n; i++) {
        x[i] *= norm;
    }
}

// Inverse FWHT (same as forward for normalized FWHT)
void ifwht(float* x, int log2n) {
    int n = 1 << log2n;
    float temp;
    
    // Butterfly operations (same as forward)
    for (int len = 1; len < n; len <<= 1) {
        for (int i = 0; i < n; i += (len << 1)) {
            for (int j = 0; j < len; j++) {
                temp = x[i + j];
                x[i + j] = temp + x[i + j + len];
                x[i + j + len] = temp - x[i + j + len];
            }
        }
    }
    
    // Normalize by 1/sqrt(n) - same as forward for inverse
    float norm = 1.0f / sqrtf((float)n);
    for (int i = 0; i < n; i++) {
        x[i] *= norm;
    }
}

// Precompute WHT weights: transform each row of weights matrix to Hadamard domain
void precompute_wht_weights(float weights[WHT_N][WHT_N], 
                           float wht_weights[WHT_N][WHT_N]) {
    for (int row = 0; row < WHT_N; row++) {
        // Copy row to temp array
        float temp[WHT_N];
        for (int i = 0; i < WHT_N; i++) {
            temp[i] = weights[row][i];
        }
        
        // Transform to Hadamard domain
        fwht(temp, WHT_LOG2_N);
        
        // Store result
        for (int i = 0; i < WHT_N; i++) {
            wht_weights[row][i] = temp[i];
        }
    }
}
