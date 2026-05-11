# Van Nueman - Complete Architecture Documentation

## System Overview#

Van Nueman is a **scale-invariant game engine** that unifies:
- **16-dimensional Pillar State Vector (PSV)** for entity representation
- **Skelly Physics** (fractal skeleton system) for scale-invariant physics
- **Sparse Voxel Octree (SVO)** with 8 LOD levels for rendering
- **Vulkan/SPIR-V** compute pipeline with custom LLVM toolchain
- **Pillar AI (CrowNest)** multi-agent system with WHT Protocol

---

## Core Realization: Everything is a Pillar State Vector#

### Fundamental Principle#
Every entity — from rock to server to federation — is defined by a **16-dimensional Pillar State Vector**:

```c++
struct PillarStateVector {
    float pillars[16];  // Awareness, Willpower, Force, Influence, ...
};
```

### Scale Invariance#
The SAME 16 pillars apply to ALL scales:

| Scale | Entity Type | Example Pillars |
|-------|-------------|-------------------|
| **Celestial** | Rock, Planet | Awareness=0.0, Force=0.0-0.3, Attraction=0.5-1.0 |
| **Living** | Planet (World-Serpent) | Awareness=0.1-0.3, Warmth=0.4-0.7 |
| **Automated** | Drone | Awareness=0.4-0.6, Force=0.4-0.7 |
| **Industrial** | Robot | Awareness=0.6-0.8, Force=0.6-0.9 |
| **Biological** | Creature, Human | Awareness=0.3-0.9, Warmth=0.5-0.9 |
| **Computational** | Server | Awareness=0.8-1.0, Force=0.6-0.9 |
| **Network** | Federation | Awareness=0.9-1.0, Cohesion=0.7-0.9 |

---

## Warmth = Energy Transfer (Universal Definition)#

**Light IS warmth physically**. The transfer of energy in ANY form = Warmth:

| Energy Type | Examples | Pillar Mapping |
|------------|----------|---------------|
| Caloric | Food consumption | Warmth=0.4-0.7 |
| Thermal | Shelter heat retention | Warmth=0.0-0.3 (inanimate) |
| Electromagnetic | Light, radiation | Warmth=0.0-0.3 (celestial) |
| Emotional | Care, empathy | Warmth=0.5-0.9 (human) |
| Potential | Resources, batteries | Warmth=0.0-0.2 (drones) |
| Risk-free | Safety expenditure | Warmth=0.4-0.7 |

All energy transfer between entities IS warmth — this unifies the Warmth pillar across all scales.

---

## Scaled Integers#

While documentation examples use floats, **all game simulation logic uses scaled integers** with bitwise logic where:
- `1 = 2^20` (1,048,576)
- Use fixed-point arithmetic for performance
- Avoid floating-point in core simulation

```c++
// Float version (documentation)
float force = 0.5f;

// Scaled integer version (actual simulation)
int32_t force_scaled = (int32_t)(0.5f * (1 << 20));  // = 524,288
```

---

## Entity Pillar Vector Examples#

### Type 1: Celestial Body (Rock/Asteroid/Planet)#

```c++
PillarStateVector rock = {
    .awareness = 0.0,      // No perception
    .willpower = 0.0,      // No intent
    .force = 0.1,          // Passive gravity
    .influence = 0.3,      // Gravity well
    .resistance = 0.9,     // Inertia, structural integrity
    .integrity = 1.0,       // Geological stability
    .cohesion = 0.7,       // Gravitational binding
    .relation = 0.1,        // No networking
    .presence = 0.5,        // Gravity visibility
    .warmth = 0.2,         // Thermal radiation
    .memory = 0.05,        // Geological record
    .attraction = 0.8,      // Strong gravitational pull
    .harm = 0.2,           // Low harm potential
    .distortion = 0.1        // Minimal field anomalies
    .flux = 0.05,      // Low system volatility
    .depth = 0.1,        // Simple geological structure
};
Physics: Skelly InterstitialTransport (pressure-driven drift through space)
```

### Type 2: Living Planet (World-Serpent)#

