#ifndef VN_SCALED_INT_TOOLCHAIN_H
#define VN_SCALED_INT_TOOLCHAIN_H

#include <cstdint>
#include <type_traits>
#include <limits>

namespace vn {

constexpr int32_t FP_BITS = 20;
constexpr int64_t FP_SCALE = int64_t(1) << FP_BITS;
constexpr int64_t FP_MASK = FP_SCALE - 1;

template<typename T, int N = FP_BITS>
class ScaledInt {
  static_assert(std::is_integral<T>::value, "ScaledInt requires integral type");

  T value_;

public:
  static constexpr int SCALE = N;
  static constexpr T SCALE_FACTOR = T(1) << N;

  constexpr ScaledInt() : value_(0) {}
  ScaledInt(const ScaledInt&) = default;
  ScaledInt& operator=(const ScaledInt&) = default;
  ScaledInt& operator=(float f) { value_ = static_cast<T>(f * SCALE_FACTOR); return *this; }
  ScaledInt& operator=(double f) { value_ = static_cast<T>(f * SCALE_FACTOR); return *this; }
  ScaledInt& operator=(int val) { value_ = static_cast<T>(val * SCALE_FACTOR); return *this; }
  ScaledInt& operator=(unsigned int val) { value_ = static_cast<T>(val * SCALE_FACTOR); return *this; }

  constexpr explicit ScaledInt(float f) : value_(static_cast<T>(f * SCALE_FACTOR)) {}
  constexpr explicit ScaledInt(double f) : value_(static_cast<T>(f * SCALE_FACTOR)) {}
  constexpr explicit ScaledInt(int val) : value_(static_cast<T>(val) * SCALE_FACTOR) {}
  constexpr explicit ScaledInt(unsigned int val) : value_(static_cast<T>(val) * SCALE_FACTOR) {}
  constexpr explicit ScaledInt(T raw) : value_(raw) {}

  float to_float() const { return static_cast<float>(value_) / static_cast<float>(SCALE_FACTOR); }
  double to_double() const { return static_cast<double>(value_) / static_cast<double>(SCALE_FACTOR); }
  T raw() const { return value_; }

  ScaledInt operator+(ScaledInt o) const { return ScaledInt(value_ + o.value_); }
  ScaledInt operator-(ScaledInt o) const { return ScaledInt(value_ - o.value_); }
  ScaledInt operator*(ScaledInt o) const { return ScaledInt((value_ * o.value_) >> N); }
  ScaledInt operator/(ScaledInt o) const {
    using UT = typename std::make_unsigned<T>::type;
    if (o.value_ == 0) return ScaledInt(T(0));
    if (value_ == std::numeric_limits<T>::min()) return ScaledInt(std::numeric_limits<T>::max());
    bool negative = (value_ < 0);
    UT abs_value = static_cast<UT>(value_);
    if (negative) abs_value = -abs_value;
    UT numerator = abs_value << N;
    UT denominator = static_cast<UT>(o.value_);
    UT q = numerator / denominator;
    UT r = numerator % denominator;
    if (r >= (denominator >> 1)) q += 1;
    if (negative) q = static_cast<UT>(-static_cast<int64_t>(q));
    return ScaledInt(static_cast<T>(q));
  }

  ScaledInt operator<<(int s) const { return ScaledInt(value_ << s); }
  ScaledInt operator>>(int s) const {
      using UT = typename std::make_unsigned<T>::type;
      return ScaledInt(static_cast<T>(static_cast<UT>(value_) >> s));
  }

  ScaledInt& operator+=(ScaledInt o) { value_ += o.value_; return *this; }
  ScaledInt& operator-=(ScaledInt o) { value_ -= o.value_; return *this; }

  bool operator==(ScaledInt o) const { return value_ == o.value_; }
  bool operator!=(ScaledInt o) const { return value_ != o.value_; }
  bool operator<(ScaledInt o) const { return value_ < o.value_; }
  bool operator>(ScaledInt o) const { return value_ > o.value_; }
  bool operator<=(ScaledInt o) const { return value_ <= o.value_; }
  bool operator>=(ScaledInt o) const { return value_ >= o.value_; }

  ScaledInt operator-() const { return ScaledInt(-value_); }

  ScaledInt abs() const {
    if (value_ < 0) return ScaledInt(-value_);
    return *this;
  }

  static ScaledInt from_raw(T raw) { return ScaledInt(raw); }
  static ScaledInt from_float(float f) { return ScaledInt(f); }
};

using fp20_t = ScaledInt<int64_t, FP_BITS>;

template<typename T, int N>
inline ScaledInt<T, N> clamp(ScaledInt<T, N> value, ScaledInt<T, N> lo, ScaledInt<T, N> hi) {
  if (value < lo) return lo;
  if (value > hi) return hi;
  return value;
}

template<typename T, int N>
inline ScaledInt<T, N> min(ScaledInt<T, N> a, ScaledInt<T, N> b) {
  return (a < b) ? a : b;
}

template<typename T, int N>
inline ScaledInt<T, N> max(ScaledInt<T, N> a, ScaledInt<T, N> b) {
  return (a > b) ? a : b;
}

} // namespace vn

#endif // VN_SCALED_INT_TOOLCHAIN_H
