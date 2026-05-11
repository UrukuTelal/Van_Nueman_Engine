# Physics System Documentation#

## Overview#

Van Nueman uses a **scale-invariant Skelly Physics** system that drives:
- **Entity scale**: Bones/muscles/organs/transports (humans, creatures)
- **Server scale**: Compute/processes/data_flow/network (computational)
- **Federation scale**: Servers/bandwidth/latency/registry (networks)
- **Celestial scale**: Plate tectonics/atmosphere/biomes/lithosphere

### Core Principle#

**Physics is universal** - the same Skelly system applies to ALL scales.

---

## Skelly Physics (Fractal Skeleton)#

### Structure#

```
Entity (any scale)
└── Skelly (fractal skeleton)
    ├── Bones (structural integrity → Resistance)
    ├── Muscles (volumetric expansion/contraction → Force)
    ├── Organs (energy output → Attraction)
    └── Transports (pressure flow → Influence, Harm)
        └── InterstitialTransport (pathfinding A→B)
```

### Scale Mapping#

| Scale | Bones | Muscles | Organs | Transports |
|-------|-------|----------|--------|------------|
| **Entity (human)** | Skeleton frame | Actuators | Biological organs | Blood/lymph |
| **Entity (drone)** | Frame/chassis | Motor actuators | Reactor/battery | Hydraulics |
| **Server** | Compute nodes | Process threads | Services | Data flow |
| **Federation** | Member servers | Bandwidth | Registry | Network links |
| **Celestial (rock)** | Inertia/lithosphere | (none) | (none) | InterstitialTransport |
| **Celestial (planet)** | Plate tectonics | Atmosphere | Biomes | Magma/lithosphere |

---

## Sparse Voxel Octree (SVO)#

### 8 LOD Levels#

```
Level 0: 256³ base resolution (deepest)
Level 1: 128³
Level 2: 64³
Level 3: 32³
Level 4: 16³
Level 5: 8³
Level 6: 4³
Level 7: 2³ (coarsest)
```

### Chunk Size#

- **64³ voxels** per chunk for persistence
- **Dirty chunk tracking**: Only rebuild changed chunks
- **Materials**: Octohedron/cube/polyhedra

### SVO Rebuilding (per tick)#

```
1. Mark dirty chunks (changed by Skelly physics)
2. Rebuild 8-LOD structure
3. Assign materials:
   - Octohedron (organic)
   - Cube (structural)
   - Polyhedra (complex)
4. Delta compress for network transmission
```

---

## Pillar Coupling (14 Forces)#

### Physics Mapping#

| Pillar | Physics Mapping | Skelly Component |
|--------|---------------|-------------------|
| **Force (2)** | Muscle expansion/contraction | Muscles |
| **Influence (3)** | Pressure flow, transport | Transports |
| **Resistance (4)** | Structural integrity | Bones |
| **Integrity (5)** | Value consistency | Policy alignment |
| **Cohesion (6)** | Binding strength | Family/nation/ideology |
| **Attraction (11)** | Energy output, draw | Organs |
| **Harm (12)** | Destructive force | Damage |
| **Distortion (13)** | Noise, deformation | State unreliability |

### Force Application#

```c++
// Apply Force from Pillar State Vector
void apply_pillar_forces(Entity* entity) {
    float force = entity->pillars[PILLAR_FORCE];
    float attraction = entity->pillars[PILLAR_ATTRACTION];
    float resistance = entity->pillars[PILLAR_RESISTANCE];
    
    // Net force = raw - (resistance × interaction)
    float net_force = force * attraction - (resistance * get_interaction_matrix());
    
    // Apply to Skelly muscles
    for (auto& muscle : entity->skelly.muscles) {
        muscle.expand(net_force);
    }
}
```

---

## Deformation (Skelly → SVO)#

### Process#

```
1. Skelly Physics Update (30 Hz)
   ├── Bone: Check structural integrity (Resistance)
   ├── Muscle: Apply expansion/contraction (Force)
   ├── Organ: Output energy (Attraction)
   └── Transport: Pressure flow (Influence, Harm)

2. Deformation Detection
   ├── Muscle expansion → push voxels
   ├── Segment fracture → remove voxels
   └── Organ volume → structural deformation

3. SVO Mutation
   ├── Mark affected chunks as dirty
   ├── Rebuild octree (8 LODs)
   └── Assign materials

4. Render Packet (to clients)
   ├── Delta compress changed SVO nodes
   └── Broadcast via UDP (30 Hz, 128 clients)
```

