//===-- ScaledInteger.cpp - Scaled Integer Handling -------------------===//
//
// Handles [[scaled(N)]] attribute for integer types
// Implements implicit conversions between scaled and float types
//
//===----------------------------------------------------------------------===//

#include <iostream>
#include <string>

namespace vn {

// Scaled integer type info
struct ScaledIntInfo {
  unsigned ScaleFactor;  // N in 2^N

  ScaledIntInfo(unsigned SF) : ScaleFactor(SF) {}

  // Convert float to scaled integer
  // value * (1 << ScaleFactor)
  std::string floatToScaled(const std::string& value) const {
    return "(int)(" + value + " * (1 << " + std::to_string(ScaleFactor) + "))";
  }

  // Convert scaled integer to float
  // value / (1 << ScaleFactor)
  std::string scaledToFloat(const std::string& value) const {
    return "(float)" + value + " / (1 << " + std::to_string(ScaleFactor) + ")";
  }
};

// Type checker for scaled integer operations
class ScaledIntegerChecker {
public:
  ScaledIntegerChecker() {}

  // Check if an expression has scaled attribute
  bool isScaledType(const std::string& typeName) {
    return typeName.find("ScaledInt") != std::string::npos;
  }

  // Get scale factor for a type
  unsigned getScaleFactor(const std::string& typeName) {
    // Parse type name to extract scale factor
    // e.g., ScaledInt<int32_t,20> -> 20
    return 20; // Default: 2^20
  }

  // Validate binary operation on scaled integers
  bool checkBinOp(const std::string& LHS, const std::string& RHS) {
    unsigned LScale = getScaleFactor(LHS);
    unsigned RScale = getScaleFactor(RHS);

    if (LScale != RScale) {
      std::cerr << "vncc: warning: scaled integer scale mismatch\n";
    }
    return true;
  }
};

} // namespace vn
