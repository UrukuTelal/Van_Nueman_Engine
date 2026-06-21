# Issue #016: skelly_compute.cu — float3 operator+ (previously missing), M_PI not standard, unused parameter

**Severity:** High (CUDA compilation on strict platforms)  
**Phase:** 1 (Critical)  
**File:** `kernels/skelly_compute.cu`

## Status: float3 operator+ (previously fixed)

Line 15:

```cuda
__device__ float3 operator+(float3 a, float3 b) { return make_float3(a.x+b.x, a.y+b.y, a.z+b.z); }
```

CUDA's built-in `float3` does not ship with arithmetic operators — they must be user-defined. This file now defines `operator+`, `operator-`, `operator*` (float3 × scalar), and `dot`/`length` helpers (lines 15-19), so `operator+` is no longer missing. **Previously fixed.**

## Remaining issues

### 1. M_PI not standard in CUDA (line 134)

```cuda
mg.current_volume += M_PI * strand.current_r * strand.current_r * curr_len;
```

`M_PI` is not part of the CUDA standard library and is not guaranteed to be defined. On strict CUDA builds (e.g., with `--fast-math` or certain compilation flags), this will fail to compile. Correct approach:

```cuda
#define _USE_MATH_DEFINES
#include <cmath>
// Then use M_PI, or define: constexpr float PI = 3.14159265358979f;
```

### 2. Unused parameter (line 140)

```cuda
__global__ void skelly_operate_organs_kernel(Organ* organs, uint32_t count,
                                              Transport* transports, PillarStateVector* entity_pillars) {
```

`entity_pillars` is never used in the kernel body. If meant for future use, it should be marked with `(void)entity_pillars;` to suppress compiler warnings. Current compiler flags may treat unused parameters as errors.

### 3. sqrtf argument not guarded (line 122)

```cuda
float expansion = (rest_len > 0 && curr_len > 0) ? sqrtf(rest_len / curr_len) : 1.0f;
```

If `curr_len` is very small (approaching zero while still > 0 for the check), `rest_len / curr_len` can produce a very large value. For `float`, `sqrtf(> FLT_MAX)` returns infinity. Guard both operands:

```cuda
float ratio = (rest_len > 0 && curr_len > 1e-8f) ? fminf(rest_len / curr_len, 1e8f) : 1.0f;
float expansion = sqrtf(ratio);
```
