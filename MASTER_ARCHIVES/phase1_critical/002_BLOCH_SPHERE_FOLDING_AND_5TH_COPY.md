# SKEIN AUDIT: Issue #002 — Bloch Sphere Folding & 5th Copy Divergence

## Severity
**Critical** — Mathematics / Numerical Stability

## Category
Mathematics — Bloch Sphere Representation, Algorithm Divergence

---

## Summary

The Bloch sphere rotation in `Entity.h` applies a "fold" at π (mirroring values back to [0, π]) rather than properly wrapping through 2π. While mathematically correct for pure-state Z-coordinate representation, this masks a deeper representation problem: the pillar system tracks only the **Z-coordinate** of the Bloch sphere (a single scalar in [0,1]), losing all azimuthal (φ) information. Combined with the 5th diverged copy in `pillars_api_simple.cpp` that uses *clamping* instead of folding, there are now two different behaviors for large rotations across the codebase.

---

## Evidence

### Primary Bloch Rotation: `Entity.h:158-165`

```cpp
inline vn::fp20_t apply_bloch_rotation(vn::fp20_t current_value, vn::fp20_t delta_theta) {
    vn::fp20_t theta = pillar_value_to_theta(current_value);
    double t = theta.to_double() + delta_theta.to_double();
    t = std::fmod(t, 2.0 * 3.141592653589793);   // wrap to [0, 2π)
    if (t < 0.0) t += 2.0 * 3.141592653589793;    // handle negative
    if (t > 3.141592653589793) t = 2.0 * 3.141592653589793 - t;  // FOLD at π
    return theta_to_pillar_value(vn::fp20_t(t));
}
```

### Diverged (5th) Copy: `api/pillars_api_simple.cpp:108-113`

```cpp
inline float bloch_rotate(float current_value, float delta_theta) {
    float theta = bloch_value_to_theta(current_value);
    float new_theta = theta + delta_theta;
    // NO WRAPPING, NO FOLDING
    return bloch_theta_to_value(new_theta);  // bloch_theta_to_value clamps at π
}
```

Where `bloch_theta_to_value`:
```cpp
inline float bloch_theta_to_value(float theta) {
    float t = (theta < 0.0f) ? 0.0f : (theta > 3.14159265f) ? 3.14159265f : theta;
    return (std::cos(t) + 1.0f) * 0.5f;
}
```

### Theta-to-Value Mapping

```
pillar_value_to_theta:  θ = acos(2v - 1)        v ∈ [0,1], θ ∈ [0, π]
theta_to_pillar_value:  v = (cos(θ) + 1) / 2     θ ∈ [0, π], v ∈ [1,0]
```

The mapping is monotonic decreasing: θ=0 → v=1, θ=π/2 → v=0.5, θ=π → v=0.

---

## Mathematical Analysis

### Bloch Sphere Representation

On the Bloch sphere, a pure state is represented by:
```
|ψ⟩ = cos(θ/2)|0⟩ + e^(iφ) sin(θ/2)|1⟩
Bloch vector = (sin θ cos φ, sin θ sin φ, cos θ)
```

The pillar `value` maps to the **Z-coordinate** as:
```
value = (cos θ + 1) / 2  =  (Z + 1) / 2
```

This means:
- value = 1 → Z = 1 → north pole (|0⟩)
- value = 0 → Z = -1 → south pole (|1⟩)
- value = 0.5 → Z = 0 → equator states

### The Fold at π: Is it Correct?

When θ exceeds π and we fold: `θ' = 2π - θ`, then:
```
cos(θ') = cos(2π - θ) = cos(θ)
```

So `cos(θ') = cos(θ)`, which means the pillar value is preserved. The folding is **mathematically correct for the Z-coordinate**.

**However**, the problem is that a **rotational trajectory** is not preserved. Consider:
- Start at θ = 0.2π (near north pole, value ≈ 0.90)
- Apply δθ = 1.6π (a large rotation)
- t = 0.2π + 1.6π = 1.8π
- fmod(1.8π, 2π) = 1.8π
- Fold: 2π - 1.8π = 0.2π
- Result: same as starting point

But if we applied δθ = 0.6π incrementally over 3 steps:
- Step 1: 0.2π → 0.8π (on the way, folded to 0.8π)
- Step 2: 0.8π + 0.6π = 1.4π → fold to 0.6π
- Step 3: 0.6π + 0.6π = 1.2π → fold to 0.8π

