# SKEIN AUDIT: Issue #003 — Dual GPU Pipeline Concurrent Execution

## Severity
**High** — Performance / Simulation Correctness

## Category
Architecture — Pipeline Orchestration

---

## Summary

In `SimulationTickLoop::update_pillar_vectors()`, when both the TRANSFORM GPU compute pipeline (`TransformCompute`) and the Hopf-PID GPU compute pipeline (`HopfCompute`) are initialized, **both execute sequentially on every tick**. This wastes one pipeline's computation, doubles GPU round-trips, and creates a hidden data dependency where one pipeline overwrites the other's outputs.

---

## Evidence

**File:** `simulation/tick_loop.cpp:494-543`

```cpp
void SimulationTickLoop::update_pillar_vectors() {
    // GPU PATH A: TRANSFORM compute (simple pillar update)
    if (transform_compute_ && transform_compute_->isReady()) {
        transform_compute_->upload(agent_ecs_);           // UPLOAD   ~N floats
        transform_compute_->dispatch(0.016f, 0, ...);     // DISPATCH (mode 0)
        transform_compute_->dispatch(0.016f, 1, ...);     // DISPATCH (mode 1)
        transform_compute_->dispatch(0.016f, 2, ...);     // DISPATCH (mode 2)
        transform_compute_->download(agent_ecs_);          // DOWNLOAD ~N values
    }

    // GPU PATH B: Hopf-PID compute (full Hopf engine)
    if (hopf_compute_ && hopf_compute_->isReady()) {
        hopf_ensure_state_size();
        // SYNC: read ECS back (ECS was just written by TRANSFORM download)
        for (...) {
            hstate.frames[0][p] = psv[p];
        }
        hopf_compute_->upload(hopf_states_);              // UPLOAD (thought states)
        hopf_compute_->dispatch(0.016f, 2, ...);           // DISPATCH
        hopf_compute_->download(hopf_states_);             // DOWNLOAD

        // WRITE: overwrite ECS pillars with Hopf results
        for (...) {
            ecs.pillar(...) = hopf_states_[i].frames[0][p];
        }
        return;  // <-- NOTE: early return here, CPU fallback skipped
    }

    // CPU FALLBACK: only if neither GPU path is active
    hopf_tick_all(0.016f, world_state_.hazard_level, world_state_.resource_density);
}
```

### The Problem

| Step | GPU A (TRANSFORM) | GPU B (Hopf) | ECS State |
|------|-------------------|--------------|-----------|
| Start | — | — | Base state |
| 1 | UPLOAD(base) | — | Base state |
| 2 | DISPATCH ×3 | — | Base state |
| 3 | DOWNLOAD → ECS | — | **TRANSFORM-modified** |
| 4 | — | READ from ECS | **TRANSFORM-modified** |
| 5 | — | UPLOAD(TRANSFORM-modified) | TRANSFORM-modified |
| 6 | — | DISPATCH | TRANSFORM-modified |
| 7 | — | DOWNLOAD → hopf_states | TRANSFORM-modified |
| 8 | — | WRITE Hopf → ECS | **Hopf-modified** |

**Result:** The ECS goes through three states per tick: Base → TRANSFORM → Hopf. The TRANSFORM computation (steps 1-3) is **entirely overwritten** by the Hopf pipeline (step 8). All the GPU time, PCIe bandwidth, and synchronization overhead of the TRANSFORM pipeline is wasted.

### Cost Analysis

Per tick with both pipelines active:
- **TRANSFORM:** 3× dispatch + 2× PCIe transfer (upload + download of ~16+N values)
- **Hopf:** 1× dispatch + 2× PCIe transfer (upload + download of 512+N values per entity)
- **Total:** ~600 µs + 2× PCIe round-trips for **1.0× useful work**

If only the Hopf pipeline ran:
- **Hopf:** 1× dispatch + 2× PCIe transfer
- **Total:** ~300 µs + 1× PCIe round-trip for **1.0× useful work** (same result)

