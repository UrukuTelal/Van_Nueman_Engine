// Test: Multi-target compilation
// This file should compile to CPU, PTX, and SPIR-V

#include "../../include/vn/PillarTypes.h"

using namespace vn;

// Simple kernel-like function
// This demonstrates code that can target multiple backends
extern "C" __attribute__((always_inline))
void pillar_update_kernel(PillarStateVector* pillars,
                          int32_t count,
                          float dt) {
  // Simple loop - should work on CPU, CUDA, and Vulkan
  for (int i = 0; i < count; i++) {
    // Awareness: sample world state (simplified)
    fp20_t awareness = pillars[i][PillarIndex::Awareness];

    // Willpower: drift toward baseline
    fp20_t willpower = pillars[i][PillarIndex::Willpower];
    fp20_t force = pillars[i][PillarIndex::Force];

    // Apply force based on willpower
    fp20_t dt_scaled(dt);
    fp20_t new_force = force + ((willpower >> 10) * dt_scaled);
    pillars[i][PillarIndex::Force] = new_force;
  }
}

// CUDA-specific version (for PTX target)
#ifdef __CUDA_ARCH__
__global__ void pillar_cuda_kernel(PillarStateVector* pillars,
                                    int32_t count,
                                    float dt) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  if (idx >= count) return;
  pillar_update_kernel(pillars, 1, dt);
}
#endif

// Vulkan compute shader version (for SPIR-V target)
// In practice, vncc would generate appropriate SPIR-V entry points
#ifdef __SPIRV__
[[spirv::comp]]
void pillar_spirv_kernel() {
  // SPIR-V generation would happen here
}
#endif

int main() {
  // CPU test
  PillarStateVector pillars[10];
  pillar_update_kernel(pillars, 10, 0.033f);  // 30 Hz tick
  return 0;
}
