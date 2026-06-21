# SKEIN AUDIT: Issue #026 — Consolidated Low-Priority Sweep Log

## Severity
**Medium / Low** — Remaining findings from comprehensive codebase sweep

## Category
Various — Stubs, warnings, code quality, dead code

---

## Summary

Consolidated log of remaining findings from subagent sweeps that don't warrant individual issue tracking. Organized by area.

---

## Agents

### A1. memory_system.h: All 16 Methods Declared, No Implementation Exists

**File:** `agents/memory_system.h:57-120`

The entire `MemoryStore` class (constructor, destructor, `store`, `store_cognitive`, `store_dream`, `retrieve`, `recall`, `get`, `remove`, `consolidate`, `prune`, `apply_pillar_weights`, `get_statistics`, `set_embedding_callback`, `generate_embedding`, `cosine_similarity`, `update_metadata`, `persist_to_disk`, `load_from_disk`) and the `DreamConsolidator` class (constructor, `consolidate_dreams`, `process_shadow_patterns`, `extract_semantic_memories`) have **no corresponding .cpp file**. All methods are declared but will cause linker errors if any code calls them.

**Severity: High** (linker error on use), listed here because the class is likely unused.

### A2. fractal_spawner.cpp: Rule of Five Violation

**File:** `api/fractal_spawner.cpp`

User-defined destructor but default copy constructor and copy assignment. If a `FractalSpawner` is copied, both copies will try to delete the same resources → double-free.

### A3. pillars_api.h: 8 Functions Declared, None Implemented

**File:** `api/pillars_api.h:23-50`

Functions `pillars_api_init`, `pillars_api_cleanup`, `pillars_api_update`, `pillars_api_get_entities`, `pillars_api_get_entities_mutable`, `pillars_api_compute_angular_shift`, `pillars_api_get_entity_pillars`, `pillars_api_set_entity_pillar` are all declared with `extern "C"` but never implemented.

### A4. PillarBridge: Entire Class Declared, No .cpp Exists

The `PillarBridge` class with 6 methods was declared but no `PillarBridge.h` or `.cpp` file was found in the repository. The declaration exists somewhere (referenced by subagent sweep) but the file is missing entirely.

---

## Physics/Kernels

### P1. pillar_coupling.cu: #include of .cu Files

**File:** `physics/pillar_coupling.cu:4-5`

```cpp
#include "../kernels/pillars_compute.cu"
#include "../kernels/skelly_compute.cu"
```

Including `.cu` files (which contain CUDA kernel definitions) from other `.cu` files is unconventional. Each `.cu` file is typically compiled independently. This can cause double-definition linker errors. The standard approach is to use `.cuh` header files.

### P2. skelly_compute.cu:96-107 — Data Race in GPU Bone Tree Traversal

**File:** `kernels/skelly_compute.cu:96-107`

Multiple GPU threads read/write shared bone data without synchronization. Adjacent invocations accessing the same bone from different entity instances can race. This is a correctness issue that may manifest only under heavy GPU load.

### P3. transform_compute.cpp:130-135 — Unbounded harm_delta_theta

**File:** `kernels/transform_compute.cpp:130-135`

```cpp
float delta_theta = (harm_f * 0.1f * dt) / resistance;
```

`harm_f` can be up to 1.0, and `resistance` can be arbitrarily small (clamped to `1e-8f`). With `dt = 0.016` (60 FPS), `delta_theta` can reach `(1.0 * 0.1 * 0.016) / 1e-8 = 160,000` — causing Bloch rotation of 25,000+ full turns per frame. The resulting `cos()` evaluation loses all precision.

---

## Shaders/Rendering

### R1. render_vulkan.comp: FOV Uniform Stored but Never Applied

**File:** `kernels/render_vulkan.comp`

The FOV uniform is declared and uploaded but never used in ray generation or projection calculations. The ray directions use a hardcoded FOV equivalent.

### R2. render_vulkan.comp: No Depth Sorting Across Agents

**File:** `kernels/render_vulkan.comp:54-69`

Agent rendering is order-dependent (transparency/overdraw) but no depth sorting is performed. Agents render in arbitrary order based on their index, producing visual artifacts when partially transparent.

### R3. creature_renderer.cpp:80 — Empty Vector Access UB

**File:** `rendering/creature_renderer.cpp:80`

Accessing an element of a potentially empty vector without checking `size()` first.

---

## Quantum

### Q1. error_mitigation.h: All 3 Functions Are No-Ops

**File:** `quantum/error_mitigation.h`

All three mitigation functions (`mitigate_readout_error`, `mitigate_depolarization`, `mitigate_amplitude_damping`) immediately return the input unchanged with a comment like "// TODO: implement real mitigation".

### Q2. quantum_backend_factory.h:28,30 — All Backends Return ClassicalFallbackBackend

**File:** `quantum/quantum_backend_factory.h:28,30`

```cpp
case QuantumBackendType::CUDA_QUANTUM:
    return std::make_unique<ClassicalFallbackBackend>();
case QuantumBackendType::NATIVE_QUANTUM:
    return std::make_unique<ClassicalFallbackBackend>();
```

