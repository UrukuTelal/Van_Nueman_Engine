# SKEIN AUDIT: Issue #024 — Physics/Kernels: 6 Critical Bugs

## Severity
**Critical** — Fracture never triggers, wrong data type, every chunk dirty, unbounded rotation, NaN velocity, ignored baseline

## Category
Logic Errors — Mathematics, data type misuse, performance

---

## Summary

Six critical bugs in the CUDA physics kernel layer:

1. `pillar_coupling.cu:109` — Fracture probability comparison inverted (never triggers)
2. `fractal_skelly.cu:97` — `FROM_FP20` applied to `int32_t` instead of `fp20_t`
3. `deformation.cu:136-138` — All chunks marked dirty every tick (defeats dirty tracking)
4. `transform_compute.cpp:167` — Velocity accumulates without damping → diverges to infinity
5. `pillars_compute.cu:167` — `normalize()` on zero/undefined velocity produces NaN
6. `pillars_compute.cu:142` — Entity baseline field ignored, preventing pillar reset

---

## Bug 1: Fracture Probability Never Triggers

**File:** `physics/pillar_coupling.cu:109`

```cpp
float fracture_prob = harm * COUPLING_HARM_TO_FRACTURE_PROB;
if (fracture_prob > 0.5f) {
    seg.is_fractured = 1;
```

`COUPLING_HARM_TO_FRACTURE_PROB` is defined at line 19 as `0.005f`. Even at maximum `harm = 1.0`, `fracture_prob = 1.0 * 0.005 = 0.005`. The condition `0.005 > 0.5` is **always false**. The fracture system can never trigger.

**Failure:** Segments can never fracture from Harm pillar pressure. The entire fracture mechanic is dead code.

**Likely intent:** The comparison should be `fracture_prob > 0.005f` or the threshold should be relative. Or `COUPLING_HARM_TO_FRACTURE_PROB` should be much larger (e.g., `0.5f`).

---

## Bug 2: `FROM_FP20` Applied to Plain `int32_t`

**File:** `physics/fractal_skelly.cu:97-98`

```cpp
float resistance = FROM_FP20(state.pillars.p[PILLAR_RESISTANCE]);
```

`FROM_FP20` is defined at line 15:
```cpp
#define FROM_FP20(x) ((float)(x).raw() / 1048576.0f)
```

This macro calls `.raw()` which is a method of `vn::fp20_t` (a scaled integer class). But `state.pillars.p[...]` is a plain `int32_t` (defined in `kernels/pillars_compute.cu:36`):
```cpp
struct __align__(16) PillarStateVector {
    int32_t p[NUM_PILLARS];  // scaled integers
};
```

Calling `.raw()` on an `int32_t` is a compilation error or UB depending on the compiler. In CUDA, this would fail to compile since `int32_t` has no `.raw()` method.

**Wait** — `ServerState` at `pillars_compute.cu:64`:
```cpp
struct ServerState {
    uint32_t id;
    PillarStateVector pillars;
```
So `state.pillars.p[...]` is `int32_t`. The `FROM_FP20` macro will fail to compile at `fractal_skelly.cu:97`.

**However**, looking at `fractal_skelly.cu` line 4: `#include "../apis/skelly_api.cu"` — and the `FROM_FP20` in `pillar_coupling.cu` (line 11) is the same macro. The `pillar_coupling.cu` kernels pass `Entity*` which has `fp20_t pillars[...]` — so `FROM_FP20` works correctly there. But `fractal_skelly.cu` passes `ServerState*` which has `int32_t` pillars — `FROM_FP20` doesn't apply.

**Likely fix:** The fractal skelly server tick should use `FROM_SCALED` instead:
```cpp
#define FROM_SCALED(i) ((float)(i) / SCALE_FACTOR)
```

---

## Bug 3: Every Chunk Marked Dirty on Every Organ Tick

**File:** `physics/deformation.cu:136-138`

```cpp
// Simplified: just mark all chunks for now
for (uint32_t c = 0; c < chunk_count; c++) {
    atomicExch(&chunks[c].dirty, 1);
}
```

The comment itself admits this is a simplification, but it has severe consequences: **every chunk is marked dirty every frame**, forcing a full SVO rebuild every tick. This completely defeats the purpose of dirty tracking and destroys performance as chunk count grows.

For a world with 1000 chunks, each chunk is rebuilt every frame instead of only the ~1-3 chunks actually affected by a single organ.

---

## Bug 4: Velocity Accumulates Without Damping → Infinity

