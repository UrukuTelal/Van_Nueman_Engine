# Issue #011: ScaledInt.h — Multiple critical math bugs (MSVC paths)

**Severity:** Critical (silent numerical corruption)  
**Phase:** 1 (Critical)  
**File:** `vn/ScaledInt.h`

## Bug 1: Division — MSVC `_mul128` path overflows for large numerators

Line 92 (`#if defined(_MSC_VER) && defined(_M_X64) && !defined(__SIZEOF_INT128__)` path):

```cpp
uint64_t abs_num = static_cast<uint64_t>(negative ? -num : num);
uint64_t numerator = abs_num << N;  // N = 20
uint64_t q = numerator / abs_den;
```

If `abs_num > 2^44`, then `abs_num << 20` overflows `uint64_t`, silently dropping the high bits. This produces garbage quotient for any division where the scaled numerator exceeds 2^44.

**Example:** `fp20_t(5000.0) / fp20_t(2.0)`
- `abs_num = 5000 * 2^20 = 5,242,880,000` (OK, < 2^44)
- `abs_num << 20 = 5,242,880,000 * 2^20 = 5.5e15` (OK, < 2^63)

**But:** `fp20_t(500000.0) / fp20_t(2.0)`
- `abs_num = 500000 * 2^20 = 524,288,000,000` (OK)
- `abs_num << 20 = 524,288,000,000 * 2^20 = 5.5e17` (OK still < 2^63)

Actually, the problem only manifests for values close to 2^63-1. The threshold is `abs_num > 2^44` which is about `2^44 / 2^20 = 2^24 = 16 million`. So any value > 16 million divided by another would overflow on MSVC.

Wait, let me recalculate:
- fp20_t stores values as int64_t / 2^20
- So value 1.0 = 2^20 = 1048576
- Value 16 million = 16,777,216 * 2^20 = 2^44
- If abs_num > 2^44, then `abs_num << 20` as uint64_t would overflow

So for values > 16 million (in the unscaled representation), the MSVC path overflows. However, since pillar values are in [0,1], this wouldn't trigger for pillar computations. But other uses of ScaledInt with larger values would be affected.

The non-MSVC path (lines 100-118) uses `__int128` which is safe.

**Impact:** Silent numerical corruption on MSVC builds (the primary target platform).

## Bug 2: Division by zero returns max() instead of error

Line 79-81:

```cpp
if (o.value_ == 0) {
    return ScaledInt(std::numeric_limits<T>::max());
}
```

Division by zero returns `T::max()` (~9.22e18 for int64_t) instead of signaling an error or returning infinity/NaN. This silently replaces zero-divide with a large positive value, which can cascade through subsequent calculations without detection.

## Bug 3: MSVC sqrt path overflows for large values

Line 220:

```cpp
T quotient = static_cast<T>((static_cast<uint64_t>(val) << N) / static_cast<uint64_t>(guess));
```

Same overflow pattern as division: `val << N` wraps uint64_t for val > 2^44. The non-MSVC `__int128` path (line 232) is correct.

## Bug 4: Right-shift of negative value in operator>>

Line 122:

```cpp
ScaledInt operator>>(int s) const { return ScaledInt(value_ >> s); }
```

Right-shifting a negative `int64_t` value_ is implementation-defined in C++ (though arithmetic in practice on all major compilers). For platform-independent correctness, cast through uint64_t.

## Impact Summary

| Bug | Path | Severity |
|-----|------|----------|
| Division overflow | MSVC _mul128 | High (wrong results for values > 2^44) |
| Division by zero | All | Medium (silent error) |
| sqrt overflow | MSVC | Medium (wrong sqrt for large values) |
| Signed right-shift | All (operator>>) | Low (implementation-defined, works in practice) |

## Recommended Fix

1. **Division overflow:** Guard the MSVC path with a range check:
   ```cpp
   if (abs_num > (std::numeric_limits<uint64_t>::max() >> N)) {
       // Fall back to wider arithmetic or clamp
   }
   ```

2. **Division by zero:** Add assertion or return a signaling NaN pattern:
   ```cpp
   if (o.value_ == 0) {
       // Or: assert(false && "division by zero");
       return ScaledInt();  // return 0 instead of max
   }
   ```

3. **sqrt overflow:** Same range guard for MSVC path.

4. **operator>>:** Cast to unsigned before shift, cast back after.