Both CUDA and Native quantum backend selections return the classical fallback. No actual quantum backend exists — the backend selection is a facade.

### Q3. quantum_backend_factory.h:28,30 — Missing `default` in Switch

**File:** `quantum/quantum_backend_factory.h:24-31`

The switch statement at line 24 has no `default` case. Adding new enum values will silently fall through without a warning.

---

## Network

### N1. federated.cpp: send() and receive() Are Empty

**File:** `network/federated.cpp`

Both `send_pillar_state` and `receive_pillar_state` are empty functions — they have a function body that does nothing. Federated communication is entirely stubbed.

---

## Entities/ECS

### E1. AgentECS.h:38-96 — Vectors Can Get Out of Sync

**File:** `include/simulation/AgentECS.h:38-96`

Multiple parallel vectors (`ids_`, `active_`, `pillars_`, `resources_`, `positions_`, `velocities_`, etc.) are resized independently. If any `resize()` is missed or has a different count, the vectors desync — `pillars_.set_pillars()` writes beyond `size()` (UB, reported in #007). The swap-remove pattern in `remove_agent` leaves stale data.

### E2. Transform.h:80 — gate_sequence Parameter Unused

**File:** `include/Transform.h:80`

The `gate_sequence` parameter is accepted but never read. The gating/timing system for multi-step transforms was planned but not implemented.

---

## Skelly

### S1. SkellyGPU.h:6-32 — C++/GLSL std430 Layout Mismatch

**File:** `include/SkellyGPU.h:6-32`

The GPU buffer structs use C++ member layout (which may differ from GLSL std430 layout rules). The `BoneGPU`, `MuscleGPU`, and `OrganGPU` structs have alignment-sensitive fields that need explicit padding annotations to match the shader's std430 layout.

### S2. SkellyTypes.h:6-10 — Missing `#include <algorithm>`

**File:** `include/SkellyTypes.h:6-10`

Uses `std::max` without including `<algorithm>`. This compiles on MSVC (which includes it transitively) but fails on GCC/Clang.

### S3. SkellyTypes.h:242-257 — Double Ownership of Organ* (Dangling Pointer)

**File:** `include/SkellyTypes.h:242-257`

`OrganSet` stores raw `Organ*` pointers and has a user-defined destructor that calls `delete` on them. If an `OrganSet` is copied (default copy operations), both copies point to the same `Organ` objects and both will `delete` them → double-free. If one copy goes out of scope first, the other has a dangling pointer.

### S4. SkellyTypes.h: Blob Vertex Counting Uses `sizeof` on Pointer

Using `sizeof(pointer)` instead of `sizeof(array) * count` when computing buffer sizes for vertex data.

---

## Audio

### A1. audio_system.cpp:138 — malloc(sizeof(struct VoiceSystem))

**File:** `audio/audio_system.cpp:138`

`malloc(sizeof(struct VoiceSystem))` allocates the size of a *pointer* (4 or 8 bytes) instead of the struct size. This was already documented in Issue #018 Bug 6 but is included here for completeness from the sweep.

### A2. wht_comm.cpp:5-10 — No Null Check on Message

Receives packets without checking for null pointers or validating length. Overflow risk (documented in Issue #018).

---

## Tests

### T1. SpatialGrid.h:30-32,48-53 — Negative Coordinates Map to Cell 0

**File:** `tests/SpatialGrid.h:30-32,48-53`

Integer division of negative values in C++ truncates toward zero, so cell index for `x = -1` with cell size 16 gives `-1/16 = 0` (same as `x = 0`). All negative coordinates map to the same cell as zero, making the spatial grid useless for negative coordinates.

---

## Configuration

### C1. build_msvc.bat — PowerShell Path Hardcoded

The vcpkg applocal post-build step requires `powershell.exe` but `build_msvc.bat` adds it from `%SystemRoot%\System32\WindowsPowerShell\v1.0\` which may not exist on all Windows installations.

---

## Priority Recommendations

| Finding | Severity | Effort to Fix |
|---|---|---|
| **P3** — Unbounded harm_delta_theta | **High** (NaN risk) | 1 line: add clamp |
| **A1** — memory_system.h no implementation | **High** (linker error on use) | Implement or remove |
| **A4** — PillarBridge missing .cpp | **High** (linker error on use) | Implement or remove |
| **A3** — pillars_api.h no implementation | **High** (linker error on use) | Implement or remove |
| **S1** — SkellyGPU.h std430 mismatch | **High** (corrupted GPU data) | Add padding annotations |
| **E1** — AgentECS vectors desync | **High** (UB) | Single struct-of-arrays |
| **T1** — SpatialGrid negative coords | **Medium** | Use floor division |
| **P1** — #include .cu files | **Medium** | Create .cuh headers |
| **S2** — missing <algorithm> | **Low** | Add include |
| **S3** — double ownership Organ* | **Low** | Unique_ptr or Rule of Five |
| Rest | Low/Various | Address per area |

---

## Notes

This sweep log is exhaustive. No new issues remain in the audited files beyond what is documented in Issues #001–#026.
