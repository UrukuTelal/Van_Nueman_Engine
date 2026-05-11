# Build System Documentation#

## Overview#

Van Nueman uses:
- **CMake 3.20+** for build configuration
- **vcpkg** for dependency management
- **Custom LLVM 17** toolchain for SPIR-V compilation
- **Vulkan SDK 1.3+** for graphics and compute
- **Visual Studio 2019+** (Windows) or GCC 9+ (Linux)

---

## Prerequisites#

### Windows#

| Requirement | Version | Install Location |
|--------------|---------|------------------|
| **Visual Studio** | 2019+ | C:/Microsoft Visual Studio/18/Professional |
| **Vulkan SDK** | 1.3+ | https://vulkan.lunarg.com/ |
| **CMake** | 3.20+ | https://cmake.org/download/ |
| **vcpkg** | Latest | C:/vcpkg/ |
| **Python** | 3.8+ | https://python.org/downloads/ |

### Linux#

```bash
sudo apt update
sudo apt install build-essential cmake vulkan-tools libvulkan-dev
```

---

## vcpkg Setup#

### Installation#

```bash
# Clone vcpkg
git clone https://github.com/microsoft/vcpkg.git C:/vcpkg
cd C:/vcpkg

# Bootstrap
./bootstrap-vcpkg.bat  # Windows
# or
./bootstrap-vcpkg.sh  # Linux
```

### Install Dependencies#

```bash
# Install required packages
vcpkg install glfw3 imgui curl vulkan

# For Windows (x64-windows triplet)
vcpkg install glfw3:x64-windows imgui:x64-windows curl:x64-windows
```

---

## CMake Configuration#

### Toolchain File#

The project uses vcpkg toolchain automatically:

```cmake
# CMakeLists.txt (lines 3-13)
if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
endif()
```

### Build Types#

| Type | CMake Flag | Optimization | Use Case |
|------|-------------|---------------|---------|
| **Release** | `-DCMAKE_BUILD_TYPE=Release` | -O3 | Production (default) |
| **Debug** | `-DCMAKE_BUILD_TYPE=Debug` | -g | Development |
| **RelWithDebInfo** | `-DCMAKE_BUILD_TYPE=RelWithDebInfo` | -O2 -g | Profiling |

### Configure Command#

```bash
# Windows (Visual Studio)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Linux (Unix Makefiles)
cmake -B build -DCMAKE_BUILD_TYPE=Release -G "Unix Makefiles"
```

---

## Building#

### Windows#

```bash
# Build all targets
cmake --build build --config Release

# Build specific target
cmake --build build --config Release --target VanNueman
cmake --build build --config Release --target vn-bob-agent
cmake --build build --config Release --target vn-bob-federated
```

### Linux#

```bash
cd build
make -j$(nproc)  # Parallel build
```

---

## Project Targets#

### Main Executables#

| Target | Source | Description |
|-------|--------|-------------|
| **VanNueman** | main.cpp | Main game executable |
| **vn-bob-agent** | scenarios/bob_agent_scenario.cpp | Bob agent scenario |
| **vn-bob-federated** | scenarios/bob_federated_scenario.cpp | Federated scenario (WHT) |
| **vn-benchmark** | (placeholder) | Performance testing |

### Static Libraries#

| Library | Sources | Description |
|---------|---------|-------------|
| **vncore** | Core engine code | Entity, math, utilities |
| **vnphysics** | physics/ | Skelly physics, deformation |
| **vnnetwork** | network/ | Server, client, federation |
| **vnneuroevolution** | neuroevolution/ | Neuroevolution system |
| **vnagents** | agents/ | Agent login, management |
| **vnscene** | scene/ | Scene management |
| **vnsimulation** | simulation/ | Tick loop |
| **vngameplay** | game_play/ | Gameplay mechanics |
| **vnaudio** | audio/ | WHT audio system |
| **vnrendering** | rendering/ | Opacity renderer |
| **vnentityneuro** | neuro/ | Entity neural network |
| **vnbio** | biology/ | Biological systems |

---

## Custom LLVM Toolchain#

### Purpose#

The project uses a **custom LLVM 17** build to:
- Compile **CUDA → SPIR-V** (`.cu` → `.spv`)
- Provide **vncc** (Vulkan compute compiler)
- Support custom SPIR-V translations

### Directory Structure#

```
llvm-project-release-17.x/
├── clang/              # C++ frontend
├── llvm/                # Core LLVM
├── lld/                 # Linker
└── ...

llvm-toolchain/
├── backend/            # SPIR-V backend
├── frontend/           # CUDA frontend
├── include/            # Headers
├── kernels/            # Pre-built kernels
├── passes/             # Optimization passes
└── test/               # Tests

spirv-translator/
├── docs/               # SPIR-V documentation
├── include/            # Headers
├── install/            # Installed tools
├── lib/                # Libraries
├── test/               # Tests
└── tools/              # Translation tools
```

### Building LLVM 17#

```bash
cd C:/Projects/Van_Nueman_Engine/Van_Nueman_Toolchain/llvm-project-release-17.x

# Configure
cmake -B build -G "Visual Studio 17 2022" \
    -DCMAKE_INSTALL_PREFIX=C:/llvm17-install \
    -DLLVM_ENABLE_PROJECTS="clang;lld" \
    -DLLVM_TARGETS_TO_BUILD=X86

# Build (takes 1-2 hours)
cmake --build build --config Release

# Install
cmake --build build --config Release --target install
```

### Using vncc#

```bash
# Compile CUDA kernel to SPIR-V
vncc -emit-spv kernel.cu -o kernel.spv

# Compile to LLVM IR
vncc -emit-llvm kernel.cu -o kernel.bc
```

---

## SPIR-V Kernels#