The result after 3 steps of 0.6π is 0.8π, NOT 0.2π. The system is **not rotationally invariant** — the same total rotation applied in different accumulations produces different results.

This is a **qualitative flaw** for the simulation. When multiple entities apply small incremental rotations, the path-dependence means the same total force applied over different time distributions produces different results.

### 5th Copy Divergence

The `pillars_api_simple.cpp` copy does not wrap or fold at all — it clamps θ to [0, π]. This means:
- A rotation of δθ = 2.0 applied to θ = 1.0 gives new_theta = 3.0, which is clamped to π
- The same rotation applied as two steps of 1.0 each gives π after the first step already

This version is **even more divergent** and will produce qualitatively different results from the Entity.h version for large rotations.

### Precision Loss from Round-Trip

Every `value → θ → value` conversion goes through:
1. fp20_t → double (quantization: ~0.0001% error)
2. acos() (floating-point, non-deterministic across platforms/compilers)
3. cos() (floating-point, non-deterministic across platforms/compilers)
4. double → fp20_t (re-quantization)

This round-trip loss breaks the **determinism guarantee** that fp20_t was designed to provide. Two clients on different hardware may compute different values for the same inputs.

### Where the Theta Angle is Used

| Location | Tracking Method | Issue |
|----------|----------------|-------|
| `Entity.h` | value → θ → value round-trip | Precision loss per op |
| `HopfPID.h` | float θ computed from value each time | Same, via float |
| `kernels/transform_compute.cpp` | float θ from float value | Different float path |
| `api/pillars_api_simple.cpp` | float θ, no wrap at all | Different behavior |
| `agents/cognition.cpp` | θ from value, modifies θ, converts back | Round-trip per decision |

---

## Failure Scenario

1. Two entities apply opposing forces to a third. Due to folding path-dependence, the order of force application changes the final state. This produces **non-deterministic simulation results** despite fixed-point arithmetic.

2. The `pillars_api_simple.cpp` API produces different results from `Entity.h` for the same inputs when `|δθ| > π`, breaking cross-module consistency.

3. fp20_t → double → acos → double → fp20_t round-trip accumulates error at ~0.0001% per operation × thousands of operations per tick × millions of ticks = measurable drift.

---

## Root Cause

The pillar representation stores only the **Z-coordinate** of the Bloch sphere, but the TRANSFORM algorithm needs to apply rotations. The conversion back-and-forth between value-space and angle-space was chosen to keep the API simple (a single scalar per pillar) but sacrifices rotational determinism and path-independence. The 5th copy in `pillars_api_simple.cpp` diverged because it was written independently without cross-referencing the Entity.h implementation.

---

## Recommended Fix

### Short-term (Correctness):
1. **Unify all copies** to use the same wrapping/folding logic. The Entity.h version (fmod + fold) is the best existing behavior.
2. **Fix `pillars_api_simple.cpp`** to match Entity.h's wrapping behavior precisely.
3. **Document** that the pillar value is the Z-coordinate defined as `v = (cos θ + 1) / 2`.

### Medium-term (Determinism):
4. **Store θ directly** in the simulation (as fp20_t in [0, π]) instead of converting from value. This eliminates the acos/cos round-trip. The pillar `value` becomes a derived property: `v = theta_to_pillar_value(θ)`.
5. **Add a `theta` field** to `PillarStateVector` or maintain a parallel theta array in `PillarArrays`.

### Long-term (Full Bloch representation):
6. **Track both θ and φ** as fp20_t to represent the full Bloch sphere state. This enables proper quantum-like interference effects and is necessary for the CUDA-Quantum integration roadmap (where full amplitude information is needed).
7. The pillar `value` becomes `v = (cos θ + 1) / 2` (the Z-coordinate), computed from (θ, φ) when needed.

### Verification:
8. **Add a deterministic test** that applies the same total δθ in different step sizes and asserts the same final state.
9. **Add round-trip precision tests** that measure maximum error over 10,000+ operations.

---

## Confidence

**90%** — The path-dependence (non-rotational-invariance) is mathematically provable. The 5th copy divergence is confirmed by direct code comparison. The 10% uncertainty is whether the practical impact in the current simulation (where δθ per tick is small, typically < 0.01) is large enough to produce visible artifacts.
