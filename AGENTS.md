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
# Source environment first (cmake, MSVC, vcpkg, Python paths):
source /c/Projects/env_build.sh

# Configure and build (VS 17 2022 generator):
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release -T "v143"
cmake --build build --config Release

# Or Windows: Use build_msvc.bat (sets up MSVC environment via vcvarsall.bat)
cmd //c build_msvc.bat

# Run tests:
./build/Release/VanNuemanTests.exe

# Or via ctest:
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
Submodules: `whisper.cpp/`, `piper/`

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
- **vcpkg applocal step requires PowerShell** – vcpkg's post-build step copies DLLs via `powershell.exe`. If missing from PATH, either run via `build_msvc.bat` (which adds `%SystemRoot%\System32\WindowsPowerShell\v1.0\`) or set `Z_VCPKG_POWERSHELL_PATH` in CMake cache before `project()`.
- **whisper.cpp is opt-in** – `VN_USE_WHISPER=OFF` by default. The `audio/voice_system.cpp` already guards with `VOICE_ENABLED=0`, but set `-DVN_USE_WHISPER=ON` to build the submodule (~650 files).

## Build Environment (Post-Windows-Reset)

After a Windows reset, restore the build environment with:
```bash
# 1. Install cmake
winget install Kitware.CMake --accept-source-agreements --accept-package-agreements

# 2. Install MSVC Build Tools to user-local path (no admin needed)
curl -sL -o /tmp/vs_buildtools.exe "https://aka.ms/vs/17/release/vs_BuildTools.exe"
/tmp/vs_buildtools.exe --installPath "C:\Users\aobie\.vs\BuildTools" --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --quiet --wait

# 3. Install Python 3.11
winget install Python.Python.3.11 --accept-source-agreements --accept-package-agreements

# 4. Source the env script (adds cmake, MSVC, vcpkg, Python to PATH)
source /c/Projects/env_build.sh

# 5. Install pip deps
pip install pyyaml numpy fastapi uvicorn pydantic websockets requests
```

The environment is recovered. See `MASTER_LOG.md Phase 10` for the full restoration log.

## Cross-Repo Dependencies

- `CMakeLists.txt:356` references `Van_Nueman_Toolchain/llvm-toolchain/build/frontend/Release/vncc.exe` (in-repo sibling)
- `agents/agent_login.cpp` writes UID matrices to `data/uid_matrices/` (local)
- **Van_Nueman_AI** sibling repo (`../Van_Nueman_AI/`) provides the AI agent framework, council, and scripts
- AI repo's `scripts/create_ca_header.py` references this repo at `../Van_Nueman_Engine/`

## Van_Nueman_AI Integration

The sibling **Van_Nueman_AI** repo (`../Van_Nueman_AI/`) provides the AI agent framework:
- **Web terminal**: `python ../Van_Nueman_AI/scripts/wht_cli.py` — serves the admin UI on port 8889
- **Task delegation**: `../Van_Nueman_AI/scripts/task_delegator.py` — routes work to local Ollama models
- **Pipeline orchestration**: `../Van_Nueman_AI/scripts/pillar_task_delegate.py` — multi-step agent workflows with Pillar Council approval
- **Pillar configs**: `../Van_Nueman_AI/Pillar_00_*` through `Pillar_15_*` — 16-pillar constraint system

Run the web terminal from the repo root:
```bash
python ../Van_Nueman_AI/scripts/wht_cli.py
```

## Key Reference Files

| File | What it contains |
|---|---|
| `CMakeLists.txt` | Full build system, target list, vcpkg integration |
| `.github/workflows/ci.yml` | CI pipeline (lint/build/test) |
| `.gitmodules` | Submodule URLs |
| `docs/architecture.md` | System architecture overview |
| `README.md` | Quick start and build instructions |
| `pillars.yaml` | Single source of truth — 16 pillar definitions with constraints (min/max/allowed targets) |
| `generated/PillarEnum.h` | Codegen: `enum PillarIndex` from `pillars.yaml` |
| `generated/PillarConstraints.h` | Codegen: `PID_CONSTRAINTS[16]` + `is_valid_pillar_target()` |
| `generated/pillar_constants.py` | Codegen: Python pillar constants |
| `scripts/generate_pillar_sources.py` | Codegen script — run to regenerate all `generated/*` outputs |