### Kernel Files (.cu)#

Located in `kernels/` directory:

| File | Description |
|------|-------------|
| **voxel_svo.cu** | 8-LOD SVO, octohedron/cube/polyhedra math |
| **pillars_compute.cu** | 14-pillar controller (entity/server/federation) |
| **skelly_compute.cu** | Fractal skeleton physics |
| **render_image.cu** | SVO traversal + ray-march (1080p) |

### Example Kernel#

```cuda
// kernels/pillars_compute.cu
#include <vulkan/vulkan.h>

__constant__ float PILLAR_WEIGHTS[14][14];

void compute_pillars(float* pillar_states, uint32_t num_entities) {
    uint32_t idx = get_global_id(0);
    if (idx >= num_entities) return;
    
    float* my_pillars = &pillar_states[idx * 14];
    
    // Apply pillar interactions
    for (int i = 0; i < 14; i++) {
        float force = 0.0f;
        for (int j = 0; j < 14; j++) {
            force += my_pillars[j] * PILLAR_WEIGHTS[i][j];
        }
        my_pillars[i] += force;
    }
}
```

### Building Kernels#

```bash
# Compile all kernels
cd C:/Projects/Van_Nueman_Engine/kernels
for f in *.cu; do
    vncc -emit-spv "$f" -o "${f%.cu}.spv"
done
```

---

## Bindings#

### Vulkan Wrapper#

```python
# bindings/vulkan_wrapper.py
import ctypes
from pathlib import Path

class VulkanWrapper:
    def __init__(self):
        self.lib = ctypes.CDLL("vulkan-1.dll")
        
    def create_instance(self):
        # Call Vulkan API via ctypes
        pass
```

### Entity Manager#

```python
# bindings/entity_manager.py
class EntityManager:
    def __init__(self):
        self.entities = []
    
    def create_entity(self, pillar_vector: list):
        entity_id = len(self.entities)
        self.entities.append({
            "id": entity_id,
            "pillars": pillar_vector
        })
        return entity_id
```

---

## Database Integration#

### PostgreSQL Setup#

```bash
# Windows (using vcpkg)
vcpkg install libpq:x64-windows

# Linux
sudo apt install postgresql libpq-dev
```

### Schema#

```sql
-- See docs/architecture.md for full schema
CREATE TABLE world_snapshots (
    server_id INT,
    timestamp TIMESTAMP,
    checksum VARCHAR(64)
);

CREATE TABLE svo_chunks (
    server_id INT,
    x INT, y INT, z INT,
    level INT,
    voxel_data BYTEA
);
```

---

## Build Troubleshooting#

### Common Issues#

| Issue | Solution |
|-------|----------|
| `vcpkg not found` | Set `VCPKG_ROOT` environment variable |
| `Vulkan not found` | Install Vulkan SDK, set `VK_SDK_PATH` |
| `LLVM 17 not found` | Build from source, set `LLVM_HOME` |
| `Linker errors` | Check vcpkg toolchain file is used |
| `SPIR-V compile fails` | Verify `vncc` is in PATH |

### Build Log Locations#

```
build.log                    # Main build log
build_output.txt              # Build output
build_integration.txt         # Integration status
BUILD_ISSUES_LOG.md         # Known issues
```

### Verbose Build#

```bash
# Enable verbose output
cmake -B build -DCMAKE_VERBOSE_MAKEFILE=ON
cmake --build build --config Release --verbose
```

---

## IDE Integration#

### Visual Studio Code#

`.vscode/settings.json`:

```json
{
    "cmake.sourceDirectory": "${workspaceFolder}",
    "cmake.buildDirectory": "${workspaceFolder}/build",
    "cmake.configureArgs": [
        "-DCMAKE_BUILD_TYPE=Release"
    ],
    "C_Cpp.default.includePath": [
        "${workspaceFolder}/include",
        "${workspaceFolder}/kernels",
        "C:/vcpkg/installed/x64-windows/include"
    ]
}
```

### Visual Studio 2019+#

- Open `build/VanNueman.sln`
- Select configuration (Release/Debug)
- Build → Build Solution

---

## Testing#

### Run Tests#

```bash
cd build
ctest --output-on-failure  # CMake tests

# Or run specific executable
./VanNueman.exe --test
./vn-bob-agent.exe
```

### Test Structure#

```
tests/
├── unit/              # Unit tests
├── integration/       # Integration tests
└── scenarios/         # Scenario tests
```

---

## Continuous Integration#

### GitHub Actions#

See `.github/workflows/` for CI configuration.

### Build Matrix#

| Platform | Compiler | Build Type |
|----------|-----------|-----------|
| Windows 10 | MSVC 19.x | Release |
| Windows 10 | MSVC 19.x | Debug |
| Ubuntu 22.04 | GCC 11 | Release |

---

## Performance Optimization#

### Compiler Flags#

```cmake
# Release mode flags (already set)
set(CMAKE_CXX_FLAGS_RELEASE "/O2 /Oy /GL")  # MSVC
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -flto")       # GCC/Clang
```

### Link-Time Optimization#

```bash
# Enable LTO
cmake -B build -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON
```

---

## Summary#

✅ **CMake 3.20+** - Build configuration  
✅ **vcpkg** - Dependency management  
✅ **LLVM 17** - Custom SPIR-V toolchain  
✅ **Vulkan SDK** - Graphics/compute  
✅ **PostgreSQL** - Persistent storage  
✅ **Multiple Targets** - Main game + scenarios  
✅ **SPIR-V Kernels** - GPU compute (.cu files)  
✅ **Testing Support** - ctest integration  

**Build command reference:**
```bash
git clone https://github.com/anomalyco/Van_Nueman.git
cd Van_Nueman
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
