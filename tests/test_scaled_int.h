// test_scaled_int.h - Unit tests for ScaledInt (1 = 2**20)
#pragma once

#include <cassert>
#include <cstdio>
#include "../vn/ScaledInt.h"

inline void test_scaled_int_creation() {
    printf("Test: ScaledInt creation... ");
    ScaledInt<int, 1 << 20> a;  // Template args required
    a.set_float(1.0f);  // Should be 2**20
    assert(a.raw_value == (1 << 20));
    printf("PASS (raw=%d)\n", a.raw_value);
}

inline void test_scaled_int_addition() {
    printf("Test: ScaledInt addition... ");
    ScaledInt<int, 1 << 20> a, b;
    a.set_float(1.0f);  // 2**20
    b.set_float(2.0f);  // 2**21
    ScaledInt<int, 1 << 20> c = a + b;
    // 1.0 + 2.0 = 3.0 = 3 * 2**20
    assert(c.raw_value == 3 * (1 << 20));
    printf("PASS (raw=%d)\n", c.raw_value);
}

inline void test_scaled_int_multiplication() {
    printf("Test: ScaledInt multiplication... ");
    ScaledInt<int64_t, 1 << 20> a, b;  // Use int64_t to avoid overflow
    a.set_float(0.5f);  // raw = 0.5 * 2^20 = 524288
    b.set_float(2.0f);  // raw = 2.0 * 2^20 = 2097152
    // For fixed-point: a * b = (a.raw_value * b.raw_value) / SCALE
    int64_t result = (a.raw_value * b.raw_value) / (1 << 20);
    // 0.5 * 2.0 = 1.0 = 2**20
    assert(result == (1 << 20));
    printf("PASS (raw=%lld)\n", (long long)result);
}

inline void test_scaled_int_to_float() {
    printf("Test: ScaledInt to float... ");
    ScaledInt<int, 1 << 20> a;
    a.set_float(1.0f);
    float f = a.to_float();
    assert(f == 1.0f);
    printf("PASS (float=%.2f)\n", f);
}

inline void test_scaled_int_bitwise() {
    printf("Test: ScaledInt bitwise AND... ");
    ScaledInt<int, 1 << 20> a, mask;
    a.raw_value = (1 << 20) | (1 << 10);  // 1.0 + small offset
    mask.raw_value = (1 << 20);  // Mask for integer part
    int result = a.raw_value & mask.raw_value;
    assert(result == (1 << 20));
    printf("PASS (raw=%d)\n", result);
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
