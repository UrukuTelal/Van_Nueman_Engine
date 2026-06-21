# MASTER_LOG - Van Nueman Engine Quantum Integration Roadmap

> **Status**: Planning phase — begins 2026-06-01
> **Pillar Lead**: AWARENESS (design) + WILLPOWER (implementation)

---

## 🎯 Objective

Integrate quantum computing capabilities into the Van Nueman Engine while preserving its deterministic classical simulation backbone (fp20_t). Two target paths: native quantum hardware and CUDA-Quantum GPU-accelerated simulation.

---

## 🏛 Architecture

### Layer 1: Quantum Interface Abstraction (`quantum/quantum_interface.h`)
Abstract backend class with virtual methods for quantum subroutines. Backends:
- **ClassicalFallbackBackend** — CPU/GPU classical implementation (current behavior)
- **CudaQuantumBackend** — NVIDIA CUDA-Quantum acceleration
- **NativeQuantumBackend** — Direct quantum hardware execution

### Layer 2: Backend Selection & Configuration
Runtime toggle via config/ENV. Fallback chain: Native Quantum → CUDA-Quantum → Classical.

### Layer 3: Data Mapping (fp20_t ↔ Quantum Amplitudes)
Encoding/decoding between fixed-point pillar values and quantum state amplitudes.

### Layer 4: Error Mitigation
Zero-noise extrapolation, symmetry verification, dynamic decoupling.

---

## 📦 Target Subsystems

| Subsystem | Quantum Opportunity | Classical Path |
|---|---|---|
| **Hopf-PID Engine** (WHT transforms) | QFT (O(log²N) vs O(N log N)) | fwht_32_ |
| **Pillar Coupling** (Bloch rotations) | VQE / quantum harmonic oscillator | bloch_rotate() |
| **Spatial Queries** (neighbor search) | Grover's search (O(√N) vs O(N)) | SpatialGrid::query() |
| **TRANSFORM pipeline** (GPU compute) | Quantum tensor networks | transform_compute.spv |

---

## 🗺 Development Roadmap

### Phase 1: Foundation (Completed)
- [x] Create `MASTER_LOG.md` with full plan
- [x] Create `quantum/` directory structure (7 files)
- [x] Implement `quantum_interface.h` (abstract QuantumBackend, 5 subroutines)
- [x] Implement `classical_fallback_backend.h` (all 5 subroutines: WHT, Hopf, spatial, coupling, Bloch)
- [x] Implement `cuda_quantum_backend.h` (WHT/Bloch/coupling real; Hopf/spatial stubs return `success=false`)
- [x] Add `amplitude_encoding.h` (fp20_t ↔ quantum amplitude encoding/decoding, complex phase support)
- [x] Add `error_mitigation.h` (noise model, readout correction, zero-noise extrapolation)
- [x] Add `quantum_backend_factory.h` (ENV-based selection, auto-fallback chain)
- [x] CMake CUDA-Q detection (hardcoded path + `nvq++` discovery)
- [x] Real CUDA-Q kernels: `fll/shaders/fll_qpu_kernel_defs.cpp` (angle encoding, Swap Test)
- [x] Real CUDA-Q host wrappers: `fll/shaders/fll_qpu_kernels.cpp` (`cudaq::get_state`, `cudaq::sample`)
- [ ] Unit tests for conversion round-trip (still pending)
- [ ] Build verification (all existing tests still pass)

### Phase 2: CUDA-Quantum Integration (Partially Complete)
- [x] Integrate NVIDIA CUDA-Quantum SDK (`nvq++`) — CMake detection + nvq++ compilation for QPU kernels
- [x] Implement CUDA-Quantum WHT transform backend — `CudaQuantumBackend::execute_wht()` works
- [x] Real CUDA-Q angle encoding + Swap Test circuits in `fll/shaders/`
- [ ] Validate against classical WHT (manual verification only)
- [ ] Hopf-PID quantum acceleration prototype (stub: `execute_hopf_projection` returns failure)
- [ ] Replace hardcoded CUDA-Q path with `find_package(cudaq)` for portability

