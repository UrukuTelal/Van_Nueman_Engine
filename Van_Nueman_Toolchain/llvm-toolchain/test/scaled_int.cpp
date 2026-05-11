// Test: Scaled integer operations
// Tests ScaledInt template class and [[scaled(N)]] attribute

#include "../../include/vn/ScaledInt.h"
#include <cstdio>

using namespace vn;

void test_scaled_int_basic() {
  // Create scaled integers
  ScaledInt32 a(1.0f);  // 1.0 in scaled form
  ScaledInt32 b(2.5f);  // 2.5 in scaled form

  // Arithmetic
  ScaledInt32 c = a + b;  // Should be 3.5
  ScaledInt32 d = b - a;  // Should be 1.5

  // Convert back to float
  float cf = c.to_float();
  float df = d.to_float();

  printf("a = %f, b = %f\n", a.to_float(), b.to_float());
  printf("a + b = %f\n", cf);
  printf("b - a = %f\n", df);
}

void test_scaled_int_bitops() {
  ScaledInt32 val(1.0f);

  // Bit shift operations (optimized by compiler)
  ScaledInt32 shifted = val >> 10;  // Divide by 1024
  printf("1.0 >> 10 (scaled) = %f\n", shifted.to_float());
}

// Test with attribute syntax (would be handled by vncc)
void test_attribute_syntax() {
  [[scaled(20)]] int32_t x = 1048576;  // 1.0
  [[scaled(20)]] int32_t y = 2097152;  // 2.0

  // Implicit conversion handled by vncc
  float result = (float)(x + y) / (1 << 20);
  printf("Result: %f\n", result);
}

int main() {
  test_scaled_int_basic();
  test_scaled_int_bitops();
  test_attribute_syntax();
  return 0;
}
