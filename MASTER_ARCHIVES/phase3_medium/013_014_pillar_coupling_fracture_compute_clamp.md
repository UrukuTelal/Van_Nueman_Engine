# Issues #013-#014: pillar_coupling distribution overhead & pillars_compute.cu unclamped values

**Severity:** Medium  
**Phase:** 3 (Medium)  
**Files:**
- `physics/pillar_coupling.cpp:124-126`
- `kernels/pillars_compute.cu:110-143`

---

## Issue #013: pillar_coupling — Distribution recreated per loop iteration

Line 125:

```cpp
std::uniform_real_distribution<float> dist(0.0f, 1.0f);
```

This distribution object is recreated on **every iteration** of the loop (inside the `if (fracture_probability > 0.01f)` block, line 124-133). This is unnecessary overhead — the distribution should be created once outside the loop and reused.

The fracture probability constant `COUPLING_HARM_TO_FRACTURE_PROB = 0.02f` (line 26) produces max `fracture_probability = 1.0 * 1.0 * 0.02 = 0.02`, which correctly exceeds the `> 0.01` threshold, so the fracture can trigger. This appears to already have been adjusted from an earlier lower value.

**Impact:** Minor performance overhead. The distribution constructor may allocate state on each call.

## Issue #014: pillars_compute.cu — Unclamped pillar values after operations

The CUDA kernel operates on raw `int32_t` scaled integer values and performs arithmetic **without clamping** to the valid pillar range [0, SCALE_FACTOR]:

Line 114:
```cpp
awareness -= (distortion >> 10);  // can go negative
if (awareness < 0) awareness = 0; // awareness IS clamped
```

Line 126:
```cpp
e.pillars.p[p] = current + drift;  // NOT clamped! Can exceed [0, SCALE_FACTOR]
```

Line 142-143:
```cpp
awareness += flux_boost;           // NOT clamped!
e.pillars.p[PILLAR_AWARENESS] = awareness;
```

While line 115 clamps `awareness` after line 114, the other paths at lines 126, 142-143 have **no clamping**. Drift computations combined with `flux_boost` can push values above 1.0 (SCALE_FACTOR) or below 0.

**Impact:** Pillar values outside [0,1] violate the Bloch sphere representation invariant where 0→θ=π and 1→θ=0. Downstream consumers (Bloch rotation, TRAMSFIRM, Hopf-PID) that assume valid range will compute incorrect rotations or produce NaN from `acosf(2.0f * value - 1.0f)`.

### Fix

Add clamping after all pillar value modifications:

```cpp
// After line 126:
if (e.pillars.p[p] < 0) e.pillars.p[p] = 0;
if (e.pillars.p[p] > TO_SCALED(1.0f)) e.pillars.p[p] = TO_SCALED(1.0f);

// After line 143:
if (awareness > TO_SCALED(1.0f)) awareness = TO_SCALED(1.0f);
if (awareness < 0) awareness = 0;
```