### Fracture (Segment Breaking)#

```c++
void check_fracture(SkellySegment* segment, float harm) {
    float resistance = segment->entity->pillars[PILLAR_RESISTANCE];
    
    if (harm > resistance * 0.9f) {  // Near breaking point
        segment->fracture();  // Break connection
        mark_chunk_dirty(segment->chunk);
    }
}
```

---

## InterstitialTransport (Pathfinding)#

### Usage#

Primarily used by **celestial objects** (rocks, planets) for pathfinding through space.

```c++
// Skelly physics for celestial bodies
void InterstitialTransport::pathfind(Vector3 A, Vector3 B) {
    // Pressure-driven drift (like blood through interstitial spaces)
    Vector3 gradient = (B - A).normalized();
    float pressure = calculate_pressure(A, B);
    
    // Apply to entity's Force pillar
    entity->pillars[PILLAR_FORCE] += pressure * gradient.length();
}
```

### Physics Analogy#

| Biological | Celestial | Skelly Component |
|------------|-----------|-------------------|
| Blood flow | Orbital drift | Transports |
| Capillary pressure | Gravitational gradient | Pressure flow |
| Tissue | Space vacuum | Interstitial space |
| Oxygen delivery | Energy transfer | Organ output |

---

## Tick Cycle (30 Hz)#

### Complete Physics Tick#

```
┌─────────────────────────────────────────────────┐
│ TICK n (33.33ms)                                │
│                                             │
│ 1. PILLAR UPDATE (14 forces per entity)       │
│    - Awareness: Sample world state              │
│    - Evaluate: Check Pillar constraints       │
│    - Decide: Apply Force (Willpower+Attr)     │
│    - Memory: Record state transition         │
│                                             │
│ 2. SKELLY PHYSICS (fractal)                 │
│    - Bones: Structural integrity (Resistance)  │
│    - Muscles: Volumetric expansion (Force)   │
│    - Organs: Energy output (Attraction)      │
│    - Transports: Pressure flow (Influence)   │
│    - Interstitial: Pathfinding A→B          │
│                                             │
│ 3. DEFORMATION (Skelly → SVO)              │
│    - Muscle expansion → voxel push          │
│    - Segment fracture → voxel removal       │
│    - Organ volume → structural deform      │
│                                             │
│ 4. SVO REBUILD (dirty chunks)              │
│    - Mark dirty chunks                     │
│    - Rebuild 8-LOD structure              │
│    - Assign materials (octo/cube/poly)     │
│                                             │
│ 5. RENDER PACKET (128 clients)             │
│    - Delta compress changed SVO nodes      │
│    - Broadcast via UDP (30 Hz)            │
│                                             │
│ 6. DATABASE SYNC (async)                     │
│    - Persist changed chunks (64³)          │
│    - Update world snapshot                │
└─────────────────────────────────────────────────┘
```

---

## Logarithmic Feedback Loop#

### Server-to-Server Force Propagation#

```
Server A applies Force to Server B:
   raw_force = A.Force × A.Attraction × (1 / log(distance + 1))
```

### Hop Decay#

```
For each hop:
    Decay = base_signal / log(hop_count + 1)
    
    If Decay > threshold:
        Apply to target Pillar State Vector
        Propagate to next hop
    Else:
        Stop propagation
```

### Pillar Interaction Resolution#

```c++
net_force = raw_force - (B.Resistance × interaction_matrix[A.pillar][B.pillar]);
B.pillar_state[target] += net_force;

// Neuroevolution evaluates outcome → adapts weights
neuroevolution.evaluate(A, B, outcome);
```

---

## Neuroevolution (Dynamic Rules Learning)#

### Components#

```
neuroevolution/
├── population/          # Server PSV population
├── fitness/           # Fitness evaluation
├── operators/          # Mutation/crossover operators
└── boundaries/         # Operator-set constraints
```

### Fitness Evaluation (0.1 Hz)#

