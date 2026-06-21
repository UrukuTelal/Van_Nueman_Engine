// Test: Pillar attributes
// This file tests that [[pillar_vector]] and [[scaled(20)]] attributes
// are properly parsed by vncc

#include "vn/PillarTypes.h"

// Test 1: PillarVector struct with attribute
struct MyPillarVector : public vn::PillarStateVector {
  // Inherits p[14] from base
} __attribute__((annotate("pillar_vector")));

// Test 2: Scaled integer with attribute
void test_scaled() {
  int32_t scaled_val __attribute__((annotate("scaled(20)"))) = 1048576; // 1.0 in scaled form

  // Should compile without errors
  int32_t raw = scaled_val;

  // Implicit conversion (handled by vncc)
  float f = (float)scaled_val / (1 << 20);
}

// Test 3: Function using pillar vector
void process_pillars(MyPillarVector& p) {
  // Access pillar values
  p.p[(int)vn::PillarIndex::Awareness] = vn::fp20_t(0.5f);

  // Read back
  float awareness = p.p[(int)vn::PillarIndex::Awareness].to_float();
}

int main() {
  MyPillarVector pv;
  pv.set(vn::PillarIndex::Force, 0.8f);
  process_pillars(pv);
  return 0;
}
