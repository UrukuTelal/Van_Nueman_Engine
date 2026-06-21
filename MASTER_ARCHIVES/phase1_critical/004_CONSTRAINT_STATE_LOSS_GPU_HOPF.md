# SKEIN AUDIT: Issue #004 — Constraint State Loss in GPU Hopf Path

## Severity
**High** — Simulation Correctness

## Category
Simulation — State Persistence, GPU/CPU Parity

---

## Summary

The `HopfPIDState` struct stores `relational_complexity`, `tick_accumulator`, and `in_rupture` as persistent per-entity state, but **does not store `constraint_level` or `depth_buffer`**. The GPU Hopf upload path hardcodes constraint_level to 0.0 on every dispatch. Furthermore, `hopf_transform()` (the function that actually uses constraint) is **never called from production code** — only from a test. The entire constraint accumulation → depth overflow system is effectively non-functional in the Hopf-PID simulation path.

---

## Evidence

### Finding 1: `hopf_transform()` is production dead code

**File:** `include/HopfPID.h:405-484`

`hopf_transform()` takes `depth_buffer` and `constraint_level` as mutable references and is the only Hopf path that feeds them through `compute_delta_theta`. Grepping for callers:

```
C:\Projects\Van_Nueman_Engine\tests\test_hopf_pid.h:58  → test only
```

**No production callers.** The main simulation path (`hopf_tick()` at line 683) does NOT call `hopf_transform()`. It only:
1. Accumulates sub-Planck time
2. Updates fibers (dead code — fiber_count is always 0)
3. Projects to membrane
4. Tracks relational complexity
5. Detects rupture
6. Applies soliton stabilization

All inter-entity TRANSFORM operations in the Hopf-PID path are **non-existent**. The `hopf_transform` function was implemented, tested, but never wired into the tick loop.

### Finding 2: `HopfPIDState` lacks constraint/depth fields

**File:** `include/HopfPID.h:234-274`

```cpp
struct HopfPIDState {
    PillarStateVector frames[HOPF_FRAME_COUNT];
    float frame_wht[HOPF_FRAME_COUNT][WHT_N];
    float membrane[HOPF_BASE_DIM];
    std::vector<PhaseLockedLoop> fibers;
    int fiber_count;
    float relational_complexity;
    float complexity_cap;
    double tick_accumulator;
    double bitrate_budget;
    bool in_rupture;
    float rupture_timer;
    // ⚠ NO constraint_level field
    // ⚠ NO depth_buffer field
};
```

Compare with `Entity` struct (`Entity.h:106-107`):
```cpp
vn::fp20_t constraint_level;  // 0.0 = unconstrained, 1.0 = fully constrained
vn::fp20_t depth_buffer;      // absorbs rotational overflow
```

### Finding 3: GPU upload hardcodes constraint = 0.0

**File:** `simulation/hopf_compute.cpp:300-324`

```cpp
std::vector<float> constraint(count);
for (uint32_t i = 0; i < count; i++) {
    constraint[i] = 0.0f;  // HARDCODED — never read from HopfPIDState
}
```

### Finding 4: GPU constraint buffer exists but is unused

**File:** `simulation/hopf_compute.h:58-59`
```cpp
VkBuffer constraintLevelBuffer_ = VK_NULL_HANDLE;
VkDeviceMemory constraintLevelMemory_ = VK_NULL_HANDLE;
```

The buffer is allocated (line 103-105 in hopf_compute.cpp) and bound in the descriptor set (line 145-147), but is **always written with 0s**.

### Finding 5: CPU constraint path also leaks

In `tick_loop.cpp:458-492`, `hopf_tick_all()` calls `hopf_tick()` per entity, which does NOT process constraint. Even if it did, constraint would be a local variable on the stack and lost between ticks.

### Finding 6: `depthBufferBuffer_` stores tick_accumulator, not Depth pillar

**File:** `simulation/hopf_compute.cpp:309`
```cpp
depth[i] = static_cast<float>(hopfStates[i].tick_accumulator);
```

The `depthBufferBuffer_` SSBO is written with `tick_accumulator` (the sub-Planck time counter), NOT the Depth pillar value. The Depth pillar is part of the 16-pillar state in `thoughtStateBuffer_`.

---

## Failure Scenario

1. **Constraint never accumulates:** Entities can never enter a constrained state through the Hopf-PID path. The constraint system, which is supposed to model entity degradation under sustained force, is effectively disabled.

2. **CPU vs GPU divergence:** Even if constraint accidentally worked on CPU (e.g., through Entity.h's `transform_bloch` called from non-Hopf paths), the GPU path would reset it to 0 on every tick, causing different results depending on which backend is active.

3. **False determinism:** Tests pass because the constraint value is always 0, but real simulation behavior is wrong.

---

## Root Cause

The Hopf-PID engine was designed as an extension to the existing Entity/TRANSFORM system, but the two code paths were never fully integrated. The `hopf_transform` function was written to handle inter-entity transforms with constraint, but was never connected to the tick loop. The `HopfPIDState` struct was designed independently of the `Entity` struct and simply doesn't have slots for constraint/depth. The GPU upload code was written as a proof-of-concept with placeholder values.

---

## Recommended Fix

### Step 1: Add constraint and depth to HopfPIDState

```cpp
struct HopfPIDState {
    // ... existing fields ...
    float constraint_level;     // [0,1], persisted per-entity across ticks
    float depth_buffer;         // accumulated rotational overflow buffer
};
```

### Step 2: Wire `hopf_transform` into the tick loop

In `hopf_tick()` or `hopf_tick_all()`, add inter-entity interaction:
```cpp
// After per-entity maintenance, apply environmental TRANSFORM
for (each pair of nearby entities) {
    hopf_transform(actor_state, subject_state, 
                   operator_pillar, target_pillar,
                   subject_state.depth_buffer,   // now persistent
                   subject_state.constraint_level, // now persistent
                   cfg);
}
```

### Step 3: Fix GPU upload to read actual values

```cpp
constraint[i] = hopfStates[i].constraint_level;  // not 0.0f
depth[i] = hopfStates[i].depth_buffer;            // not tick_accumulator
```

### Step 4: Update GPU download

In `HopfCompute::download()`, write back constraint_level and depth_buffer from the GPU:
```cpp
hopfStates[i].constraint_level = constraint[i];
hopfStates[i].depth_buffer = depth[i];
```

### Step 5: Add test

```cpp
// Verify that constraint accumulates over multiple hopf_transform calls
// and persists in HopfPIDState.constraint_level
TEST(ConstraintPersistence) {
    HopfPIDState state;
    state.init();
    float initial = state.constraint_level;  // should be 0.0
    // Apply transforms that exceed depth budget
    for (int i = 0; i < 100; i++) {
        hopf_transform(actor, state, FORCE, INTEGRITY, 
                       state.depth_buffer, state.constraint_level);
    }
    EXPECT_GT(state.constraint_level, 0.0f);
}
```

---

## Confidence

**95%** — The missing fields and hardcoded zeros are verifiable by static analysis. The dead code status of `hopf_transform` is confirmed by grep. 5% uncertainty: there may be an indirect path through Entity.h's `transform_bloch` → `Entity.constraint_level` that partially works outside the Hopf system, but the GPU Hopf path is definitively broken.
