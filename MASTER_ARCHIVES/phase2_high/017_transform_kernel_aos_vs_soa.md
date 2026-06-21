# Issue #017: transform_compute vs pillars_compute kernel — AoS/SoA layout mismatch risk

**Severity:** High (silent data corruption if mixed)  
**Phase:** 2 (High)  
**Files:**
- `kernels/transform_compute.cpp` (SoA kernel — separate arrays per field)
- `kernels/pillars_compute.cpp` (AoS kernel — array of Entity structs)
- `simulation/transform_compute.cpp` (host Vulkan dispatch, SoA)

## Description

The codebase has two GPU compute kernels for pillar updates with **fundamentally incompatible memory layouts**:

### transform_compute.cpp (SoA — Structure of Arrays)

Entry point takes 24 separate array parameters — one per pillar + position/velocity components + active + resources:

```cpp
extern "C" void transform_compute_main(
    float* positions_x, float* positions_y, float* positions_z,
    float* velocities_x, float* velocities_y, float* velocities_z,
    fp20_t* pillar_awareness, fp20_t* pillar_willpower, ...  // 16 pillar arrays
    uint32_t* active, int32_t* resources, ...
);
```

### pillars_compute.cpp (AoS — Array of Structures)

Entry point takes a single `Entity*` array:

```cpp
extern "C" void pillars_compute_main(
    Entity* entities,     // Array of Entity structs with {id, type, float3 position, float3 velocity, PillarStateVector pillars, ...}
    uint32_t entity_count,
    ...
);
```

## Risk

If `pillars_compute_main` is ever dispatched with SoA-style buffer pointers (or vice versa), the layout mismatch would cause **complete data scrambling**:
- SoA stride = 4 bytes (single float per buffer per entity)
- AoS stride = sizeof(Entity) with interleaved fields

The pillar index *definitions* match between both kernels — but the **memory layout** is critically different.

## Current dispatch paths

The host-side code (`simulation/transform_compute.cpp`) only dispatches `transform_compute_main` via Vulkan SSBOs in SoA layout. `pillars_compute_main` appears to be a standalone kernel for a separate use case (vncc compilation, possibly the toolchain test). However, there is **no guard** preventing incorrect dispatch — both kernels define identical pillar indices, making it easy to confuse them during maintenance.

## Recommendations

1. Rename the entry points to make the layout explicit: `transform_compute_main_soa` vs `pillars_compute_main_aos`
2. Add a `static_assert` or comment in each kernel documenting the expected layout
3. If both are needed, provide a layout converter rather than relying on manual dispatch
