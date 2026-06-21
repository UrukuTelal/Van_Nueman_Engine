# SKEIN AUDIT: Issue #001 — Triple TRANSFORM Algorithm Duplication

## Severity
**Critical** — Architecture / Mathematics / Maintainability

## Category
Architecture — Code Duplication, Algorithm Divergence

---

## Summary

The TRANSFORM algorithm (the core mathematical operation of the Pillar cosmology — computing Bloch sphere angular shifts from pillar values) exists in **three distinct C++ implementations** and **two additional kernel copies**, each with different signatures, behaviors, and bug profiles. The duplication was acknowledged in a comment but has already accumulated divergent behavior.

---

## Evidence

### Copy 1: `Entity.h` — Buggy (hardcoded alignment)

**File:** `include/Entity.h:172-189`
**Function:** `compute_harm_angular_shift`

```cpp
inline vn::fp20_t compute_harm_angular_shift(
    vn::fp20_t incoming_force,
    vn::fp20_t operator_magnitude,
    vn::fp20_t willpower,
    vn::fp20_t depth
) {
    vn::fp20_t alignment_one(1.0f);
    vn::fp20_t effective_alignment = (alignment_one + vn::fp20_t(1.0f)) * vn::fp20_t(0.5f);  // BUG!
    // Always computes (1.0 + 1.0) * 0.5 = 1.0 — alignment is HARDCODED to 1.0
    vn::fp20_t operator_mag = incoming_force * operator_magnitude * effective_alignment;
    // ... rest is same as TransformCore.h
}
```

**Root cause:** `alignment_one` is a misnamed local constant (value 1.0), not a parameter. The function signature lacks an `alignment` parameter entirely. The `effective_alignment` is computed as `(1.0 + 1.0) * 0.5 = 1.0`, meaning **the effective alignment multiplier is always 1.0 regardless of actual actor-subject alignment**.

**Called from (at least these production paths):**
- `Entity.h:191` — `transform_bloch()` (wraps this buggy function)
  - Called by: `Entity.h:233` — `apply_harm_rotation()`
    - Called by: `physics/deformation.cpp:46` — deformation physics
    - Called by: `tests/test_pillar_coupling.h:30` — test code
  - Called by: `quantum/classical_fallback_backend.h:76` — quantum fallback
  - Called by: `tests/test_entity.h:103` — test code

**Comment at line 167-170 acknowledges the duplication:**
```
// NOTE: These inline helpers duplicate the canonical TRANSFORM algorithm
// defined in TransformCore.h (compute_angular_shift) and Transform.h (transform_operation).
// They remain here for callers that cannot include TransformCore.h due to dependency order.
// Keep in sync with TransformCore.h when changing the core formula.
```

The comment says "Keep in sync" but the function is ALREADY out of sync (missing alignment parameter).

### Copy 2: `TransformCore.h` — Canonical (correct)

**File:** `include/TransformCore.h:18-65`
**Function:** `compute_angular_shift`

```cpp
inline ShiftResult compute_angular_shift(
    vn::fp20_t operator_value,
    vn::fp20_t influence,
    vn::fp20_t alignment,         // <-- alignment IS a parameter (correct)
    vn::fp20_t willpower,
    vn::fp20_t depth_buffer
) {
    // ...
    vn::fp20_t effective_alignment = (alignment + vn::fp20_t(1.0f)) * one_half;  // correct
    // ...
}
```

This copy also returns a proper `ShiftResult` struct with `shift_rad`, `depth_consumed`, `constraint_delta`, and `excess_rad` — the Entity.h copy returns only a bare `fp20_t`.

**Called from:**
- `include/Transform.h:100` — `transform_operation()` (the full wrapper)
  - Called by celestial, chemistry, and user code
- `include/HopfPID.h:381` — `compute_delta_theta()` (Hopf-PID engine)

### Copy 3: `Transform.h` — Full Wrapper

**File:** `include/Transform.h:74-134`
**Function:** `transform_operation`

This is the highest-level wrapper. It correctly delegates to `compute_angular_shift` (Copy 2) and wraps the result with full metadata. It is **not** a separate algorithm implementation.

### Copy 4: `kernels/transform_compute.cpp` — SPIR-V Kernel

