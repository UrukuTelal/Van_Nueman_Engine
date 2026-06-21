# Pending Work

Auto-generated from TODOs/FIXMEs across the codebase.  
Last updated: 2026-06-15 (Phase 10 — build restoration + native quantum backend)

## Legend
- **L** — Linker hazard (will not compile if used)
- **A** — Architecture decision needed
- **S** — Structural/cosmetic cleanup
- **D** — Deprecation tracking

## Items

### Q4: `quantum/native_quantum_backend.h` — Created with CUDA kernel stubs + CPU fallback
| | |
|---|---|
| File | `quantum/native_quantum_backend.h` |
| Severity | Low |
| Status | **Fixed** — Created in Phase 10. Implements `QuantumBackend` interface with CUDA kernels (`native_wht_kernel`, `native_hopf_kernel`, `native_spatial_search_kernel`, `native_bloch_evolve_kernel`) gated behind `#ifdef __CUDACC__` with CPU fallback (`fwht_cpu`, `hopf_cpu`). Guarded by `VN_USE_NATIVE_CUDA` in factory. Uses `cudaGetDeviceCount` for availability check. |

### FLL: Pre-existing segfault in `test_fll.cpp` (FLL Engine Tests, Phase 4 graph traversal)
| | |
|---|---|
| File | `tests/test_fll.cpp:628-637` |
| Severity | Low |
| Description | Intermittent segfault during `build_test_tree` graph traversal tests after ~66 passing assertions. Not related to recent changes — pre-existing memory/stack issue in `ArenaAllocator::get_node_by_uid` or `traverse` recursion depth. |
| Resolution | Diagnose stack overflow or dangling UID in `build_test_tree`/`traverse`. |

### Q3: Quantum roadmap in MASTER_LOG.md — interleaved with classical engine work
| | |
|---|---|
| Severity | Low |
| Description | MASTER_LOG.md quantum phases (Phases 1-4) describe planned steps interleaved with classical engine work. The quantum code is more complete than the roadmap suggests. |
| Resolution | Consider refactoring MASTER_LOG.md to separate quantum and classical roadmaps. |

## Removed / Resolved

| ID | Description | Resolution |
|---|---|---|
| L1 | memory_system.h linker hazard | **Removed** — dead code, no consumers. Inline utilities (FrequencyFilter, memory_id_from_content) kept. |
| D1 | Entity.h deprecated energy/health | **Fixed** — removed deprecated comment, collapsed to single `constraint_level` field. |
| A1 (M2) | C++↔Python PSV latency telemetry | **Implemented** — TelemetrySlot in SHM header, C-side write timing, Python-side read timing, bridge e2e tracking. |
| A2 (M4) | federated.cpp stales | Already implemented (full Winsock UDP) |
| S1 (M5) | #include .cu → .cuh | Done (prior session) |
| S2 | main.cpp DEBUG: Entering main() | **Fixed** — guarded with `#ifndef NDEBUG` |
| S3 | voice_system.cpp temp file pattern | **Fixed** — replaced `XXXXXX` pattern with `GetTempFileNameA` |
| Q1 | cuda_quantum_backend.h stubs | **Fixed** — `execute_hopf_projection()` and `execute_spatial_search()` both implemented with classical algorithms; `supports_subroutine` returns true for all 5. |
| Q2 | CMake CUDA-Q detection | Already upgraded to `find_package(cudaq)` + fallback (prior session) |
| — | PillarAIColab distortion_sandbox.py 9 TODOs | **Fixed** — max_events cap, Harm auto-reject, SNR top-25% algorithm, memory leak truncation in _save_data; all stale TODO comments removed. |
| — | PillarAIColab sensor_integration.py TODO | **Fixed** — replaced `os.system` with `subprocess.run`; DS18B20/DHT22 implementations verified complete. |
| — | Van_Nueman_Services/src/main.py placeholder | **Fixed** — replaced placeholder warning with concise description; fixed dict/kwargs mismatch in delete_task; API already functional. |
| — | pillar_ai_bridge.py blackboard stub | **Fixed** — `log_to_blackboard` now writes JSONL to `data/blackboard.jsonl`; `verify_pillar_constraints` implemented. |
| — | vulkan_wrapper.py stub | **Fixed** — `VulkanDevice.init()` now calls `vkCreateInstance`, `vkEnumeratePhysicalDevices`, `vkCreateDevice`, `vkGetDeviceQueue`; `cleanup()` calls `vkDestroyDevice`/`vkDestroyInstance`; `load_spirv()` calls `vkCreateShaderModule`. |
| — | social_bridge.py, tensorrt_llm_bridge.py, neural_agent.py "not implemented" prints | **Fixed** — replaced "not implemented" messages with graceful-fallback messages (all already had working fallback paths). |
| #024 | AABB inv_dir bug, pillar index mismatch, fracture probability | Fixed |
| #025 | Stubs (svo_build, deformation) | Fixed |
| #026 | Entity bitfield, Transform dead code, ScaledInt div-by-zero | Fixed |
| Phase 10 | Build environment restoration (7 CMake/build bugs fixed) | **Fixed** — see MASTER_LOG.md Phase 10 |
| Phase 10b | `native_quantum_backend.h` | **Created** — CUDA kernel backend with CPU fallback, guarded by `VN_USE_NATIVE_CUDA` |
| Phase 10c | SPIR-V post-build copy failures | **Fixed** — replaced `copy_if_different` with `scripts/copy_if_exists.cmake` to skip missing `.spv` files |
| Phase 10d | `audio_system.cpp` SFX queue full | **Fixed** — cleanup finished sounds before rejecting when queue is full |
| Phase 10e | `env_build.sh` PowerShell + CUDA path | **Fixed** — added `C:/Windows/System32/WindowsPowerShell/v1.0/` and CUDA Toolkit v13.2 to PATH |
