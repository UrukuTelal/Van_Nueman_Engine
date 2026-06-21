# Van Nueman Engine

**Scale-invariant game engine** with AI-driven agents, GPU compute physics, quantum-inspired mathematics, a custom LLVM compiler toolchain, and a 16-dimensional Pillar State Vector (PSV) system that unifies all entities from rocks to federations under a single mathematical framework.

Created by **UrukuTelal**. Built on the CrowLoki CrowClaw ecosystem.

---

## Table of Contents

- [What Is the Van Nueman Ecosystem?](#what-is-the-van-nueman-ecosystem)
- [Core Concept: Everything Is a Pillar State Vector](#core-concept-everything-is-a-pillar-state-vector)
- [Directory Map](#directory-map)
- [Build Guide](#build-guide)
- [Quick Start](#quick-start)
- [Build Outputs: Library & Executable Targets](#build-outputs-library--executable-targets)
- [CUDA Kernel Compilation System](#cuda-kernel-compilation-system)
- [Custom LLVM Toolchain](#custom-llvm-toolchain)
- [Test Suite](#test-suite)
- [Code Generation: Pillars from YAML](#code-generation-pillars-from-yaml)
- [Cross-Repo Dependencies](#cross-repo-dependencies)
- [Credits](#credits)
- [License](#license)

---

## What Is the Van Nueman Ecosystem?

The Van Nueman Engine is a **full-simulation game engine** that treats every entity in the universe — from a rock to a planet, a drone to a federation of servers — as instances of the same mathematical object: a **16-dimensional Pillar State Vector (PSV)**.

The ecosystem spans **five repositories**:

| Repository | Purpose |
|---|---|
| **Van_Nueman_Engine** (this repo) | C++ game engine: Vulkan renderer, CUDA physics, audio, agents, APIs, custom LLVM toolchain |
| **Van_Nueman_AI** (sibling) | Python AI stack: 16-pillar Pillar Council, Ollama agent orchestration, WHT protocol |
| **Van_Nueman_Agents** (sibling) | Multi-agent framework for social simulation and emergent behavior |
| **ugc-compiler** (sibling) | Universal GPU Compiler — C++ → SPIR-V for GPU compute kernels |
| **SPIRV-LLVM-Translator** (sibling) | LLVM IR ↔ SPIR-V translator by Khronos Group |

### Architecture in Plain Language

The engine simulates a world where **everything is defined by the same 16 numbers** (the pillars). A rock has gravity (Attraction), mass (Resistance), and heat (Warmth). A server has uptime (Integrity), load (Force), and users (Relation). A creature has perception (Awareness), intent (Willpower), and empathy (Warmth).

These 16 numbers drive:

- **Skelly Physics** — a fractal skeleton system: entities have bones (structure), muscles (force), organs (energy), and transports (flow). A rock's "skeleton" is its crystalline lattice; a server's is its process tree.
- **Sparse Voxel Octree (SVO)** — 8 LOD levels of volumetric rendering, deformed by physics in real time.
- **WHT Protocol** — entities communicate via Walsh-Hadamard Transform signals (32-dimensional audio packets).
- **Neuroevolution** — populations evolve interaction weights via mutation and crossover, learning optimal pillar coupling matrices.
- **FUGURE Protocol** — an anti-hallucination and error-handling system that extracts truth from adversarial or corrupt data by accelerating lies to their logical breaking point.
- **FLL (Fractal Language Layer)** — a quantum-aware thought representation and glyph rendering engine.

The engine runs a **30 Hz tick loop**: receive inputs → update pillars → resolve Skelly physics → deform voxels → rebuild SVO → render → sync.

---

## Core Concept: Everything Is a Pillar State Vector

```cpp
struct PillarStateVector {
    float pillars[16];  // Index → Meaning
    //  0: Awareness     1: Willpower     2: Force        3: Influence
    //  4: Resistance    5: Integrity     6: Cohesion     7: Relation
    //  8: Presence      9: Warmth       10: Memory      11: Attraction
    // 12: Harm         13: Distortion   14: Flux        15: Depth
};
```

These 16 pillars apply **identically at every scale**:

| Scale | Example | Key Pillars |
|---|---|---|
| Celestial | Rock, Planet | Attraction (gravity), Resistance (mass), Warmth (thermal) |
| Living | World-Serpent | Awareness (sensing), Warmth (biosphere), Memory (evolution) |
| Automated | Drone | Force (motion), Awareness (sensors), Influence (control) |
| Biological | Creature, Human | Warmth (empathy), Willpower (intent), Cohesion (social) |
| Computational | Server | Integrity (uptime), Relation (connections), Awareness (monitoring) |
| Network | Federation | Cohesion (consensus), Relation (peering), Awareness (health) |

All **game simulation logic uses scaled integers** (fp20_t = 1 << 20) for deterministic, high-performance fixed-point arithmetic. Floats appear only at API boundaries.

---

## Directory Map

### Core Engine

| Directory | Purpose |
|---|---|
| `include/` | Core type headers: `BlochMath.h`, `BlochSpace.h`, `HopfPID.h`, `Entity.h`, `Transform.h`, `SkellyTypes.h`, `GodNode.h`, `PhotonicWrapper.h`, `Constraint.h`, `Cord.h` |
| `vn/` | Foundational types used everywhere: `Fractal.h`, `PillarTypes.h`, `ScaledInt.h` (fp20_t scaled integer) |
| `main.cpp` | Application entry point. Initializes all subsystems, runs the game loop |
| `pillars.yaml` | **Single source of truth** — defines all 16 pillars, constraints, min/max/allowed targets |
| `generated/` | Codegen outputs: `PillarEnum.h`, `PillarConstraints.h`, `PillarEnumUGC.h`, `pillar_constants.py` |

### Graphics & Compute

| Directory | Purpose |
|---|---|
| `renderer/` | Vulkan renderer implementation: `vulkan_renderer.cpp/.h` — swapchain, pipelines, descriptor sets, push constants |
| `rendering/` | Higher-level rendering: opacity renderer, creature renderer, scale-organism renderer, voxel mesh generator, PSV visualization |
| `kernels/` | **13 CUDA kernels** + GLSL/SPIR-V compute shaders. Pillars, Skelly, SVO, Hopf-PID, WHT, render |
| `shaders/` | SPIR-V compute shaders: PSV compute, photonic wrapper, shadow process |
| `gui/` | ImGui-based UI: HUD, pillar overlay, main menu, settings, splash screen, loading screen, collaboration window |
| `scene/` | Scene management: chunk management, star cluster generation |

### Physics

| Directory | Purpose |
|---|---|
| `physics/` | Physics engine: `pillar_coupling` (PSV → Skelly), `deformation` (Skelly → voxel mutation), `fractal_skelly` (scale-invariant skeleton), `energy_system`, `voxel_coupling` |
| `voxel/` | Voxel data structures: `VoxelCell.h` (cell with deformation/dirty tracking), `BCCIndex.h` (body-centered cubic indexing), `DeformingVoxel.h`, `FracturePipeline.h`, `GrowthPipeline.h`, `TruncatedOctahedron.h`, `TetradSubCell.h`, `InteriorSubdivision.h`, `YieldMatrix.h` |
| `spatial/` | `SpatialGrid.h` — spatial hashing for broad-phase collision |

### Audio

| Directory | Purpose |
|---|---|
| `audio/` | Full audio subsystem built on `miniaudio.h`. WHT (Walsh-Hadamard Transform) signal processing: `wht.cpp`, `wht_comm.cpp` (inter-agent WHT comms), `wht_packet.cpp` (packet encode/decode), `wht_scaled.cpp` (scaled-integer WHT), `audio_system.cpp` (playback, capture, mixing), `voice_system.cpp` (speech I/O via whisper.cpp), `uid_pillar_matrix.h` |

### Agent & Cognition

| Directory | Purpose |
|---|---|
| `agents/` | Agent system: `cognition` (decision making), `perception` (world sensing), `fractal_spawner` (entity spawning), `agent_login` (UID matrix auth), `memory_system` |
| `cognition/` | FUGURE protocol implementation: `fugure.cpp/.h` — anti-hallucination, adversarial sandboxing, truth extraction |
| `scale/` | Scale-invariant routing: `scale_router` (multi-scale dispatch), `ScaleExponent.h`, `SemanticProjection.h`, `InfluenceBuffer.h`, `AttentionEvaluator.h` |

### Quantum & Mathematics

| Directory | Purpose |
|---|---|
| `quantum/` | Quantum-inspired math stack: Bloch sphere (`BlochMath.h`, `BlochSpace.h`), Hopf-PID control, thought engine, BCC thought environment, quantum amplitude encoding, WHT tokenizer, quantum forward pass, CUDA-Q backend bridge, cognitive pipeline |
| `fll/` | **Fractal Language Layer** — quantum-seeded thought representation: `FractalNode.h`, `QuantumSeed.h`, `ArenaAllocator.h`, `SemanticCompiler.h`, `FLLQPU.h` (CUDA-Q quantum kernel bridge), `FLLShaders.h`, `FLLRenderer.h`, `ThoughtGlyphBridge.h`, `FLL.h`. Includes QPU compute shaders and SPIR-V glyph rendering |
| `consciousness/` | High-level simulation: `DreamEngine.h` (dream state simulation), `AstralProjection.h`, `MetaphysicalRealms.h` |

### Simulation & Gameplay

| Directory | Purpose |
|---|---|
| `simulation/` | Core simulation loop: `tick_loop` (30 Hz game tick), `alife_growth` (artificial life), `attention_loop`, `transform_compute` (entity transforms), `hopf_compute` (Hopf-PID projection), `llm_worker` (Ollama LLM integration), `native_reasoning_worker` (native Hopf-PID reasoning), `cuda_pillar_compute` (GPU-accelerated pillar simulation), `cellular_automata` (CA on GPU) |
| `game_play/` | Gameplay systems: `simulation_chamber` (sandbox environment), `crispr_vault` (genetic modification), `relay_station` (inter-entity relay), `emergency_console`, `inter_scale_feedback`, `drone_fleet` |
| `scenarios/` | Standalone demo executables: BOB federated, BOB agent, WHT inter-agent, voxel evolution, Hopf-PID, chemistry, environment, society, celestial, cosmological, consciousness |
| `single_player/` | Linear tutorial mode |

### Networking & APIs

| Directory | Purpose |
|---|---|
| `network/` | Network layer: `server` (UDP game server), `client` (UDP game client), `protocol` (binary protocol), `federated` (server↔server), `feedback_loops` (pillar-driven log-decay feedback), `causal_handshake` |
| `api/` | REST API (`rest_api.cpp` — HTTP/JSON), WebSocket API (`websocket_api.cpp` — real-time events), LLM bridge (`llm_bridge.cpp` — Ollama integration), pillar API (`pillars_api_simple.cpp`, `pillars_api.cu`), skelly API (`skelly_api_simple.cpp`, `skelly_api.cu`, `skelly_api_c.cpp`) |
| `server_registry/` | Server approval system: `approval_system.cpp/.h` — manual content approval for multiplayer servers |
| `database/` | PostgreSQL persistence: `persistence.cpp`, `world_state.cpp`, `schema.sql` |
| `bindings/` | Python bindings: `entity_manager.py`, `vulkan_wrapper.py` (ctypes Vulkan) |

### Evolution & Biology

| Directory | Purpose |
|---|---|
| `neuroevolution/` | Population-based evolution: `population` (PSV population management), `fitness` (fitness evaluation), `operators` (mutation/crossover), `boundaries` (operator-set constraints), `organism_fitness`, `voxel_neuro_bridge`, `voxel_crispr_bridge` |
| `neuro/` | Entity neural network (GPU compute): entity_neural_net.cpp/.h + compute shader |
| `biology/` | Biological simulation: `biological_system`, `creature_system`, `voxel_organism`, `voxel_skelly_bridge`, material science, society network, tool use |
| `chemistry/` | Chemistry simulation: `Molecule.h`, `ChemicalBond.h`, `ReactionSystem.h`, `DiffusionSystem.h`, `EnvironmentalChemistry.h`, `ReactionDiffusion.h` |
| `celestial/` | Celestial mechanics: `PlanetarySystem.h`, `StellarSystem.h`, `GravitationalSystem.h`, `LargeScaleStructure.h`, `UniversalExpansion.h`, `CrossScaleWHTBridge.h` |

### Toolchain

| Directory | Purpose |
|---|---|
| `Van_Nueman_Toolchain/` | Custom LLVM compiler tools — see [Custom LLVM Toolchain](#custom-llvm-toolchain) section below |

### Support

| Directory | Purpose |
|---|---|
| `tests/` | 38 test files covering all subsystems — see [Test Suite](#test-suite) |
| `docs/` | Architecture overview, build system, physics, neural agents, pitch deck, game design doc |
| `scripts/` | Build helpers, pillar codegen (`generate_pillar_sources.py`), SPIR-V validation (`validate_spirv.py`), copy helpers |
| `assets/` | Game assets |
| `PillarAIColab/` | 16-pillar configuration directories, LLM interaction models, RAG store, colab blackboard |
| `skein_audit/` | SKEIN audit index and audit trail |
| `whisper.cpp/` | Git submodule — speech-to-text via whisper (opt-in, VN_USE_WHISPER=OFF by default) |

---

## Build Guide

### Prerequisites

| Dependency | Version | Purpose |
|---|---|---|
| **CMake** | ≥ 3.20 | Build system |
| **C++ compiler** | MSVC 2022, Clang 16+, GCC 12+ | C++17 compilation |
| **Vulkan SDK** | ≥ 1.3 | Graphics and compute |
| **CUDA Toolkit** | ≥ 12.x | GPU kernel compilation (detected via `find_package(CUDAToolkit)`) |
| **vcpkg** | Latest | Package manager for Vulkan, GLFW, ImGui, CURL, ZeroMQ |
| **LLVM 17** | 17.x | Custom toolchain passes and vncc compiler |
| **Python** | ≥ 3.8 | Code generation scripts, SPIR-V validation, test helpers |
| **PostgreSQL** | ≥ 14 (optional) | World state persistence |

### Step-by-Step Build

```bash
# 1. Clone with submodules
git clone --recursive https://github.com/UrukuTelal/Van_Nueman_Engine.git
cd Van_Nueman_Engine

# 2. Source the build environment
#    Sets up MSVC, vcpkg, CMake, and Python paths
source /path/to/env_build.sh
#    On Windows: cmd //c build_msvc.bat (runs vcvarsall.bat)

# 3. Configure with CMake
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release

# 4. Build
cmake --build build --config Release

# 5. Run tests
ctest --test-dir build -C Release --output-on-failure
#    Or directly:
./build/Release/VanNuemanTests.exe
```

### CMake Options

| Option | Default | Description |
|---|---|---|
| `VN_USE_WHISPER` | `OFF` | Enable whisper.cpp speech-to-text (submodule, ~650 files) |
| `VN_USE_NATIVE_REASONING` | `ON` | Use native Hopf-PID cognitive pipeline instead of Ollama LLM worker |
| `BUILD_TESTING` | `ON` | Build test executable |
| `BUILD_SINGLE_PLAYER` | `OFF` | Build single-player tutorial executable |
| `BUILD_SERVER_REGISTRY` | `OFF` | Build server registry executable |

### vcpkg Integration

The CMakeLists.txt automatically detects vcpkg through:

1. `VCPKG_ROOT` environment variable
2. `C:/vcpkg/` (common install path)
3. Visual Studio's bundled vcpkg

Required vcpkg packages: `vulkan`, `glfw3`, `imgui`, `curl`, `zeromq`.

### Windows-Specific Notes

- The vcpkg applocal post-build step requires PowerShell. If missing, set `Z_VCPKG_POWERSHELL_PATH` in CMake cache before `project()`.
- Use `build_msvc.bat` which calls `vcvarsall.bat x64` before building.
- CUDA kernels are compiled via `nvcc` directly (custom `vn_compile_cu()` CMake function), not through MSBuild's CUDA toolset.

---

## Quick Start

### Minimal Example: Create an Entity

```cpp
#include "PillarTypes.h"
#include "Entity.h"

int main() {
    // A rock: 16 pillars define everything
    Entity rock;
    rock.pillars[PILLAR_ATTRACTION]  = 0.8f;   // gravity
    rock.pillars[PILLAR_RESISTANCE]  = 0.9f;   // mass/inertia
    rock.pillars[PILLAR_WARMTH]      = 0.2f;   // thermal
    rock.pillars[PILLAR_AWARENESS]   = 0.0f;   // no perception

    // Tick the simulation
    SimulationChamber chamber;
    chamber.add_entity(rock);

    for (int tick = 0; tick < 100; ++tick) {
        chamber.tick(1.0f / 30.0f);  // 30 Hz simulation
    }

    return 0;
}
```

Build and run:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
./build/Release/van-nueman-game.exe
```

### Run a Scenario Demo

```bash
./build/Release/vn-wht-demo.exe      # WHT inter-agent communication
./build/Release/vn-bob-agent.exe      # BOB agent scenario
./build/Release/vn-voxel-evolution.exe  # Voxel evolution demo
./build/Release/vn-hopf-pid-demo.exe  # Hopf-PID membrane projection
```

### Launch the AI Web Terminal

```bash
python ../Van_Nueman_AI/scripts/wht_cli.py
# Opens admin UI at http://localhost:8889
```

---

## Build Outputs: Library & Executable Targets

### Static Libraries (16)

| Target | Source Dir | Description |
|---|---|---|
| **vncore** | `vn/`, `scale/`, `database/` | Core types: Fractal, PillarTypes, ScaledInt (fp20_t), scale router, database persistence |
| **vnphysics** | `physics/` | Pillar coupling, deformation, energy system |
| **vnnetwork** | `network/` | UDP server/client, federation, feedback loops, causal handshake |
| **vnneuroevolution** | `neuroevolution/` | Population management, fitness, mutation/crossover, boundaries, voxel bridges |
| **vnentityneuro** | `neuro/` | Entity neural network (GPU compute kernels) |
| **vnapi** | `api/` | REST API, WebSocket API, LLM bridge, pillar API, skelly API |
| **vnagents** | `agents/`, `cognition/` | Agent cognition, perception, fractal spawner, agent login, FUGURE |
| **vnscene** | `scene/` | Scene chunk management, star cluster generation |
| **vnsimulation** | `simulation/` | Tick loop, ALife growth, attention loop, transform/Hopf compute, LLM workers, native reasoning |
| **vngameplay** | `game_play/` | Simulation chamber, CRISPR vault, relay station, emergency console, inter-scale feedback |
| **vnaudio** | `audio/` | Audio system, voice I/O, WHT signal processing, WHT comms, WHT packets |
| **vnrendering** | `rendering/` | Opacity renderer, creature renderer, scale-organism renderer |
| **vnbio** | `biology/` | Biology system, creature system, voxel-skelly bridge, voxel organism |
| **vngui** | `gui/` | Collaboration window UI component |
| **vnfll** | `fll/` | Fractal Language Layer: FractalNode, QuantumSeed, ArenaAllocator, SemanticCompiler, QPU kernel bridge, ThoughtGlyphBridge, FLL renderer |
| **vncuda** | (CUDA .obj objects) | All CUDA kernel objects compiled by nvcc, combined with device-link step |

### Executables (Main + Scenarios)

| Target | Source | Description |
|---|---|---|
| **van-nueman-game** | `main.cpp` + renderer + GUI | Main game executable with Vulkan rendering, ImGui HUD, full simulation loop |
| **VanNuemanTests** | `tests/test_main.cpp` | Test runner — 163+ tests across all subsystems |
| **vn-bob-federated** | `scenarios/bob_federated_scenario.cpp` | BOB federated multi-server scenario |
| **vn-bob-agent** | `scenarios/bob_agent_scenario.cpp` | BOB single-agent scenario |
| **vn-wht-demo** | `scenarios/wht_inter_agent_demo.cpp` | WHT inter-agent communication demo |
| **vn-voxel-evolution** | `scenarios/voxel_evolution_demo.cpp` | Voxel neuroevolution integration demo |
| **vn-chemistry-demo** | `scenarios/voxel_chemistry_demo.cpp` | Chemistry/molecule layer demo |
| **vn-environment-demo** | `scenarios/voxel_environment_demo.cpp` | Environmental chemistry demo |
| **vn-society-demo** | `scenarios/voxel_society_demo.cpp` | Society, tool use, material science demo |
| **vn-celestial-demo** | `scenarios/voxel_celestial_demo.cpp` | Celestial formation, planetary differentiation |
| **vn-cosmological-demo** | `scenarios/voxel_cosmological_demo.cpp` | Large-scale structure, expansion, WHT bridge |
| **vn-consciousness-demo** | `scenarios/voxel_consciousness_demo.cpp` | Dream engine, astral projection, metaphysical realms |
| **vn-hopf-pid-demo** | `scenarios/hopf_pid_demo.cpp` | Hopf-PID membrane projection engine |
| **vn-single-player** (optional) | `single_player/tutorial.cpp` | Linear tutorial narrative |
| **vn-server-registry** (optional) | `server_registry/approval_system.cpp` | Multiplayer server approval system |

---

## CUDA Kernel Compilation System

The engine compiles **13 CUDA kernel source files** using a custom CMake function (`vn_compile_cu()`) that invokes `nvcc` directly (not through MSBuild's CUDA toolset).

### CUDA Kernel Sources

| Source File | Purpose |
|---|---|
| `kernels/pillars_compute.cu` | 16-pillar state vector updates (entity + server + federation scale) |
| `kernels/skelly_compute.cu` | Fractal skeleton physics — bones, muscles, organs, transports |
| `kernels/voxel_svo.cu` | Sparse Voxel Octree — 8 LOD levels, octohedron/cube/polyhedra math |
| `kernels/hopf_pid_kernel.cu` | Hopf-PID controller — membrane projection and stabilization |
| `kernels/wht_fp_kernel.cu` | Walsh-Hadamard Transform — fixed-point fp20_t signal processing |
| `kernels/render_image.cu` | SVO traversal + ray-march rendering (1080p) |
| `physics/pillar_coupling.cu` | Pillar coupling computations (CPU/GPU shared logic) |
| `physics/deformation.cu` | Skelly physics → voxel mutation |
| `physics/fractal_skelly.cu` | Scale-invariant fractal skeleton system |
| `api/pillars_api.cu` | GPU pillar API (fractal parameters) |
| `simulation/cellular_automata.cu` | Cellular automata simulation (GPU) |
| `simulation/cuda_pillar_compute.cu` | GPU-accelerated pillar simulation |
| `fll/shaders/fll_resonance.cu` | FLL resonance kernel — quantum-seeded thought resonance |

### Build Process

```
nvcc -x cu -ccbin <compiler> -I<includes> -rdc=true -O3 \
     --generate-code arch=compute_86,code=sm_86 \
     -o <kernel>.obj -c <kernel>.cu
```

1. Each `.cu` file is compiled to a `.obj` object via `vn_compile_cu()`
2. All objects are combined into the **vncuda** static library
3. A **device-link step** (`vn_device_link()`) links relocatable device code into `vncuda_dlink.obj` using `nvcc -dlink`
4. The dlink object is attached to `van-nueman-game` and all demo executables that depend on CUDA

Target architecture: **SM 86** (Ampere, RTX 3070). Change `arch=compute_86,code=sm_86` for other GPU generations.

---

## Custom LLVM Toolchain

Located at `Van_Nueman_Toolchain/llvm-toolchain/`, this is a custom LLVM 17-based compiler suite with Van Nueman-specific passes, attributes, and code generation.

### Architecture

```
llvm-toolchain/
├── frontend/       # vncc compiler, pillar attributes, fractal codegen
│   ├── VnCompiler.cpp     # Main compiler driver (Clang wrapper)
│   ├── PillarSema.cpp     # Semantic analysis for pillar attributes
│   ├── FractalCodeGen.cpp # Code generation for fractal patterns
│   ├── ScaledInteger.cpp  # Scaled integer type handling
│   └── AttrPillar.cpp     # Pillar attribute parsing
├── passes/         # 6 custom LLVM passes
├── backend/        # NVPTX extensions, SPIR-V emitter (Phase 3)
├── jit/            # NeuroevolutionJIT, VnJIT (Phase 4)
└── test/           # Unit tests for passes
```

### LLVM Passes (6)

| Pass | File | Purpose |
|---|---|---|
| **PillarFusionPass** | `PillarFusionPass.cpp` | Fuses pillar interaction loops, eliminates redundant 16×16 matrix lookups |
| **ScaledIntSimplifyPass** | `ScaledIntSimplifyPass.cpp` | Optimizes fp20_t scaled integer ops: mul/div by 2²⁰ → shift, masking patterns |
| **InteractionMatrixPass** | `InteractionMatrixPass.cpp` | Specializes constant-index matrix loads, hoists loop-invariant addresses |
| **LogDecayOptPass** | `LogDecayOptPass.cpp` | Optimizes `signal / logf(hop + 1)` patterns with LUT or polynomial approximation |
| **SVOMemoryPass** | `SVOMemoryPass.cpp` | Optimizes SVO node pool: GEP hoisting, cache-friendly reordering, prefetch insertion |
| **FractalInlinePass** | `FractalInlinePass.cpp` | Inlines `[[fractal]]`-annotated functions with scale constant propagation (entity/server/federation) |

### vncc Compiler (vncc.exe)

The **vncc** compiler wraps Clang to support Van Nueman-specific attributes:

```bash
vncc --emit-spirv -o kernel.spv kernel.cpp    # C++ → SPIR-V
vncc --emit-ptx -o kernel.ptx kernel.cpp       # C++ → PTX
vncc --emit-llvm -o kernel.ll kernel.cpp       # C++ → LLVM IR
vncc -O -o a.out kernel.cpp                    # C++ → native
```

Custom attributes: `[[pillar_vector]]`, `[[scaled(N)]]`, `[[fractal]]`.

The SPIR-V pipeline: `C++ → (Clang) → LLVM IR → (llvm-spirv) → SPIR-V`.

### SPIR-V Kernel Compilation (UGC Pipeline)

The engine's CMakeLists.txt also supports the **ugc-compiler** (Universal GPU Compiler) for compiling C++ kernel sources to SPIR-V `.spv` files:

```
ugc.exe kernel.cpp -o kernel.spv --spv-version 1.6 -O3
llvm-spirv → validate_spirv.py → copy to output directory
```

All `.spv` files are copied to the game executable directory via `POST_BUILD` steps.

---

## Test Suite

The test suite contains **163+ tests** across **30+ test suites**, all compiled into the single `VanNuemanTests.exe` executable.

### Test Files

| Test File | Tests |
|---|---|
| `test_entity.h` | Entity construction, pillar state vectors, serialization |
| `test_transform.cpp` | Transform math (direct TransformCore.h tests) |
| `test_transform_pipeline.h` | Full transform pipeline |
| `test_pillar_coupling.h` | Pillar→Skelly→deformation coupling |
| `test_scaled_int.h` | fp20_t arithmetic, bitwise ops, overflow |
| `test_neuroevolution.h` | Population, fitness, mutation, crossover |
| `test_vulkan.h` | Vulkan device, swapchain, pipeline creation |
| `test_chunk.h` | SVO chunk management |
| `test_spatial_hash.h` | Spatial grid queries |
| `test_fracture.h` | Voxel fracture pipeline |
| `test_voxel_mesh.h` | Voxel mesh generation |
| `test_voxel_coupling.h` | Voxel→physics coupling |
| `test_orbital.h` | Orbital mechanics |
| `test_llm_worker.h` | LLM worker (native + Ollama paths) |
| `test_hopf_pid.h` | Hopf-PID controller |
| `test_quantum_amplitude.h` | Quantum amplitude encoding |
| `test_quantum_forward_pass.h` | Quantum forward pass |
| `test_wht_fp.h` | WHT fixed-point transforms |
| `test_wht_tokenizer.h` | WHT tokenization |
| `test_wht_audio.h` | WHT audio signal processing |
| `test_thought_engine.h` | Thought engine |
| `test_thought_glyph_bridge.h` | Thought↔glyph bridge |
| `test_cognitive_pipeline.h` | Cognitive pipeline |
| `test_bcc_thought_env.h` | BCC thought environment |
| `test_adversarial_fixes.h` | Adversarial input handling |
| `test_bloch_cross_validate.h` | Bloch sphere cross-validation |
| `test_bloch_space.h` | Bloch space math |
| `test_gpu_cpu_cross_validate.h` | GPU/CPU cross-validation (4 tests) |
| `test_fll.cpp` | FLL Engine (FractalNode, QuantumSeed, SemanticCompiler) |
| `test_fugure.cpp` | FUGURE anti-hallucination protocol |
| `test_scale_router.cpp` | Scale router dispatch |
| `test_glyph_only.cpp` | Glyph rendering |
| `test_persistence.h` | PostgreSQL persistence |
| `test_api.py` | REST API integration test (Python) |
| `test_rest_api.py` | REST API endpoint tests (Python) |
| `test_rest_api.py` | REST API |
| `check_kernel_compat.py` | SPIR-V kernel compatibility static analysis (Python) |

### Running Tests

```bash
# Build and run all tests
cmake --build build --config Release
./build/Release/VanNuemanTests.exe

# Via ctest
ctest --test-dir build -C Release --output-on-failure

# Run kernel compatibility check
python tests/check_kernel_compat.py

# Run API tests
python tests/test_rest_api.py
```

---

## Code Generation: Pillars from YAML

The `pillars.yaml` file at the project root is the **single source of truth** for the 16-pillar system.

```yaml
pillars:
  - name: Awareness
    index: 0
    constraints: { min: 0.0, max: 1.0 }
    allowed_targets: [entity, server, federation]
  - name: Willpower
    index: 1
    ...
```

Running `scripts/generate_pillar_sources.py` regenerates three files in `generated/`:

| Generated File | Contents |
|---|---|
| `PillarEnum.h` | `enum PillarIndex { PILLAR_AWARENESS = 0, ... }` |
| `PillarConstraints.h` | `PID_CONSTRAINTS[16]` + `is_valid_pillar_target()` |
| `pillar_constants.py` | Python constants matching the C++ definitions |

The codegen runs automatically at build time when `pillars.yaml` is newer than the generated files (requires Python).

---

## Cross-Repo Dependencies

The Van Nueman Engine integrates with two sibling repositories:

### Van_Nueman_AI (`../Van_Nueman_AI/`)

The Python AI stack provides:

- **Web terminal** (`scripts/wht_cli.py`) — serves admin UI on port 8889
- **Task delegation** (`scripts/task_delegator.py`) — routes work to local Ollama models
- **Pillar Council** (`Van_Nueman_Social_Sim/pillar_council/`) — 16-pillar approval system
- **Pillar configs** (`Pillar_00_*` through `Pillar_15_*`) — 16-pillar constraint definitions
- **Pipeline orchestration** (`scripts/pillar_task_delegate.py`) — multi-step agent workflows

Launch from the engine root:

```bash
python ../Van_Nueman_AI/scripts/wht_cli.py
```

### Van_Nueman_Agents (`../Van_Nueman_Agents/`)

Multi-agent framework for social simulation, emergent behavior, and inter-agent WHT communication.

### ugc-compiler (`C:/Projects/ugc-compiler/`)

Universal GPU Compiler used to compile C++ kernel sources to SPIR-V `.spv` files. When detected, the engine's CMakeLists.txt uses it as the primary kernel compilation path (over the older vncc pipeline).

### SPIRV-LLVM-Translator

Provides `llvm-spirv.exe` for the LLVM IR → SPIR-V translation step in the shader compilation pipeline.

---

## Credits

**Van Nueman Engine** by **UrukuTelal**.

### CrowLoki CrowClaw Ecosystem

- **crowclaw-public-release** — Core CrowClaw framework
- **CrowNest-Public** — Multi-agent environment and social simulation

### Third-Party Libraries

| Library | Author | License | Usage |
|---|---|---|---|
| **CUDA-Quantum** | NVIDIA | NVIDIA EULA | Quantum kernel compilation and simulation (`__qpu__` kernels) |
| **whisper.cpp** | ggerganov | MIT | Speech-to-text (submodule, optional) |
| **piper** | RHasspiper | MIT | Text-to-speech (submodule) |
| **LLVM** | LLVM Foundation | Apache 2.0 w/ LLVM exceptions | Custom toolchain passes, vncc compiler |
| **SPIRV-LLVM-Translator** | Khronos Group | Apache 2.0 w/ LLVM exceptions | LLVM IR ↔ SPIR-V translation |
| **Vulkan** | Khronos Group | MIT | Graphics and compute API |
| **GLFW** | GLFW Contributors | zlib/libpng | Window management and input |
| **Dear ImGui** | Omar Cornut | MIT | GUI system |
| **miniaudio** | David Reid | Public Domain / Unlicense | Audio playback and capture |
| **stb_image / stb_truetype** | Sean Barrett | Public Domain | Image and font loading |
| **CUDA Toolkit** | NVIDIA | NVIDIA EULA | GPU compute kernel compilation |

---

## License

Copyright © 2026 **Van Nueman Project**. Licensed under the **MIT License**.

See [docs/GAME_LICENSE.md](docs/GAME_LICENSE.md) for full license text and third-party license details.

```
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.
```
