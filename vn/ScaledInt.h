#ifndef VN_SCALED_INT_H
#define VN_SCALED_INT_H

#include <cstdint>
#include <type_traits>
#include <limits>
#include <cmath>
#if defined(_MSC_VER) && defined(_M_X64)
#include <intrin.h>
#endif



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
   // FIXED: Assignment operators now scale int values like float/double versions
   ScaledInt& operator=(int val) { value_ = static_cast<T>(val * SCALE_FACTOR); return *this; }
   ScaledInt& operator=(unsigned int val) { value_ = static_cast<T>(val * SCALE_FACTOR); return *this; }

  constexpr explicit ScaledInt(float f) : value_(static_cast<T>(f * SCALE_FACTOR)) {}
  constexpr explicit ScaledInt(double f) : value_(static_cast<T>(f * SCALE_FACTOR)) {}
  constexpr explicit ScaledInt(int val) : value_(static_cast<T>(val) * SCALE_FACTOR) {}
  constexpr explicit ScaledInt(unsigned int val) : value_(static_cast<T>(val) * SCALE_FACTOR) {}
#if !defined(_MSC_VER)
  constexpr explicit ScaledInt(long long val) : value_(static_cast<T>(val) * SCALE_FACTOR) {}
  constexpr explicit ScaledInt(unsigned long long val) : value_(static_cast<T>(val) * SCALE_FACTOR) {}
