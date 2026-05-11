# Van Nueman Toolchain

Custom LLVM compiler toolchain — Pillar-aware compiler passes, custom target backend, and JIT compilation.

## Structure
- `llvm-toolchain/frontend/` — Pillar attributes, fractal codegen, scaled integer types
- `llvm-toolchain/backend/` — NVPTX extensions, SPIR-V emitter, custom target machine
- `llvm-toolchain/passes/` — FractalInline, PillarFusion, ScaledIntSimplify, InteractionMatrix, LogDecayOpt, SVOMemory
- `llvm-toolchain/jit/` — NeuroevolutionJIT, rule compiler, VnJIT
- `llvm-toolchain/test/` — Unit tests for each pass

## Dependencies
- LLVM 17 (built with NVPTX target)
- CMake 3.20+
- C++17 compiler

## Build
```bash
cmake -B build -DLLVM_DIR=/path/to/llvm17-install/lib/cmake/llvm
cmake --build build
```