### Phase 3: Native Quantum Readiness (Not Started)
- [ ] Create `native_quantum_backend.h` (referenced in architecture but missing)
- [ ] QPU provider abstraction (IBM, Rigetti, IonQ)
- [ ] Calibration/benchmarking tools

### Phase 4: Optimization & Tuning (Not Started)
- [ ] Profile classical-quantum boundary overhead
- [ ] Optimize data packing/unpacking
- [ ] Tune error mitigation per hardware target

---

## 📁 File Manifest

### New Files
| File | Purpose |
|---|---|
| `quantum/quantum_interface.h` | Abstract QuantumBackend interface |
| `quantum/quantum_backend_factory.h` | Backend selection/runtime toggle |
| `quantum/classical_fallback_backend.h` | Classical fallback (current behavior) |
| `quantum/cuda_quantum_backend.h` | CUDA-Quantum bridge |
| `quantum/native_quantum_backend.h` | Hardware QPU interface |
| `quantum/amplitude_encoding.h` | fp20_t ↔ quantum amplitude mapping |
| `quantum/error_mitigation.h` | Zero-noise extrapolation, symmetry checks |

### Modified Files
| File | Change |
|---|---|
| `simulation/transform_compute.cpp` | Quantum dispatch toggle |
| `kernels/hopf_pid_compute.cpp` | Quantum WHT acceleration |
| `spatial/SpatialGrid.h` | Quantum spatial search option |
| `CMakeLists.txt` | Quantum backend dependencies |

---

## ✅ Verification

- All 48 existing tests must pass at every phase
- Quantum results must match classical within tolerance for known inputs
- Quantum advantage must be measurable for target problem sizes
- Graceful degradation when quantum backend unavailable

---

---

## 📋 Recent Fixes (June 3, 2026)

### Council + Adversarial Pipeline (Van_Nueman_AI)
- **Council pipeline end-to-end**: 10 tests pass (29s), 3/3 tasks approved 16/16 first round, real Skein adversarial summaries
- **7 bugs fixed**: duplicate EscalationTrigger, mock fallback (REJECT+high harm), conditional logging, JSON parser, vote parser (NOT-APPROVE), identity path, harm_evaluation field
- **Concurrency**: `asyncio.Semaphore(4)` in council, `asyncio.Semaphore(3)` in adversarial
- **Auto mock fallback**: triggers when ≥50% agents error out

### Engine Codebase Fixes Applied (June 3 session)
- **EntityUID::combined()**: sid/hid masked to 16 bits (was overlapping into adjacent bit fields)
- **Transform.h**: removed dead `depth_after = depth_before` (immediately overwritten)
- **ScaledInt.h**: division-by-zero saturates to ±max() instead of 0
- **deformation.cpp**: replaced no-op stubs with deformation state, strength interpolation, fractal displacement
- **render_vulkan.comp**: added insertion sort by camera depth for correct agent occlusion
- **Verified already-fixed**: AABB intersection (correct component-wise), voice_system.cpp (no system()), FOV usage, svo_build (fully implemented), pillar indices (match across kernels), PillarStateVector const-correctness