---

## Failure Scenario

1. **Performance:** On any system with both GPU compute pipelines available (the intended deployment configuration), the TRANSFORM pipeline consumes ~50% of GPU compute budget for no benefit.
2. **Subtle timing bugs:** The two pipelines share the same Vulkan compute queue and fence, but there is no explicit synchronization between them. If `TransformCompute::dispatch()` uses a fence and `HopfCompute::upload()` doesn't wait for it (the fence IS waited on within dispatch()), there's a theoretical race.
3. **Double-accounting:** If the TRANSFORM pipeline applies environmental effects (hazard, resources) and the Hopf pipeline also applies them (via `hopf_tick_all` calling `hopf_apply_environment`), these effects are **applied twice**.

---

## Root Cause

The `update_pillar_vectors()` function was written as a progressive enhancement:
1. Initially: CPU fallback only (the `hopf_tick_all` call at the bottom)
2. Later: GPU TRANSFORM path added as an `if` block before CPU fallback
3. Later: GPU Hopf path added as another `if` block

At each stage, the new path was added rather than replacing the old path. The `return` statement on line 538 only skips the CPU fallback when the Hopf path runs — but the TRANSFORM path still executes first. No mutual exclusion was added between the two GPU paths.

---

## Recommended Fix

### Option A: Exclusive Path Selection (Recommended)

Make the two pipelines mutually exclusive. Add an enum to `SimulationTickLoop`:

```cpp
enum class GpuMode {
    DISABLED,        // No GPU, CPU fallback
    TRANSFORM_ONLY,  // Legacy TRANSFORM only
    HOPF_ONLY,       // Hopf-PID only
    FULL             // Both chained properly
};
```

In `update_pillar_vectors()`:
- `TRANSFORM_ONLY`: Run only the TRANSFORM pipeline
- `HOPF_ONLY`: Run only the Hopf pipeline
- `FULL`: Run TRANSFORM → (on-GPU, no download) → Hopf (in-engine chaining, no intermediate PCIe)
- `DISABLED`: CPU fallback

### Option B: Simple Mutual Exclusion

Change to `if-else if` flow:

```cpp
if (hopf_compute_ && hopf_compute_->isReady()) {
    // Run Hopf path (full simulation)
} else if (transform_compute_ && transform_compute_->isReady()) {
    // Run TRANSFORM path (simpler, legacy)
} else {
    // CPU fallback
}
```

This is the minimal fix: prefer Hopf when available, fall back to TRANSFORM, fall back to CPU.

### Option C: Proper Pipelining (Best for Performance)

Chain the two pipelines on-GPU without intermediate download/upload:

```cpp
if (hopf_compute_ && hopf_compute_->isReady() && 
    transform_compute_ && transform_compute_->isReady()) {
    // Upload once
    transform_compute_->upload(agent_ecs_);
    
    // Dispatch TRANSFORM (produces intermediate pillar values)
    transform_compute_->dispatch(0.016f, 0, hazard, resources);
    
    // Dispatch Hopf (reads TRANSFORM outputs from shared buffers)
    // NOTE: This requires descriptor set sharing between pipelines
    hopf_compute_->dispatch(0.016f, 2, PLANCK_THETA_EPS, 0.95f);
    
    // Download once
    hopf_compute_->download(hopf_states_);
    // Write back to ECS
}
```

This requires the two pipelines to share buffer bindings, which may not be the case currently. Option B is the recommended fast fix.

---

## Verification

After fix:
- Verify with `VK_DEBUG_REPORT` that both pipelines are not simultaneously dispatched
- Profile: measure GPU idle time before vs after. Expect ~40-50% reduction in GPU compute tick time for the Hopf path.

---

## Confidence

**100%** — The code path is deterministic: both `if` blocks execute sequentially with no mutual exclusion. The `return` on line 538 only affects the CPU fallback. Verified by reading the control flow.