**File:** `kernels/transform_compute.cpp:80-95`
Contains inline `pillar_value_to_theta`, `theta_to_pillar_value`, and `apply_bloch_rotation` that operate on float. This is a **partial reimplementation** for GPU compute — it only implements Bloch rotation, not the full angular shift computation. The full ANGULAR SHIFT is expected to be computed by the GPU SPIR-V shader (`transform_compute.spv`). This is a separate code path and its correctness depends on the SPIR-V shader's implementation.

### Copy 5: `kernels/hopf_pid_compute.cpp` — SPIR-V Kernel

**File:** `kernels/hopf_pid_compute.cpp:96-115`
Same pattern as Copy 4, with trailing underscore function names (`pillar_value_to_theta_`, `theta_to_pillar_value_`). Operates on float.

### Copy 6: `api/pillars_api_simple.cpp` — Simplified API

**File:** `api/pillars_api_simple.cpp:98-113`
Contains `bloch_value_to_theta`, `bloch_theta_to_value`, `bloch_rotate` — implementing the same Bloch rotation but with **different behavior for large rotations** (clamping at π instead of wrapping/folding).

---

## Failure Scenario

**Scenario:** An engineer changes the TRANSFORM formula in `TransformCore.h` to fix a physics bug. All callers that use `transform_operation()` or `compute_angular_shift()` get the fix. But `physics/deformation.cpp` and `quantum/classical_fallback_backend.h` continue using the `Entity.h` copy (which is also buggy independently with the hardcoded alignment). The simulation diverges silently — deformation physics and quantum backends produce wrong results while the main simulation appears correct.

**More insidious:** Even before any change, the deformation physics ALWAYS computes with `effective_alignment = 1.0`, meaning it treats all actor-subject pairs as perfectly aligned, regardless of their actual pillar states. This is a qualitative physics error.

---

## Root Cause

Historical: The Entity.h copy existed first (or was ported from Python). When the proper `TransformCore.h` was created with the correct signature, the old copy was kept "for callers that cannot include TransformCore.h due to dependency order." This dependency concern is likely resolvable with forward declarations or refactoring, but was never addressed. The SPIR-V kernel copies are unavoidable (different compilation target) but represent an additional source-truth fragmentation problem.

---

## Recommended Fix

1. **Delete** `Entity.h:172-247` (the `compute_harm_angular_shift`, `transform_bloch`, `apply_harm_rotation`, `apply_distortion_torsion` functions).
2. **Add** a thin `compute_angular_shift` wrapper with the old Entity.h signature in a compatibility header OR fix all callers to use the canonical API.
3. **Update** `physics/deformation.cpp:51` to call `compute_angular_shift` from TransformCore.h with a proper alignment parameter.
4. **Update** `quantum/classical_fallback_backend.h:76` to use the canonical API.
5. **Update** `tests/test_entity.h` to test `compute_angular_shift` instead of the buggy Entity.h versions.
6. **Audit** `kernels/transform_compute.cpp` and `kernels/hopf_pid_compute.cpp` to ensure their Hilbert curve (SPIR-V shader) matches the CPU algorithms exactly. Add a cross-validation test that compares CPU TRANSFORM output to GPU TRANSFORM output for identical inputs.
7. **Add** a compile-time assertion or CI step that the SPIR-V kernel tests match CPU mathematical outputs within tolerance.

---

## Caller Dependency Map

```
TRANSFORM Algorithm
├── TransformCore.h::compute_angular_shift (correct, canonical)
│   ├── Transform.h::transform_operation (correct, full wrapper)
│   │   ├── celestial/StellarSystem.h
│   │   ├── chemistry/ReactionSystem.h
│   │   └── user code
│   └── HopfPID.h::compute_delta_theta (correct)
│       └── HopfPID.h::hopf_transform
│
└── Entity.h::compute_harm_angular_shift (BUGGY — hardcoded alignment)
    ├── Entity.h::transform_bloch (BUGGY — propagates hardcoded alignment)
    │   ├── Entity.h::apply_harm_rotation (BUGGY)
    │   │   ├── physics/deformation.cpp **PRODUCTION BUG**
    │   │   └── tests/test_pillar_coupling.h
    │   └── quantum/classical_fallback_backend.h **PRODUCTION BUG**
    └── physics/deformation.cpp (direct call)
```

## Confidence

**95%** — Verified by static analysis. The hardcoded alignment bug is trivially verifiable. The full caller map is confirmed by grep. The 5% uncertainty is whether some dependent code paths are dead code (never actually reached at runtime), which would reduce practical impact but not architectural concern.
