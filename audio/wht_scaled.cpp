#include "wht_scaled.h"

void fwht_fp(vn::fp20_t* data, int log2n, bool normalize) {
    int n = 1 << log2n;
    for (int len = 1; len < n; len <<= 1) {
        for (int i = 0; i < n; i += (len << 1)) {
            for (int j = 0; j < len; j++) {
                vn::fp20_t temp = data[i + j];
                data[i + j] = temp + data[i + j + len];
                data[i + j + len] = temp - data[i + j + len];
            }
        }
    }
    if (normalize) {
        vn::fp20_t inv_n = vn::fp_sqrt(vn::fp20_t(n));
        for (int i = 0; i < n; i++) {
            data[i] = data[i] / inv_n;
        }
    }
}

void ifwht_fp(vn::fp20_t* data, int log2n, bool normalize) {
    int n = 1 << log2n;
    // Inverse: apply fwht again (WHT is self-inverse up to scale factor n)
    fwht_fp(data, log2n, false);
    // Divide by n: WHT(WHT(x)) = n*x, so divide each element by n
    for (int i = 0; i < n; i++) {
        data[i] = data[i] / vn::fp20_t(static_cast<float>(n));
    }
    if (normalize) {
        // For unitary normalization, multiply back by sqrt(n) since the
        // forward call's normalize factor of 1/sqrt(n) has been applied
        vn::fp20_t sqrt_n = vn::fp_sqrt(vn::fp20_t(n));
        for (int i = 0; i < n; i++) {
            data[i] = data[i] * sqrt_n;
        }
    }
}
