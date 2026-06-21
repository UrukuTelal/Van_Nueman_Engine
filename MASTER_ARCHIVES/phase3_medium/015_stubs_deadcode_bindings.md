# Issue #015: Stubs, dead code, and binding mismatches (consolidated)

**Severity:** Low–Medium  
**Phase:** 3 (Medium)  
**Files:** Multiple

---

## 1. deformation.cpp — All deformation functions are stubs

`physics/deformation.cpp` has 3 stub functions and 1 semi-functional one:

| Function | Lines | Status |
|----------|-------|--------|
| `deformation_init()` | 7-10 | `(void)max_entities;` — empty |
| `apply_deformation()` | 12-31 | Checks harm > 0.7, does nothing: `(void)dt;` |
| `fractal_deform()` | 33-39 | Computes scale, does nothing: `(void)scale;` |
| `calculate_stress()` | 42-56 | Functional — computes harm angular shift |

The `apply_deformation` function is called from `tick_entity_voxel_coupling` (meso_tick line 373) but performs no actual deformation.

## 2. HopfPID.h — Production dead code

`include/HopfPID.h:405` — `hopf_transform()` is a GPU-style transform wrapper that is:
- Never called from `tick_loop.cpp` (which uses `hopf_tick` + `hopf_tick_all`)
- Only called from `tests/test_hopf_pid.h:58`
- The `fibers` array (256 entries, `fiber_count = 0`) is dead code — the fiber loop in `hopf_couple_fibers` is never entered

## 3. Descriptor set binding mismatches

Between `simulation/hopf_compute.cpp` (9 bindings) and `kernels/hopf_pid_compute.cpp` (the kernel), the binding order must match. Comparing:

| Index | hopf_compute.cpp (host) | hopf_pid_compute.cpp (kernel) |
|-------|------------------------|------------------------------|
| 0 | thoughtState | thought_state (int64_t*) |
| 1 | whtCoeffs | wht_coeffs (float*) |
| 2 | membrane | membrane (float*) |
| 3 | fiberCoherence | fiber_coherence (float*) |
| 4 | relComplexity | rel_complexity (float*) |
| 5 | depthBuffer | depth_buffer (float*) |
| 6 | constraintLevel | constraint_level (float*) |
| 7 | active | active (uint32_t*) |
| 8 | inRupture | in_rupture (uint32_t*) |

These need independent verification that the kernel parameter order matches exactly — any mismatch silently corrupts data.

## 4. render_vulkan.comp — FOV uniform never applied

`kernels/render_vulkan.comp:47` declares a FOV uniform that is never used in the pipeline:

```glsl
layout(push_constant) uniform PushConstants {
    float fov;       // declared but never applied to projection
    // ...
};
```

The projection matrix in the shader uses hardcoded values instead of incorporating `fov`.

## 5. No depth sorting in render pass

`kernels/render_vulkan.comp:54-69` renders agents in arbitrary order without depth sorting, causing order-dependent transparency/overdraw artifacts for overlapping agents.

## Impact

| Item | Impact |
|------|--------|
| deformation stubs | No visual/physical deformation in simulation |
| Hopf dead code | ~7 KB of unreachable code, maintenance burden |
| Binding mismatch risk | Potential silent GPU data corruption |
| FOV uniform unused | Uniform parameter misleading, no actual FOV control |
| No depth sorting | Visual artifacts with overlapping agents |
