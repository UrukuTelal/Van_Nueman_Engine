//===-- test_spirv.cpp - Test SPIR-V Compilation -===//
//
// Tests that vncc can compile C++ to SPIR-V via LLVM IR translation.
//
//===----------------------------------------------------------------------===//

#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>

// Simple kernel for testing SPIR-V output
// Compile with: vncc -emit-spirv test_spirv.cpp -o test.spv

[[gnu::always_inline]] 
void simple_kernel(int32_t* data, int32_t value) {
  // Simple store operation
  if (data) {
    *data = value;
  }
}

int main() {
  std::cout << "SPIR-V Test Kernel\n";
  std::cout << "Compile with: vncc -emit-spirv test_spirv.cpp -o test.spv\n";
  
  // Verify SPIR-V file starts with SPIR-V magic number
  std::ifstream spv("test.spv", std::ios::binary);
  if (spv) {
    uint32_t magic;
    spv.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    if (magic == 0x07230203) {  // SPIR-V magic number
      std::cout << "Valid SPIR-V file!\n";
    } else {
      std::cout << "Invalid SPIR-V file (magic: 0x" << std::hex << magic << ")\n";
    }
  } else {
    std::cout << "No SPIR-V file found (run vncc first)\n";
  }
  
  return 0;
}