```c++
PillarStateVector planet = {
    .awareness = 0.2,      // Primordial sensing
    .willpower = 0.2,      // Primitive direction
    .force = 0.4,         // Tectonic activity
    .influence = 0.5,      // Ecological influence
    .resistance = 0.7,     // Adaptive resilience
    .integrity = 0.6,       // Dynamic ecosystems
    .cohesion = 0.7,       // Life binding
    .relation = 0.2,        // Mycorrhizal networks
    .presence = 0.6,        // Geographic prominence
    .warmth = 0.5,         // Biosphere energy transfer
    .memory = 0.4,         // Evolutionary record
    .attraction = 0.6,      // Habitability draw
    .harm = 0.3,           // Natural selection pressure
    .distortion = 0.2        // Mutation, ecological drift
    .flux = 0.15,      // Moderate system volatility
    .depth = 0.4,        // Complex ecological layers
};
Physics: Full Skelly (bones=plate tectonics, muscles=atmosphere, organs=biomes)
```
```

---

## "Hidden Factors" in Celestial Objects#

The **Distortion** and **Memory** pillars explain observed anomalies:

| Observation | Pillar Explanation |
|-------------|---------------------|
| Rogue planets drifting without cause | Memory=low, Distortion=medium (chaotic trajectory retention) |
| Pulsars maintaining rhythm | Presence=1.0, Harm=0.3 (rhythmic energy output) |
| Fast Radio Bursts | Distortion=high, Influence=low (fragmented information) |
| Rogue stars vs galactic rotation | Distortion=high, Resistance=low (distortion erodes orbit) |
| Magnetars with extreme fields | Attraction=high, Harm=high (directed force) |
| Black hole variations | Distortion=variable (different singularity states) |

---

## Scale-Invariant Physics Model#

```
┌─────────────────────────────────────────────────┐
│ EVERY ENTITY: Pillar State Vector + Skelly Physics            │
│                                                                 │
│ Pillar Vector → drives → Skelly System → produces → Physics     │
│                                                                 │
│ Entity Scale:                                                   │
│   Pillar Vector → Bones + Muscles + Organs + Transports        │
│                                                                 │
│ Server Scale:                                                   │
│   Pillar Vector → Compute + Processes + DataFlow + Network       │
│                                                                 │
│ Federation Scale:                                               │
│   Pillar Vector → Servers + Bandwidth + Latency + Registry     │
│                                                                 │
│ Physics is universal:                                           │
│   • Force application (via Pillar Force)                        │
│   • Pressure flow (via InterstitialTransport)                  │
│   • Deformation (via Skelly muscles/turgor)                    │
│   • Fracture (via Skelly segment breaking)                    │
│   • Pathfinding (via pressure gradient A→B)                    │
└─────────────────────────────────────────────────┘
```

---

## Complete Tick Cycle (30 Hz)#

```
┌─────────────────────────────────────────────────┐
│ TICK n (33.33ms)                                                │
│                                                                 │
│ 1. NETWORK INPUT                                                │
│    - Receive client inputs                                      │
│    - Receive federated server feedback (if applicable)           │
│                                                                 │
│ 2. PILLAR UPDATE (16 forces per entity, per server, per federation)│
│    - Awareness: Sample world state                              │
│    - Evaluate: Check Pillar constraints                         │
│    - Decide: Apply Force based on Willpower + Attraction       │
│    - Memory: Record state transition                           │
│                                                                 │
│ 3. SKELLY PHYSICS (fractal, per entity)                         │
│    - Bones: Structural integrity (Resistance)                  │
│    - Muscles: Volumetric expansion/contraction (Force)         │
│    - Organs: Energy output (Attraction)                        │
│    - Transports: Pressure flow (Influence, Harm)               │
│    - Hydraulics: Turgor, deformation, fracture                 │
│                                                                 │
│ 4. DEFORMATION (Skelly → SVO)                                   │
│    - Muscle expansion → voxel push                            │
│    - Segment fracture → voxel removal                         │
│    - Organ volume → structural deformation                     │
│                                                                 │
│ 5. SVO REBUILD (changed chunks only)                            │
│    - Mark dirty chunks                                         │
│    - Rebuild 8-LOD structure                                  │
│    - Assign materials (octohedron/cube/polyhedra)             │
│                                                                 │
│ 6. RENDER PACKET (for clients)                                 │
│    - Delta compress changed SVO nodes                          │
│    - Broadcast to 128 clients (30 Hz UDP)                     │
│                                                                 │
│ 7. DATABASE SYNC (async)                                       │
│    - Persist changed chunks (64³)                             │
│    - Update world snapshot                                     │
└─────────────────────────────────────────────────┘
```

### Background Tasks#

| Frequency | Task |
|-----------|------|
| **1 Hz** | Feedback loop propagation (unlimited hops, log decay) |
| **1 Hz** | Cross-server sync |
| **1 Hz** | Database async writes |
| **0.1 Hz** | Neuroevolution evaluation |
| **0.1 Hz** | Fitness evaluation |
| **0.1 Hz** | Policy adaptation |

---

## Logarithmic Feedback Loop#

Server A applies Force to Server B:

```
raw_force = A.Force × A.Attraction × (1 / log(distance + 1))
```

For each hop:
```
Decay = base_signal / log(hop_count + 1)
If Decay > threshold:
    Apply to target Pillar State Vector
    Propagate to next hop