#endif
  constexpr explicit ScaledInt(T raw) : value_(raw) {}

  float to_float() const { return static_cast<float>(value_) / static_cast<float>(SCALE_FACTOR); }
  double to_double() const { return static_cast<double>(value_) / static_cast<double>(SCALE_FACTOR); }
  operator float() const { return to_float(); }
  operator double() const { return to_double(); }
  T raw() const { return value_; }

  ScaledInt operator+(ScaledInt o) const { return ScaledInt(value_ + o.value_); }
  ScaledInt operator-(ScaledInt o) const { return ScaledInt(value_ - o.value_); }
    // Signed overflow-safe multiplication
    ScaledInt operator*(ScaledInt o) const {
#if defined(_MSC_VER) && defined(_M_X64) && !defined(__SIZEOF_INT128__)
        if constexpr (std::is_same_v<T, int64_t>) {
            int64_t high;
            int64_t low = _mul128(value_, o.value_, &high);
            uint64_t result = (static_cast<uint64_t>(low) >> N) |
                              (static_cast<uint64_t>(high) << (64 - N));
            return ScaledInt(static_cast<T>(static_cast<int64_t>(result)));
        } else if constexpr (std::is_same_v<T, int32_t>) {
            return ScaledInt(static_cast<T>((static_cast<int64_t>(value_) * static_cast<int64_t>(o.value_)) >> N));
        } else {
            return ScaledInt(static_cast<T>((static_cast<int32_t>(value_) * static_cast<int32_t>(o.value_)) >> N));
        }
#else
        using WideT = typename std::conditional<std::is_same<T, int64_t>::value, __int128,
                            typename std::conditional<std::is_same<T, int32_t>::value, int64_t,
                            typename std::conditional<std::is_same<T, int16_t>::value, int32_t, T>::type>::type>::type;
        WideT product = static_cast<WideT>(value_) * static_cast<WideT>(o.value_);
        return ScaledInt(static_cast<T>(product >> N));
#endif
    }
    ScaledInt operator/(ScaledInt o) const {
      if (o.value_ == 0) {
          return ScaledInt(T(0));
      }
#if defined(_MSC_VER) && defined(_M_X64) && !defined(__SIZEOF_INT128__)
      if constexpr (std::is_same_v<T, int64_t>) {
          if (value_ == std::numeric_limits<T>::min()) return ScaledInt(std::numeric_limits<T>::max());
          int64_t num = value_;
          int64_t den = o.value_;
          if (den < 0) { den = -den; num = -num; }
          if (num == std::numeric_limits<int64_t>::min()) return ScaledInt(std::numeric_limits<T>::max());
          bool negative = (num < 0);
          uint64_t abs_num = static_cast<uint64_t>(negative ? -num : num);
          uint64_t abs_den = static_cast<uint64_t>(den);
          uint64_t q, r;
          if (abs_num > (std::numeric_limits<uint64_t>::max() >> N)) {
              uint64_t numerator_hi = abs_num >> (64 - N);
              uint64_t numerator_lo = abs_num << N;
              q = _udiv128(numerator_hi, numerator_lo, abs_den, &r);
          } else {
              uint64_t numerator = abs_num << N;
              q = _udiv128(0, numerator, abs_den, &r);
          }
          if (r >= (abs_den >> 1)) q += 1;
          if (negative) q = static_cast<uint64_t>(-static_cast<int64_t>(q));
          return ScaledInt(static_cast<T>(q));
      } else
#endif
      {
          using WideT = typename std::conditional<std::is_same<T, int64_t>::value, __int128,
                              typename std::conditional<std::is_same<T, int32_t>::value, int64_t,
                              typename std::conditional<std::is_same<T, int16_t>::value, int32_t, T>::type>::type>::type;
          using UWideT = typename std::make_unsigned<WideT>::type;
          WideT num = static_cast<WideT>(value_);
          WideT den = static_cast<WideT>(o.value_);
          if (den < 0) { den = -den; num = -num; }
          if (num == std::numeric_limits<WideT>::min()) return ScaledInt(std::numeric_limits<T>::max());
          bool negative = (num < 0);
          UWideT abs_num = static_cast<UWideT>(negative ? -num : num);
          UWideT abs_den = static_cast<UWideT>(den);
          UWideT numerator = abs_num << N;
          UWideT q = numerator / abs_den;
          UWideT r = numerator % abs_den;
          if (r >= (abs_den >> 1)) q += 1;
          if (negative) q = static_cast<UWideT>(-static_cast<WideT>(q));
          return ScaledInt(static_cast<T>(q));
      }
    }

  ScaledInt operator<<(int s) const { return ScaledInt(value_ << s); }
   ScaledInt operator>>(int s) const {
       using UT = typename std::make_unsigned<T>::type;
       return ScaledInt(static_cast<T>(static_cast<UT>(value_) >> s));
   }

  ScaledInt& operator+=(ScaledInt o) { value_ += o.value_; return *this; }
  ScaledInt& operator-=(ScaledInt o) { value_ -= o.value_; return *this; }
  ScaledInt& operator*=(ScaledInt o) { *this = *this * o; return *this; }

  bool operator==(ScaledInt o) const { return value_ == o.value_; }
  bool operator!=(ScaledInt o) const { return value_ != o.value_; }
  bool operator<(ScaledInt o) const { return value_ < o.value_; }
  bool operator>(ScaledInt o) const { return value_ > o.value_; }
  bool operator<=(ScaledInt o) const { return value_ <= o.value_; }
  bool operator>=(ScaledInt o) const { return value_ >= o.value_; }

  // Mixed ScaledInt/float operators (convenience — no determinism loss)
  ScaledInt operator+(float f) const { return *this + ScaledInt(f); }
  ScaledInt operator-(float f) const { return *this - ScaledInt(f); }
  ScaledInt operator*(float f) const { return *this * ScaledInt(f); }
  ScaledInt operator/(float f) const { return *this / ScaledInt(f); }
  ScaledInt& operator+=(float f) { return *this += ScaledInt(f); }
  ScaledInt& operator-=(float f) { return *this -= ScaledInt(f); }
  ScaledInt& operator*=(float f) { return *this *= ScaledInt(f); }
  bool operator<(float f) const { return *this < ScaledInt(f); }
  bool operator>(float f) const { return *this > ScaledInt(f); }
  bool operator<=(float f) const { return *this <= ScaledInt(f); }
  bool operator>=(float f) const { return *this >= ScaledInt(f); }
  bool operator==(float f) const { return *this == ScaledInt(f); }
  bool operator!=(float f) const { return *this != ScaledInt(f); }

  ScaledInt operator-() const { return ScaledInt(-value_); }

  ScaledInt saturating_add(ScaledInt o) const {
    T sum = value_ + o.value_;
    if (o.value_ > 0 && sum < value_) return ScaledInt(std::numeric_limits<T>::max());
    if (o.value_ < 0 && sum > value_) return ScaledInt(std::numeric_limits<T>::min());
    return ScaledInt(sum);
  }

  ScaledInt saturating_sub(ScaledInt o) const {
    T diff = value_ - o.value_;
    if (o.value_ > 0 && diff > value_) return ScaledInt(std::numeric_limits<T>::min());
    if (o.value_ < 0 && diff < value_) return ScaledInt(std::numeric_limits<T>::max());
    return ScaledInt(diff);
  }

  ScaledInt abs() const {
    if (value_ < 0) return ScaledInt(-value_);
    return *this;
  }

  static ScaledInt from_raw(T raw) { return ScaledInt(raw); }
  static ScaledInt from_float(float f) { return ScaledInt(f); }
};

