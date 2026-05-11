# C++/Python Language Boundaries in Van Nueman Monorepo

## Overview
This document defines the clear boundaries between C++ (performance-critical) and Python (higher-level logic) components in the Van Nueman monorepo, as approved by the Pillar Council.

## Language Allocation

### C++17/CUDA (Performance-Critical Systems)
- **Van_Nueman_Engine**: Core game engine with Vulkan rendering, physics, entity systems
- **Van_Nueman_Toolchain**: Custom LLVM compiler (vncc.exe for SPIR-V compilation)  
- **Van_Nueman_Whisper**: Speech-to-text processing (standalone version)
- **Kernel compute shaders**: CUDA → SPIR-V pipeline via vncc

### Python 3.10+ (Higher-Level Logic)
- **Van_Nueman_PillarAI**: 16-Pillar AI configuration and reasoning system
- **Van_Nueman_Services**: FastAPI service layer
- **Van_Nueman_Social_Sim**: CrowNest multi-agent simulation lab
- **Tooling, scripts, automation**: Build systems, CI/CD, administrative tasks

## Interaction Points

### 1. Engine ←→ PillarAI (Primary Interface)
- **Protocol**: REST API over HTTP/JSON
- **Endpoint**: `http://localhost:8888/api/*` (Services layer)
- **Data Exchanged**: 
  - Pillar State Vectors (PSV): 16-dimensional float arrays
  - Task requests and results
  - Configuration updates
- **Performance Target**: <10ms latency for critical paths

### 2. Services ←→ PillarAI (Secondary Interface)  
- **Protocol**: Direct Python module imports (when co-located)
- **Fallback**: REST API for distributed deployments
- **Data Exchanged**:
  - Pillar configuration objects
  - Workflow task definitions
  - Simulation parameters

### 3. Engine ←→ Services (Indirect via PillarAI)
- **Protocol**: Engine → Services (REST) → PillarAI (Python/REST)
- **Use Case**: Engine-triggered AI reasoning requests
- **Data Flow**: Sensor data → PSV analysis → Action commands

### 4. Toolchain ←→ Engine (Build-Time Interface)
- **Protocol**: File system + CMake integration
- **Data Exchanged**:
  - Kernel source (.cpp) → SPIR-V (.spv) 
  - Build artifacts and dependencies
- **Timing**: Compile-time only, no runtime interaction

## Data Contracts

### Pillar State Vector (PSV) Format
```cpp
// C++ representation (engine-side)
struct PillarStateVector {
    float pillars[16];  // [Awareness, Willpower, Force, Influence, Resistance, Integrity, Cohesion, Relation, Presence, Warmth, Memory, Attraction, Harm, Distortion, Flux, Depth]
};

// Python representation (services/pillarai-side)
class PillarStateVector:
    def __init__(self):
        self.pillars = [0.0] * 16  # Same order as C++
    
    def to_list(self) -> List[float]:
        return self.pillars.copy()
        
    def from_list(self, values: List[float]):
        assert len(values) == 16
        self.pillars = values.copy()
```

### API Endpoints (Services Layer)

#### POST /api/pillar/analyze
- **Input**: `{"psv": [16 floats], "context": "optional string"}`
- **Output**: `{"recommendations": [{"pillar": int, "delta": float, "confidence": float}]}`
- **Purpose**: Get AI-driven pillar adjustment recommendations

#### POST /api/workflow/execute
- **Input**: `{"task_description": string, "params": dict}`
- **Output**: `{"workflow_id": string, "status": "processing"}`
- **Purpose**: Initiate multi-agent workflow execution

#### GET /api/workflow/{id}/status
- **Output**: `{"status": "completed|failed|processing", "results": [...]}`
- **Purpose**: Poll for workflow completion

## Build and Deployment

### C++ Build Requirements
- C++17 compiler (MSVC on Windows, GCC/Clang on Linux)
- Vulkan SDK
- vcpkg dependencies (GLFW3, ImGui, etc.)
- Custom LLVM toolchain (vncc.exe) for shader compilation

### Python Environment Requirements  
- Python 3.10-3.12 (3.13+ incompatible with some dependencies)
- Core packages: fastapi, uvicorn, flask, torch, ollama
- Service-specific: See individual requirements.txt files
- Virtual environments recommended per service

## Performance Guidelines

### Cross-Language Call Overhead
- Target: <1ms for local Python→C++ calls via bindings
- Target: <10ms for REST-based service communication
- Monitor: Add timing middleware to all API endpoints

### Data Transfer Optimization
- Use binary serialization (ProtoBuf) for high-volume data
- Minimize cross-boundary data copying
- PSV updates: Prefer delta compression over full state transfer

## Error Handling

### Cross-Boundary Exceptions
- C++ exceptions → Python: Convert to appropriate Python exceptions via bindings
- Python exceptions → C++: Return error codes, log via engine logging system
- API failures: Return HTTP 5xx with JSON error details
- Circuit breaker pattern: Prevent cascade failures between layers

## Monitoring and Observability

### Metrics to Collect
- API request/response latency (by endpoint)
- PSV update frequency and volume
- Cross-boundary call failure rates
- Resource utilization (CPU, memory, GPU) per language boundary

### Logging Standards
- Structured JSON logging for machine parsing
- Correlation IDs for cross-request tracing
- Language-tagged log entries (CPP/PY) for filtering

## Future Evolution Paths

### Potential Optimizations
1. **Shared Memory**: For ultra-low latency PSV updates
2. **gRPC/ProtoBuf**: Replace REST for service-to-service calls
3. **Cython/PyBind11**: Generate optimized bindings for hot paths
4. **Message Queues**: Async communication for non-critical paths

### Migration Guidelines
- New performance-critical features → C++
- New AI/service features → Python  
- Prototyping: Python first, migrate to C++ if performance demands
- Deprecation: Mark boundaries for review quarterly

---
*This document captures the approved architecture as of the Pillar Council review. Changes to language allocations or interaction patterns require new Council submission and approval.*