Else:
    Stop propagation
```

### Pillar Interaction Resolution:#
```
net_force = raw_force - (B.Resistance × interaction_matrix[A.pillar][B.pillar])
B.pillar_state[target] += net_force
Neuroevolution evaluates outcome → adapts interaction weights
```

---

## Force Types (6 Categories)#

| Force Type | Examples | Pillar Mapping |
|------------|----------|---------------|
| **Economic** | Trade, resources, embargoes | Attraction, Resistance |
| **Military** | Raids, defense | Force, Harm |
| **Social** | Migration, culture | Cohesion, Warmth |
| **Diplomatic** | Alliances, treaties | Integrity, Relation |
| **Cultural** | Influence, memes | Influence, Distortion |
| **Information** | State, data flow | Memory, Awareness |

---

## Configuration Summary#

| Setting | Value |
|---------|-------|
| Players per server | 128 |
| SVO chunk size | 64³ voxels |
| SVO LOD levels | 8 (256³ base → 1-voxel deepest) |
| Entity capacity | 500K per server |
| Rendering resolution | 1080p |
| Physics tick | 30 Hz |
| Feedback tick | 1 Hz |
| Learning tick | 0.1 Hz |
| Transport | UDP |
| Database | PostgreSQL |
| Federation hops | Unlimited (log decay) |
| Tutorial | Single linear path (narrative) |
| Force types | All 6 (Economic + Military + Social + Diplomatic + Cultural + Information) |
| Pillar interaction | Hybrid (operators set boundaries, neuroevolution learns weights) |

---

## Directory Structure#

```
Van_Nueman/
├── kernels/                    # CUDA → SPIR-V 
│   ├── voxel_svo.cu           # 8-LOD SVO, octohedron/cube/polyhedra math
│   ├── pillars_compute.cu    # 16-pillar controller (entity + server + federation)
│   ├── skelly_compute.cu     # Fractal skeleton physics (entity/server/federation)
│   │                           # Rocks use InterstitialTransport for A→B pathfinding
│   └── render_image.cu       # SVO traversal + ray-march (1080p)
├── bindings/
│   ├── vulkan_wrapper      # ctypes Vulkan + SPIR-V loader
│   └── entity_manager
├── physics/
│   ├── pillar_coupling    # Pillars → Skelly → Deformation
│   ├── deformation        # Skelly physics → SVO mutation
│   └── fractal_skelly     # Scale-invariant Skelly system
├── network/
│   ├── client              # UDP client, 1080p rendering
│   ├── server             # Authoritative, 30 Hz tick
│   ├── protocol           # Binary protocol
│   ├── federated          # Server↔Server communication (unlimited hops)
│   └── feedback_loops     # Pillar-driven log-decay feedback
├── neuroevolution/
│   ├── population          # Server PSV population
│   ├── fitness           # Fitness evaluation
│   ├── operators          # Mutation/crossover operators
│   └── boundaries         # Operator-set constraints
├── database/
│   ├── persistence         # PostgreSQL SVO saves (64³ chunks)
│   ├── world_state        # World snapshots
│   └── schema.sql            # Database schema
├── single_player/
│   └── tutorial           # Linear narrative, ephemeral completion toggle
├── server_registry/
│   └── approval_system    # Manual content approval
├── apis/
│   ├── pillars_api.cu        # 16-pillar API (fractal)
│   └── skelly_api.cu        # Skeleton physics API (fractal)
└── CMakeLists.txt
```

---

## PostgreSQL Schema (High-Level)#

```sql
-- Players
CREATE TABLE players (
    id SERIAL PRIMARY KEY,
    username VARCHAR(255),
    server_id INT,
    created_at TIMESTAMP DEFAULT NOW()
);