template<typename T, int N>
ScaledInt<T, N> abs(ScaledInt<T, N> x) { return x.abs(); }

// Reverse mixed operators: float <op> ScaledInt
template<typename T, int N>
inline ScaledInt<T, N> operator+(float f, ScaledInt<T, N> s) { return ScaledInt<T, N>(f) + s; }
template<typename T, int N>
inline ScaledInt<T, N> operator-(float f, ScaledInt<T, N> s) { return ScaledInt<T, N>(f) - s; }
template<typename T, int N>
inline ScaledInt<T, N> operator*(float f, ScaledInt<T, N> s) { return ScaledInt<T, N>(f) * s; }
template<typename T, int N>
inline ScaledInt<T, N> operator/(float f, ScaledInt<T, N> s) { return ScaledInt<T, N>(f) / s; }

using fp20_t = ScaledInt<int64_t, FP_BITS>;

template<typename T, int N>
inline ScaledInt<T, N> clamp(ScaledInt<T, N> value, ScaledInt<T, N> lo, ScaledInt<T, N> hi) {
  if (value < lo) return lo;
  if (value > hi) return hi;
  return value;
}

template<typename T, int N>
inline ScaledInt<T, N> fp_min(ScaledInt<T, N> a, ScaledInt<T, N> b) {
  return (a < b) ? a : b;
}

template<typename T, int N>
inline ScaledInt<T, N> fp_max(ScaledInt<T, N> a, ScaledInt<T, N> b) {
  return (a > b) ? a : b;
}

template<typename T, int N>
inline ScaledInt<T, N> fp_sqrt(ScaledInt<T, N> x) {
   if (x.raw() <= 0) return ScaledInt<T, N>();
   T val = x.raw();
   if (val <= T(1)) return ScaledInt<T, N>::from_raw(val);
#if defined(_MSC_VER) && defined(_M_X64) && !defined(__SIZEOF_INT128__)
   T guess = val >> 1;
   for (int i = 0; i < 6; i++) {
       if (guess == 0) break;
       int64_t high;
       int64_t low = _mul128(static_cast<int64_t>(guess), static_cast<int64_t>(guess), &high);
       uint64_t guess_sq = (static_cast<uint64_t>(low) >> N) |
                           (static_cast<uint64_t>(high) << (64 - N));
       T quotient;
       if (static_cast<uint64_t>(val) > (std::numeric_limits<uint64_t>::max() >> N)) {
           uint64_t r;
           quotient = static_cast<T>(_udiv128(static_cast<uint64_t>(val) >> (64 - N),
                                              static_cast<uint64_t>(val) << N,
                                              static_cast<uint64_t>(guess), &r));
       } else {
           quotient = static_cast<T>((static_cast<uint64_t>(val) << N) / static_cast<uint64_t>(guess));
       }
       if (quotient > std::numeric_limits<T>::max() / 2) { guess = (guess + (T(1) << N)) >> 1; continue; }
       guess = (guess + quotient) >> 1;
       if (guess_sq == static_cast<uint64_t>(val) || guess_sq == static_cast<uint64_t>(val) + 1) break;
   }
#else
   using WideT = typename std::conditional<std::is_same<T, int64_t>::value, __int128,
                       typename std::conditional<std::is_same<T, int32_t>::value, int64_t,
                       typename std::conditional<std::is_same<T, int16_t>::value, int32_t, T>::type>::type>::type;
   WideT guess = static_cast<WideT>(val) >> 1;
   for (int i = 0; i < 6; i++) {
       if (guess == 0) break;
       WideT quotient = (static_cast<WideT>(val) << N) / guess;
       guess = (guess + quotient) >> 1;
   }
#endif
   return ScaledInt<T, N>::from_raw(static_cast<T>(guess));
}

} // namespace vn

#endif // VN_SCALED_INT_H