```python
# neuroevolution/fitness.py
def evaluate_fitness(server_a, server_b, outcome):
    """Evaluate outcome of pillar interaction."""
    fitness_score = 0.0
    
    # Success = positive fitness
    if outcome == "success":
        fitness_score += 1.0
    elif outcome == "harm":
        fitness_score -= 0.5 * server_a.pillars[PILLAR_HARM]
    
    # Adapt interaction weights
    adjust_weights(server_a.id, server_b.id, fitness_score)
    return fitness_score
```

### Operator-Set Boundaries#

```sql
-- Operators set min/max values for pillars
CREATE TABLE operator_boundaries (
    server_id INT,
    pillar INT,
    min_val FLOAT,
    max_val FLOAT
);
```

Neuroevolution learns weights **within** these boundaries.

---

## Fractal Skelly System#

### Implementation#

```c++
// physics/fractal_skelly.h
class FractalSkelly {
public:
    struct Bone {
        float integrity;  // Maps to Resistance pillar
        Vector3 position;
    };
    
    struct Muscle {
        float expansion;  // Maps to Force pillar
        float contraction;
    };
    
    struct Organ {
        float energy_output;  // Maps to Attraction pillar
        float volume;
    };
    
    struct Transport {
        float pressure;  // Maps to Influence/Harm pillars
        Vector3 flow_direction;
    };
    
    std::vector<Bone> bones;
    std::vector<Muscle> muscles;
    std::vector<Organ> organs;
    std::vector<Transport> transports;
};
```

### Scale-Invariant Methods#

```c++
void update_skelton(FractalSkelly* skelly, float pillars[14]) {
    // Bones: Check structural integrity
    for (auto& bone : skelly->bones) {
        float resistance = pillars[PILLAR_RESISTANCE];
        if (bone.integrity > resistance) {
            bone.integrity -= 0.01f;  // Degrade
        }
    }
    
    // Muscles: Apply expansion/contraction
    float force = pillars[PILLAR_FORCE];
    for (auto& muscle : skelly->muscles) {
        muscle.expansion = force * 0.5f;
    }
    
    // Organs: Output energy
    float attraction = pillars[PILLAR_ATTRACTION];
    for (auto& organ : skelly->organs) {
        organ.energy_output = attraction * organ.volume;
    }
    
    // Transports: Pressure flow
    float influence = pillars[PILLAR_INFLUENCE];
    for (auto& transport : skelly->transports) {
        transport.pressure = influence * 0.3f;
    }
}
```

---

## Database Persistence#

### Chunk Storage (64³)#

```sql
CREATE TABLE svo_chunks (
    server_id INT,
    x INT,
    y INT,
    z INT,
    level INT,  -- LOD level (0-7)
    voxel_data BYTEA,  -- Serialized voxel data
    last_modified TIMESTAMP
);

-- World snapshots
CREATE TABLE world_snapshots (
    server_id INT,
    timestamp TIMESTAMP,
    checksum VARCHAR(64)
);
```

### Async Writes (1 Hz)#

```c++
// database/persistence.cpp
void async_persist_changes() {
    // Called at 1 Hz
    for (auto& chunk : dirty_chunks) {
        if (chunk.is_modified()) {
            save_chunk_to_db(chunk);
            chunk.mark_clean();
        }
    }
}
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

## Configuration#

### Physics Settings#

| Setting | Value | Description |
|---------|-------|-------------|
| Tick rate | 30 Hz | Physics + pillar update |
| SVO chunk | 64³ | Persistence unit |
| LOD levels | 8 | 256³ base → 1-voxel |
| Entity capacity | 500K | Per server |
| Feedback tick | 1 Hz | Log-decay propagation |
| Learning tick | 0.1 Hz | Neuroevolution |

---

## Summary#

✅ **Skelly Physics** - Scale-invariant fractal skeleton  
✅ **SVO Rendering** - 8 LOD sparse voxel octree  
✅ **Pillar Coupling** - 14 forces → physics mapping  
✅ **Deformation** - Skelly → SVO mutation  
✅ **InterstitialTransport** - Pathfinding (celestial)  
✅ **Logarithmic Feedback** - Server-to-server force  
✅ **Neuroevolution** - Dynamic weight learning  
✅ **PostgreSQL** - Persistent chunk storage  

**Physics is universal** - Same system drives rocks, humans, servers, and federations.
