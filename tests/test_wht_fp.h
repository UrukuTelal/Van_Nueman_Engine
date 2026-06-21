#pragma once

#include <cstdio>
#include <cstdlib>
#include "../audio/wht_scaled.h"

inline void test_wht_roundtrip() {
    printf("Test: WHT fp20 round-trip... ");

    vn::fp20_t data[32];
    vn::fp20_t original[32];

    for (int i = 0; i < 32; i++) {
        int64_t r = (rand() % 1048576) - 524288;
        data[i] = vn::fp20_t::from_raw(r);
        original[i] = data[i];
    }

    fwht_fp(data, 5, false);
    ifwht_fp(data, 5, false);

    bool pass = true;
    vn::fp20_t eps(0.001f);
    for (int i = 0; i < 32; i++) {
        vn::fp20_t diff = data[i] - original[i];
        if (diff < vn::fp20_t(0)) diff = vn::fp20_t(0) - diff;
        if (eps < diff) {
            pass = false;
            break;
        }
    }

    printf("%s\n", pass ? "PASS" : "FAIL");
}

inline void run_wht_fp_tests() {
    test_wht_roundtrip();
}
