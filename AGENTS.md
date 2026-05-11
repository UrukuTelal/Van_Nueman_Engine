# AGENTS.md — Van Nueman Engine

Standalone repository for the Van Nueman game engine and custom LLVM toolchain, split from the original monorepo.

## Component Map

| Directory | Purpose | Tech | Quick Build/Run |
|---|---|---|---|
| `.` (root) | Core C++ game engine (Vulkan, physics, agent cognition) | C++17, CMake, Vulkan, vcpkg | `cmake -B build && cmake --build build` |
| `Van_Nueman_Toolchain` | Custom LLVM compiler passes + vncc shader compiler | C++17, CMake, LLVM 17 | `cmake -B build -DLLVM_DIR=...` |

## Key Commands

### Engine (root)
```bash
# Windows: Use build_msvc.bat (sets up MSVC environment via vcvarsall.bat)
cmd //c build_msvc.bat

# Or manual build (requires Visual Studio Developer Command Prompt):
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

CI: `.github/workflows/ci.yml` (Windows MSVC+vcpkg, Linux gcc, clang-format lint)

**Build outputs:**
- `build/van-nueman-game.exe` - Main game executable
- `build/VanNuemanTests.exe` - Test executable

Clone with `--recursive` or run after clone:
```bash
git submodule update --init --recursive
```
Submodules: `PillarAIColab/`, `whisper.cpp/`, `piper/`

### Toolchain (`Van_Nueman_Toolchain/llvm-toolchain/`)
```bash
cmake -B build -DLLVM_DIR=/path/to/llvm17-install/lib/cmake/llvm
cmake --build build
```

**Custom Shader Compiler (vncc.exe):**
- Located at: `Van_Nueman_Toolchain/llvm-toolchain/build/frontend/Release/vncc.exe`
- Used by Engine's CMakeLists.txt to compile `.cpp` kernels to SPIR-V (`.spv`)
- Enable by ensuring vncc.exe exists at the expected path before building

## Critical Gotchas

- **Engine builds with C++17**, not C++20 (despite README). `CMakeLists.txt` sets `CMAKE_CXX_STANDARD 17`.
- **Engine requires MSVC environment** - Use `build_msvc.bat` which calls `vcvarsall.bat x64` to set up the Visual Studio environment before building.
- **Engine requires vcpkg** for Vulkan, glfw3, imgui. Set `VCPKG_ROOT` env var or CMake falls back to hardcoded paths.
- **Custom LLVM toolchain (vncc.exe)** - Shader compiler at `Van_Nueman_Toolchain/llvm-toolchain/build/frontend/Release/vncc.exe` must exist for kernel SPIR-V compilation.
- **Piper dev moved upstream** to `github.com/OHF-Voice/piper1-gpl`. `piper/` submodule is stale.
- **Duplicated forks**: `whisper.cpp` appears as submodule and also in the AI repo. Use submodule for engine-integrated work.

## Cross-Repo Dependencies

- `CMakeLists.txt:356` references `Van_Nueman_Toolchain/llvm-toolchain/build/frontend/Release/vncc.exe` (in-repo sibling)
- `agents/agent_login.cpp` writes UID matrices to `data/uid_matrices/` (local)
- **PillarAIColab** submodule provides the AI agent framework; the sibling **Van_Nueman_AI** repo has the standalone AI stack
- AI repo's `scripts/create_ca_header.py` references this repo at `C:/Projects/Van_Nueman_Engine/`

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

## Key Reference Files

| File | What it contains |
|---|---|
| `CMakeLists.txt` | Full build system, target list, vcpkg integration |
| `.github/workflows/ci.yml` | CI pipeline (lint/build/test) |
| `.gitmodules` | Submodule URLs |
| `docs/architecture.md` | System architecture overview |
| `README.md` | Quick start and build instructions |
