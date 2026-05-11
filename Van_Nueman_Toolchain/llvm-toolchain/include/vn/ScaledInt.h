#ifndef VN_SCALED_INT_H
#define VN_SCALED_INT_H

#include <cstdint>
#include <type_traits>

namespace vn {

// Scaled integer template
// Represents a fixed-point number where 1 = 2^N
// N is the scale factor (default 20 as per architecture spec)
template<typename T, int N = 20>
class ScaledInt {
  static_assert(std::is_integral<T>::value, "ScaledInt requires integral type");

  T value;

public:
  static constexpr int SCALE = N;
  static constexpr T SCALE_FACTOR = T(1) << N;

  // Constructor from float
  explicit ScaledInt(float f) : value(static_cast<T>(f * SCALE_FACTOR)) {}

  // Default constructor
  ScaledInt() : value(0) {}

  // Constructor from raw scaled value
  explicit ScaledInt(T raw) : value(raw) {}

  // Convert to float
  float to_float() const { return static_cast<float>(value) / SCALE_FACTOR; }

  // Get raw value
  T raw() const { return value; }

  // Arithmetic operations (preserve scaled representation)
  ScaledInt operator+(ScaledInt other) const {
    return ScaledInt(value + other.value);
  }

  ScaledInt operator-(ScaledInt other) const {
    return ScaledInt(value - other.value);
  }

  ScaledInt operator*(ScaledInt other) const {
    // Note: This is simplified - real implementation needs overflow handling
    return ScaledInt((value * other.value) >> N);
  }

  ScaledInt operator>>(int shift) const {
    return ScaledInt(value >> shift);
  }

  ScaledInt operator<<(int shift) const {
    return ScaledInt(value << shift);
  }
};

// Common typedefs
using ScaledInt32 = ScaledInt<int32_t, 20>;

} // namespace vn

#endif // VN_SCALED_INT_H