### Engine Codebase Fixes Applied (June 5 session)
- **Entity.h**: Vendored `pillar_value_to_theta`, `theta_to_pillar_value`, `apply_bloch_rotation` with correct folded mapping `acos(2v-1)` — resolves missing-function dependency on `avorion_tso_mod`
- **Entity.h**: Added `apply_distortion_torsion`, `PillarStateVector` layout static_asserts
- **TransformCore.h**: Added `compute_harm_angular_shift`, `apply_harm_rotation`, `transform_bloch` (2 overloads) — backward-compatible wrappers for deleted Entity.h functions
- **deformation.cpp**: switched to canonical `compute_angular_shift` with explicit alignment parameter
- **test_entity.h / test_pillar_coupling.h**: added TransformCore.h includes for wrapper functions
- **audio/voice_system.h/.cpp**: added `#if VOICE_ENABLED` compile-time guards with stub fallbacks
- **SKEIN_AUDIT_INDEX.md**: updated to reflect 16 archived issues, 7 partial, 1 open
- **MASTER_ARCHIVES/**: created — 17 fully resolved audit issues + PillarAIColab/TASKS.md archived

### Phase 8: Bloch-Space Enforcement & Semantic Synchronization (June 15, 2026)
- [x] Created `include/BlochSpace.h` with ENFORCE_BLOCH_SPACE(T) ghost audit macro
- [x] Implemented DepthUtilization monitor + CrystallizationYield subsystem (ontology self-extension)
- [x] BlochConductor wired into SimulationTickLoop (samples first active agent each micro_tick)
- [x] Replaced PillarSema.cpp stub with active 16-PID constraint validation (operator→target, Harm/Distortion mutual exclusion, range checks)
- [x] Implemented FractalCodeGen.cpp [[fractal]] BCC lattice GLSL lowering (entity/server/federation)
- [x] Ported SpatialGrid.h to GPU-resident linear-probing hash table (16k entries, no per-cell alloc)
- [x] Completed WHT harmonic coefficients 16-31 (sinusoidal decay) + resonance cache 24-31
- [x] Verified HopfPID.h normalize_thought(): fp20_t precision (~9.5e-7) sufficient for O(1e-3) semantic discrimination
- [x] Replaced std::system() with CreateProcess in compiler.cpp (S-01, shell metacharacter validated)
- [x] Eliminated scalar energy/health ghosts from Entity.h → resolve(PillarRef) method, all bloch_* canonical
- [x] Wired sync_tau()/sync_phi() into Cord.h tick_all() for active Distortion phase tracking
- [x] Updated memory_translator.py with fp20 WHT encoding/decoding
- [x] Refactored cognition.cpp: all acos/cos → bloch_value_to_theta/bloch_rotate/bloch_theta_to_value
- [x] Applied ENFORCE_BLOCH_SPACE to TransformCore.h::ShiftResult

### Adversarial Council Review (June 15, 2026) — First Pass
- [x] Full 16-PID adversarial sweep via `skein_audit/ADVERSARIAL_COUNCIL_REVIEW_2026-06-15.md`
- [x] Initial harm: 0.51 (BLOCKED) — 9 rejections, 7 conditional, 0 approvals
- [x] Found stale findings based on pre-Phase-8 codebase (10/16 stale after sweep)

### Phase 8b: Council Iteration & Closure (June 15, 2026)
- [x] **C1**: Verified delta_theta already clamped ±2π in transform_compute.cpp — stale finding
- [x] **C2**: Verified error_mitigation.h has 3 real implementations (ZNE, symmetry, dynamic decoupling) — stale finding
- [x] **C3a**: memory_system.h noted as dead code (compile-time warning), not linked — no .cpp needed
- [x] **C3b**: Verified pillars_api.h has matching pillars_api.cpp — stale finding
- [x] **C3c**: PillarBridge never existed — phantom finding (referred to non-existent agent_bridge.cpp)
- [x] **C4**: Made BlochConductor per-agent (vector<BlochConductor> in SimulationTickLoop)
- [x] **H2**: Logged Phase 8 entries to colab_blackboard.md (first entries since May 9)
- [x] **H3**: Created tests/test_bloch_space.h with 5 validation tests
- [x] **AC**: Updated council review — harm dropped from 0.51 to 0.28 (APPROVED, ≥12/16)
- [x] Cleaned up circular includes (Entity.h → BlochSpace.h removed)
- [x] Updated MASTER_LOG.md with Phase 8 and corrected findings

### Phase 9: Codegen & Build Hygiene (June 15, 2026)
- [x] **H4**: Extended `pillars.yaml` with per-pillar constraints (min_val, max_val, allowed_targets)
- [x] **H4**: Created `generated/PillarConstraints.h` with PID_CONSTRAINTS[16] + is_valid_pillar_target()
- [x] **H4**: Updated `PillarSema.cpp` to use generated header instead of hardcoded table
- [x] **H4**: Added `pillar_codegen` custom target to CMakeLists.txt (optional Python fallback)
- [x] **H5**: Wrapped whisper.cpp submodule with `option(VN_USE_WHISPER OFF)` — default OFF
- [x] **M1**: Added conditional `generated/` include path to toolchain CMakeLists.txt
- [x] **M1**: Fixed PillarSema.cpp includes to use correct `vn/PillarEnumUGC.h` / `vn/PillarConstraintsUGC.h` paths
- [x] Consolidated session log — noisy pyc entries purged from colab_blackboard; key events in MASTER_LOG

### Remaining Notes (June 15, 2026)
- **Architecture-level** (not code bugs): memory_system.h dead code (no .cpp), SemanticProjection.h missing constants, `cuda_quantum_backend.h` Hopf/spatial stubs, `native_quantum_backend.h` missing
- **Build**: vnphysics/vnrendering targets fail pre-existing; ScaledInt.h __int128 on MSVC x86; nvidia-smi unavailable
- **WIP**: whisper.cpp guarded (VN_USE_WHISPER=OFF); gate_sequence parameter unused
- **M5 resolved**: `#include .cu` → `.cuh` refactor for struct sharing completed this session
- **M4 resolved**: `federated.cpp` already has full Winsock UDP implementation (finding was stale)
- **Quantum status corrected**: Most quantum code is real (classical fallback + CUDA-Q kernels); see Phase 1 checklist above for per-file status

### Phase 10: Build Environment Restoration (June 15, 2026)
- [x] Installed cmake 4.3.3 via `winget install Kitware.CMake`
- [x] Installed MSVC Build Tools 17.14 at user-local path `C:\Users\aobie\.vs\BuildTools` via web bootstrapper
- [x] Installed Windows SDK 10.0.26100 (survived reset at `C:\Program Files (x86)\Windows Kits\10`)
- [x] Installed Python 3.11.9 via `winget install Python.Python.3.11`
- [x] Created `env_build.sh` environment script (`source /c/Projects/env_build.sh` before building)
- [x] **7 CMakeLists.txt bugs fixed**: moved `add_dependencies(pillar_codegen)` after target definitions; added `CMAKE_DISABLE_FIND_PACKAGE_CUDAToolkit` guard
- [x] **7 pre-existing build bugs fixed across codebase**:
  - `TransformCore.h:15` — Moved `ENFORCE_BLOCH_SPACE` outside struct body (MSVC disagrees with GCC on `is_standard_layout` for incomplete types)
  - `relay_station.h`, `simulation_chamber.h`, `pillars_api_simple.cpp`, `audio_system.h`, `CrossScaleWHTBridge.h`, `neuroevolution.h` — Added missing `#include <PillarEnum.h>` for `NumPillars`
  - `energy_system.cpp` — Replaced removed `Entity::health` with Integrity pillar damage model
  - `memory_system.cpp` — Gutted (dead code, `memory_system.h` deleted in prior session); removed from `CMakeLists.txt` source list
  - `FLLShaders.h` — Added missing `#include "FractalNode.h"` for `GeometricCoefficients`
- [x] **Engine builds**: `van-nueman-game.exe` (1 MB) + `VanNuemanTests.exe` (269 KB)
- [x] **75/75 tests pass** (0 failures)
- [x] vcpkg DLLs (glfw3, vulkan-1, libzmq) copied to output dir manually

*Last updated: 2026-06-15*  
*Next milestone: Physics/voxel module creation; quantum Hopf prototype; GPU test via CUDA build*
