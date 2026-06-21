// test_scaled_int.h - Unit tests for ScaledInt (1 = 2**20)
#pragma once

#include <cassert>
#include <cstdio>
#include "../vn/ScaledInt.h"

inline void test_scaled_int_creation() {
    printf("Test: ScaledInt creation... ");
    vn::fp20_t a;
    a = vn::fp20_t(1.0f);
    assert(a.raw() == (int64_t(1) << 20));
    printf("PASS (raw=%lld)\n", (long long)a.raw());
}

inline void test_scaled_int_addition() {
    printf("Test: ScaledInt addition... ");
    vn::fp20_t a(1.0f), b(2.0f);
    vn::fp20_t c = a + b;
    // 1.0 + 2.0 = 3.0 = 3 * 2**20
    assert(c.raw() == 3 * (int64_t(1) << 20));
    printf("PASS (raw=%lld)\n", (long long)c.raw());
}

inline void test_scaled_int_multiplication() {
    printf("Test: ScaledInt multiplication... ");
    vn::fp20_t a(0.5f), b(2.0f);
    // 0.5 * 2.0 = 1.0 = 2**20
    assert(a * b == vn::fp20_t(1.0f));
    printf("PASS (raw=%lld)\n", (long long)(a * b).raw());
}

inline void test_scaled_int_to_float() {
    printf("Test: ScaledInt to float... ");
    vn::fp20_t a(1.0f);
    float f = a.to_float();
    assert(f == 1.0f);
    printf("PASS (float=%.2f)\n", f);
}

inline void test_scaled_int_bitwise() {
    printf("Test: ScaledInt bitwise AND... ");
    vn::fp20_t a = vn::fp20_t::from_raw((int64_t(1) << 20) | (int64_t(1) << 10));
    vn::fp20_t mask = vn::fp20_t::from_raw(int64_t(1) << 20);
    int64_t result = a.raw() & mask.raw();
    assert(result == (int64_t(1) << 20));
    printf("PASS (raw=%lld)\n", (long long)result);
}

inline void run_scaled_int_tests() {
    printf("=== ScaledInt Tests ===\n");
    test_scaled_int_creation();
    test_scaled_int_addition();
    test_scaled_int_multiplication();
    test_scaled_int_to_float();
    test_scaled_int_bitwise();
    printf("=== All ScaledInt Tests PASSED ===\n\n");
}
