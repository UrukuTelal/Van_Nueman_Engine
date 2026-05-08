# Van Nueman Engine

Core C++ game engine — Vulkan rendering, physics, audio, agent cognition, GPU compute, and gameplay systems.

## Dependencies
- **CMake** 3.20+
- **Vulkan SDK** 1.3+
- **C++20** compiler (MSVC 2022, Clang 16+, GCC 12+)
- **LLVM 17** (for custom toolchain integration)

## Quick Start
```bash
git clone --recursive https://github.com/UrukuTelal/Van_Nueman_Engine.git
cd Van_Nueman_Engine
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release
```

## Submodules
- `PillarAIColab` — 16-pillar AI constraint system (scripts, web terminal, agent pipeline)
- `whisper.cpp` — Speech-to-text
- `piper` — Text-to-speech

## PillarAIColab Integration
The PillarAIColab submodule provides the AI agent framework:
- **Web terminal**: `python PillarAIColab/scripts/wht_cli.py` — serves the admin UI on port 8889
- **Task delegation**: `PillarAIColab/scripts/task_delegator.py` — routes work to local Ollama models
- **Pipeline orchestration**: `PillarAIColab/scripts/delegation_pipeline.py` — multi-step agent workflows
- **Pillar configs**: `PillarAIColab/Pillar_00_*` through `Pillar_15_*` — 16-pillar constraint system

Run the web terminal from the repo root:
```bash
python PillarAIColab/scripts/wht_cli.py
```

## Architecture
The engine is built as 14 static libraries + 5 executables:
- **vncore** — Core types (Fractal, PillarTypes, ScaledInt)
- **vnrendering** — Vulkan render pipeline
- **vnphysics** — Physics + pillar coupling
- **vnagents** — Agent cognition system
- **vngameplay** — Gameplay systems
- **vnneuroevolution** — Population-based evolution
- _(see CMakeLists.txt for full list)_