-- World persistence (64³ chunk hybrid)
CREATE TABLE world_snapshots (
    server_id INT,
    timestamp TIMESTAMP,
    checksum VARCHAR(64)
);

CREATE TABLE svo_chunks (
    server_id INT,
    x INT,
    y INT,
    z INT,
    level INT,
    voxel_data BYTEA,
    last_modified TIMESTAMP
);

-- Server registry
CREATE TABLE servers (
    id SERIAL PRIMARY KEY,
    owner VARCHAR(255),
    status VARCHAR(50),
    pillar_state FLOAT[16],
    approved BOOLEAN DEFAULT FALSE,
    approved_by INT
);

CREATE TABLE server_relationships (
    server_a INT,
    server_b INT,
    connection_type VARCHAR(50),
    bandwidth FLOAT,
    latency FLOAT
);

-- Federation feedback
CREATE TABLE feedback_logs (
    source INT,
    target INT,
    force_type VARCHAR(50),
    magnitude FLOAT,
    pillar INT,
    timestamp TIMESTAMP
);

CREATE TABLE interaction_history (
    server_a INT,
    server_b INT,
    outcome VARCHAR(255),
    fitness_score FLOAT
);

-- Neuroevolution
CREATE TABLE evolution_weights (
    server_id INT,
    pillar_pair VARCHAR(10),
    weight FLOAT,
    fitness FLOAT,
    generation INT
);

CREATE TABLE operator_boundaries (
    server_id INT,
    pillar INT,
    min_val FLOAT,
    max_val FLOAT
);

-- Single player
CREATE TABLE tutorial_progress (
    player_id INT,
    step INT,
    completed BOOLEAN DEFAULT FALSE,
    completed_at TIMESTAMP
);
```

---

## Implementation Phases#

| Phase | Components | Description |
|-------|-----------|-------------|
| **1** | kernels/ + bindings/ | SPIR-V rendering pipeline (SVO + ray-march) |
| **2** | apis/ | Port pillars_api.py + skelly_api.py to CUDA |
| **3** | physics/ | Pillar coupling + deformation + fractal skelly |
| **4** | database/ | PostgreSQL schema + persistence |
| **5** | network/server.py | Authoritative server (30 Hz tick) |
| **6** | network/client.py | Dumb terminal client |
| **7** | network/feedback_loops.py | Log-decay federation feedback |
| **8** | neuroevolution/ | Dynamic rules learning |
| **9** | network/federated.py | Cross-server communication |
| **10** | single_player/ | Tutorial system |
| **11** | server_registry/ | Manual approval |

---

## Technology Stack#

### Core Engine#
- **Language**: C++17
- **Graphics**: Vulkan 1.3
- **Compute**: SPIR-V (custom LLVM toolchain)
- **Build**: CMake 3.20+, vcpkg

### Custom LLVM Toolchain#
- **llvm-project-release-17.x**: Custom build for SPIR-V
- **spirv-translator**: SPIR-V translation tools
- **vncc**: Custom compiler (Vulkan compute)

### Database#
- **PostgreSQL**: Persistent world storage
- **Schema**: World snapshots, SVO chunks, federation state

### AI System#
- **Pillar AI**: CrowNest multi-agent environment
- **WHT Protocol**: Walsh-Hadamard Transform (32D signals)
- **Ollama**: Local LLM models (qwen, mistral, etc.)
- **Python 3.8+**: AI scripts, task delegation

### Networking#
- **Protocol**: Binary UDP (30 Hz)
- **Federation**: Unlimited hops with log decay
- **Feedback**: Pillar-driven propagation

---

## Plan Status: READY FOR IMPLEMENTATION#

All specifications captured. Ready to begin when you give the go-ahead.