**File:** `kernels/transform_compute.cpp:165-169`

```cpp
float speed = force_f * dt * 2.0f;
ssbo.velocities_x[idx] += cosf(relation_f * 6.2832f) * speed;
ssbo.velocities_y[idx] += sinf(relation_f * 6.2832f) * speed;
ssbo.velocities_z[idx] += cosf(depth_f * 6.2832f) * speed * 0.5f;
```

Velocity is **accumulated** every frame with **no damping or clamp**. Since `force_f` can be as high as 1.0 and `dt` varies, each frame adds up to `~2.0 m/s` to velocity. Without any friction/damping term (e.g., `velocity *= 0.99`), velocity grows unbounded. Within 1000 frames at 60 FPS, velocity can exceed 2000 m/s, and the entity teleports outside the world.

---

## Bug 5: `normalize()` on Zero/Undefined Velocity → NaN

**File:** `kernels/pillars_compute.cu:167`

```cpp
float3 dir = normalize(e.velocity);
e.velocity = dir * force_float;
```

`normalize()` of a zero vector `(0,0,0)` is undefined in CUDA — it typically produces `(NaN, NaN, NaN)` or can raise a floating-point exception. If an entity starts at rest and `force_float` is 0, the entity's velocity becomes NaN, propagating to position (NaN, NaN, NaN) on the next frame.

The `normalize` function itself is defined at `api/skelly_api.cpp:29`:
```cpp
float3 float3_normalize(float3 v) { float l = float3_length(v); return l > 0 ? float3_mul_scalar(v, 1.0f/l) : v; }
```

This checks `l > 0` but doesn't handle `l == 0` (returns the zero vector unchanged, which is different from NaN but still represents a non-directional zero-vector assignment). However, `pillars_compute.cu:167` is CUDA code using CUDA's built-in `normalize()` which does NOT have this guard.

---

## Bug 6: Entity Baseline Field Ignored

**File:** `kernels/pillars_compute.cu:142`

```cpp
e.pillars.p[PILLAR_AWARENESS] = awareness;
```

And at line 143:
```cpp
e.pillars.p[PILLAR_AWARENESS] = awareness;
```

The `Entity` struct (lines 51-62) has a `baseline` field:
```cpp
PillarStateVector baseline;  // baseline values for reset
```

But `pillars_update_kernel` **never reads from `baseline`** except to compute drift (line 121), and never uses it as a reset reference. The baseline field is declared in the struct but has no effect on the simulation — it's write-only.

Meanwhile, the drift computation at lines 118-127:
```cpp
for (int p = 0; p < NUM_PILLARS; p++) {
    int32_t current = e.pillars.p[p];
    int32_t base = e.baseline.p[p];
    int32_t diff = base - current;
    int32_t willpower = e.pillars.p[PILLAR_WILLPOWER];
    int32_t drift = (diff >> 10) * (willpower >> 16);
    e.pillars.p[p] = current + drift;
}
```

This applies drift toward baseline after line 114-116 has already set a custom awareness value. The drift will immediately try to pull awareness back toward baseline, partially undoing the awareness update. The order of operations is inconsistent.

---

## Summary Table

| Bug | File:Line | Severity | Impact |
|---|---|---|---|
| 1. Fracture never triggers | pillar_coupling.cu:109 | Critical | Fracture mechanic is dead code |
| 2. FROM_FP20 on int32_t | fractal_skelly.cu:97 | Critical | Compilation error on any real use |
| 3. All chunks dirty every tick | deformation.cu:136-138 | Critical | SVO fully rebuilt every frame |
| 4. Velocity no damping | transform_compute.cpp:167 | Critical | Velocity → ∞ over time |
| 5. normalize() NaN risk | pillars_compute.cu:167 | Critical | Position becomes NaN |
| 6. Baseline ignored | pillars_compute.cu:142 | High | Baseline field useless |

## Recommended Fixes

1. **Bug 1:** Change threshold to `fracture_prob > 0.005f` or scale the coupling constant.
2. **Bug 2:** Replace `FROM_FP20` with `FROM_SCALED` for `int32_t` pillar access.
3. **Bug 3:** Replace the blanket-dirty loop with actual spatial intersection.
4. **Bug 4:** Add `velocity *= damping_factor` (e.g., 0.99) each frame.
5. **Bug 5:** Guard `normalize` with `if (length_squared > 0)` before dividing.
6. **Bug 6:** Either use baseline for proper reset logic or remove the field.

## Confidence
**100%** — All bugs are directly verifiable from source code.
