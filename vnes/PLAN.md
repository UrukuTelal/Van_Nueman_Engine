# VNES — Van Neumann Ecosystem Substrate

## Implementation Plan

**Version:** 0.1  
**Status:** Planning  
**Dependencies:** Van_Nueman_Engine (voxel/SVO, scale/math, mesh pipeline), Van_Nueman_Services (API, placement), Van_Nueman_AI (council approval for design changes)

---

## Overview

The VNES specification defines a modular construction framework around a single universal primitive: the **Regular Truncated Octahedron (Kelvin Cell)** , 250 mm circumscribed diameter, with 5 functional variants (VNE-A through VNE-E). This plan covers the full implementation pipeline:

1. Procedural geometry generator (SDF → CSG → Marching Cubes → STL/PLY)
2. C++ variant header definitions with compiled JSON metadata
3. Deterministic placement engine (genotype → lattice)
4. SVO/voxel system integration (simulation physics)
5. Standalone Python `vnes-generator` package
6. Specification document & README

---

## Track 1 — Procedural Geometry Generator

**Goal:** A C++ library that reads the VNES JSON spec and outputs watertight STL/PLY/STEP meshes.

### Architecture

```
VNES JSON Spec
     ↓
Metadata Compiler (compile-time or load-time)
     ↓
Signed Distance Field (SDF) generator per variant
     ↓
CSG operations (union/difference/intersect with Kelvin cell hull)
     ↓
Marching Cubes (or Dual Contouring) → triangle mesh
     ↓
Mesh cleanup → manifold fix → STL/PLY export
```

### Implementation Stages

| Stage | Description | Est. Effort |
|-------|-------------|-------------|
| 1.1 | SDF primitive library — sphere, plane, box, gyroid, Schwarz P, cylinder, torus | 3–5 days |
| 1.2 | Kelvin cell hull SDF — truncated octahedron with shell thickness, snap-fit pegs/sockets | 3–4 days |
| 1.3 | Variant A — Schwarz P minimal surface within Kelvin hull | 2 days |
| 1.4 | Variant B — density-gradient gyroid with growth face and open chambers | 3–4 days |
| 1.5 | Variant C — open-cell gyroid (3–6 mm pores) with hydraulic constraints | 2 days |
| 1.6 | Variant D — 3 vascular trunks (20 mm), Steinmetz junction, gyroid lattice | 3 days |
| 1.7 | Variant E — utility corridors, sensor mounts, inspection ports | 2–3 days |
| 1.8 | Marching Cubes integration (grid resolution, iso-level, chunking) | 2 days |
| 1.9 | Mesh cleanup — manifold fix, self-intersection removal, hole-fill | 2 days |
| 1.10 | STL/PLY/STEP export (binary + ASCII) | 1 day |

**Total:** ~22–28 days

### Files

| File | Role |
|------|------|
| `vnes/sdf_primitives.h` | SDF primitives (gyroid, Schwarz P, torus, cylinder, etc.) |
| `vnes/kelvin_hull.h` | Kelvin cell hull with snap-fit interlocking system |
| `vnes/variant_a.h` | Variant A SDF generator |
| `vnes/variant_b.h` | Variant B SDF generator (gradient gyroid + growth openings) |
| `vnes/variant_c.h` | Variant C SDF generator (open-cell gyroid) |
| `vnes/variant_d.h` | Variant D SDF generator (vascular trunks + Steinmetz) |
| `vnes/variant_e.h` | Variant E SDF generator (utility corridors) |
| `vnes/marching_cubes.h` | Grid-based isosurface extraction |
| `vnes/mesh_cleanup.h` | Manifold repair, hole-fill, de-duplication |
| `vnes/export.h` | STL, PLY, STEP writers |
| `vnes/vnes_generator.h` | Top-level orchestrator: spec → mesh pipeline |

### Data Flow

```cpp
VNESConfig cfg = VNESConfig::from_json("spec.json");
for (auto& variant : cfg.variants) {
    SDFTree tree = build_sdf(variant);          // per-variant SDF
    Grid3D grid  = sample_grid(tree, resolution); // uniform or adaptive
    Mesh mesh    = marching_cubes(grid);
    mesh         = cleanup(mesh);
    export_stl(mesh, variant.class_id + ".stl");
}
```

### Key Algorithms

**Gyroid SDF:**
```cpp
float sdf_gyroid(vec3 p, float scale, float wall_thickness) {
    float g = sin(scale * p.x) * cos(scale * p.y) +
              sin(scale * p.y) * cos(scale * p.z) +
              sin(scale * p.z) * cos(scale * p.x);
    return abs(g) - wall_thickness;  // shell
}
```

**Density-gradient gyroid (Variant B):**
```cpp
float sdf_gradient_gyroid(vec3 p, float t) {
    float scale = lerp(1.0, 0.3, t);  // t = 0..1 from core to growth face
    return sdf_gyroid(p, scale, wall_thickness(t));
}
```

**Schwarz P (Variant A):**
```cpp
float sdf_schwarz_p(vec3 p, float scale) {
    return cos(scale * p.x) + cos(scale * p.y) + cos(scale * p.z);
}
```

---

## Track 2 — C++ Variant Header Definitions

**Goal:** Compile-time metadata constants for all 5 variants, matching the JSON spec exactly.

### Implementation

Each variant gets a header with:
- Variant class ID (e.g., `VNE_A_250`)
- Structural role flags (load-bearing, compressive priority, fluid priority)
- Biological support flags (roots, mycelium, bacteria, invertebrates)
- Flow characteristics (air, water, root, mycelium passage)
- Pore geometry (min/max pore size, distribution)
- Face interface roles (square = structural, hex = transport — per variant)
- Manufacturing constraints (supportless, min wall, min feature)

### Files

| File | Role |
|------|------|
| `vnes/variant_types.h` | Enums and structs for all metadata fields |
| `vnes/variant_a.h` | VNE-A-250 metadata constants |
| `vnes/variant_b.h` | VNE-B-250 metadata constants |
| `vnes/variant_c.h` | VNE-C-250 metadata constants |
| `vnes/variant_d.h` | VNE-D-250 metadata constants |
| `vnes/variant_e.h` | VNE-E-250 metadata constants |
| `vnes/variant_registry.h` | Runtime lookup: class_id → VariantMetadata |

### Example Struct

```cpp
enum class FaceRole : uint8_t {
    LoadBearing,
    StructuralFlange,
    StaticStructural,
    StructuralSeal,
    CapillaryFlow,
    FluidAirExchange,
    HydroponicFlow,
    MainVascularPath,
    UtilityTrunk
};

struct VariantMetadata {
    const char* class_id;           // "VNE-A-250"
    float compressive_priority;      // 0.0–1.0
    float fluid_priority;            // 0.0–1.0
    bool supports_roots;
    bool supports_mycelium;
    bool supports_bacteria;
    bool supports_invertebrates;
    float pore_size_min;             // mm
    float pore_size_max;             // mm
    FaceRole square_face_role;
    FaceRole hex_face_role;
    // ... conduit specs for D, sensor mounts for E, etc.
};

constexpr VariantMetadata VNE_A_250 = {
    .class_id = "VNE-A-250",
    .compressive_priority = 0.95f,
    .fluid_priority = 0.15f,
    .supports_roots = false,
    .supports_mycelium = true,
    .supports_bacteria = true,
    .supports_invertebrates = false,
    .pore_size_min = 1.0f,
    .pore_size_max = 3.0f,
    .square_face_role = FaceRole::LoadBearing,
    .hex_face_role = FaceRole::CapillaryFlow
};
```

**Est. effort:** 2–3 days

---

## Track 3 — Deterministic Placement Engine

**Goal:** A standalone engine that takes a genotype (list of functional requirements) and produces a 3D lattice of placed VNES variant cells.

### Architecture

```
Genotype (requirements list)
     ↓
Rule Matcher (IF load > threshold → VNE-A, IF root_zone → VNE-C, ...)
     ↓
Constraint Solver (structural continuity, face compatibility, orientation)
     ↓
Lattice Builder (produces CellLattice — a 3D array of Variant + orientation)
     ↓
Variant Interconnect Validator (face-type compatibility matrix check)
     ↓
Output: JSON lattice, serialized CellLattice, or direct mesh assembly
```

### Rule System

Rules are YAML-based, matching the VNES spec's Assembly Genome Concept:

```yaml
rules:
  - condition: "load_bearing == true"
    place: "VNE-A-250"
    priority: 10

  - condition: "root_zone == true"
    place: "VNE-C-250"
    priority: 8

  - condition: "transport_required == true"
    place: "VNE-D-250"
    priority: 7

  - condition: "harvest_surface == true"
    place: "VNE-B-250"
    priority: 6

  - condition: "instrumentation == true"
    place: "VNE-E-250"
    priority: 5
```

### Face Compatibility Validation

Before placing a cell adjacent to another, validate the Interface Compatibility Matrix:

```cpp
bool faces_compatible(VariantMetadata a, FaceRole fa,
                      VariantMetadata b, FaceRole fb) {
    // Look up (fa, fb) in compatibility matrix
    // Return true if the pair is a valid interface
}
```

### Files

| File | Role |
|------|------|
| `vnes/placement_engine.h` | Genotype → lattice |
| `vnes/rule_matcher.h` | Requirement → variant assignment |
| `vnes/constraint_solver.h` | Structural + interface constraints |
| `vnes/lattice.h` | CellLattice 3D array type |
| `vnes/interface_matrix.h` | Face compatibility lookup |
| `vnes/placement_rules.yaml` | Default rule set |

### Example CLI

```
vnes-place --genotype '{"requirements":["load_support","root_habitat","water_transport"]}' \
           --dimensions 3x5x1 \
           --output lattice.json
```

**Est. effort:** 5–7 days

---

## Track 4 — SVO / Voxel System Integration

**Goal:** Map VNES Kelvin cells onto the existing SVO octree for physical simulation (fluid flow, structural load, biological growth).

### Integration Points

| System | What it provides | What VNES needs |
|--------|------------------|-----------------|
| `voxel/svo` | Sparse voxel octree | Per-cell occupancy + variant ID |
| `physics/voxel_coupling.h` | Deformation, fracture | Variant-specific material properties |
| `physics/pillar_coupling.h` | Pillar field propagation | Per-cell pillar influence (for biological simulation) |
| `physics/energy_system.cpp` | Energy transport | Hydroponic nutrient flow through cells |
| `simulation/cellular_automata.cu` | Grid-based CA | Agent diffusion through cell pores |

### Approach

Each Kelvin cell maps to an SVO node or a dense block of voxels at a chosen resolution (e.g., 1 mm → 250³ voxels per cell, or coarser using the SVO's LOD):

```cpp
struct VNESCell {
    VariantID variant;      // VNE_A through VNE_E
    uint8_t orientation;    // 0–23 face rotations
    float occupancy;        // 0.0–1.0 (for partial cells at boundaries)
    float fluid_saturation; // for irrigation simulation
    float structural_load;  // for load-bearing analysis
};
```

### Simulation-Side Benefits

- Fluid flow through VNE-D trunks + VNE-C pores for hydroponic simulation
- Mycelial growth through VNE-A Schwarz P network
- Structural load propagation through VNE-A compressive skeleton
- Light penetration through VNE-B growth face openings
- Sensor data routing through VNE-E utility corridors

### Files

| File | Role |
|------|------|
| `vnes/vnes_svo_bridge.h` | Convert VNES lattice ↔ SVO nodes |
| `vnes/vnes_simulation.h` | Per-cell simulation state + update |
| `vnes/vnes_fluid_sim.h` | Hydroponic nutrient/water flow |
| `vnes/vnes_growth_sim.h` | Biological growth + colonization |

### Resolution Strategy

| Simulation Type | Voxel Resolution | Notes |
|----------------|------------------|-------|
| Structural load | ~10 mm (25³ / cell) | Continuum approximation |
| Fluid flow | ~2 mm (125³ / cell) | Resolves VNE-D trunk (20 mm) |
| Biological growth | ~1 mm (250³ / cell) | Resolves pores (1–6 mm) |
| Light penetration | ~5 mm (50³ / cell) | Volumetric shadow maps |

**Est. effort:** 8–12 days

---

## Track 5 — Standalone Python `vnes-generator` Package

**Goal:** A pip-installable Python package for designers and biologists to generate VNES lattices and export STL without needing the C++ toolchain.

### CLI

```
pip install vnes-generator

vnes generate spec.json -o output.stl
vnes lattice --genotype '{"requirements":["load_support","root_habitat"]}' \
             --dimensions 4x4x1 --format stl
vnes info VNE-A-250
vnes validate spec.json
vnes visualize lattice.json --output lattice.obj
```

### Architecture

Python bindings over the C++ library (Track 1 + Track 3) via pybind11, with a pure-Python fallback for basic cases:

```
┌──────────────────────────────────────┐
│           vnes CLI (click)           │
├──────────────────────────────────────┤
│  vnes.generator  ← pybind11 → C++   │
│  vnes.lattice    ← pybind11 → C++   │
│  vnes.sdf        (Python reference)  │
│  vnes.validate   (JSON Schema)       │
│  vnes.visualize  (trimesh + vtk)     │
│  vnes.metadata   (variant database)  │
└──────────────────────────────────────┘
```

### Pure-Python SDF Reference

For users who cannot compile C++, provide a NumPy-based SDF → Marching Cubes (scikit-image) pipeline:

```python
import numpy as np
from skimage.measure import marching_cubes

def gyroid_sdf(p, scale, thickness):
    g = (np.sin(scale * p[..., 0]) * np.cos(scale * p[..., 1]) +
         np.sin(scale * p[..., 1]) * np.cos(scale * p[..., 2]) +
         np.sin(scale * p[..., 2]) * np.cos(scale * p[..., 0]))
    return np.abs(g) - thickness

# Sample on 3D grid
grid = np.stack(np.meshgrid(x, y, z, indexing='ij'), axis=-1)
sdf = gyroid_sdf(grid, scale=2*np.pi/6, thickness=0.1)
verts, faces, _, _ = marching_cubes(sdf, level=0)
```

### Files

| File | Role |
|------|------|
| `vnes/__init__.py` | Package init, version |
| `vnes/metadata.py` | Variant database (parsed from JSON spec) |
| `vnes/sdf.py` | Pure-Python SDF primitives |
| `vnes/generator.py` | SDF → marching cubes → export |
| `vnes/lattice.py` | Genotype → placement engine (Python) |
| `vnes/validate.py` | JSON Schema validation |
| `vnes/visualize.py` | trimesh/plotly visualization |
| `vnes/cli.py` | Click CLI entry point |
| `pyproject.toml` | Build system (setuptools + pybind11) |
| `src/bindings.cpp` | pybind11 bindings to C++ |

**Est. effort:** 5–8 days

---

## Track 6 — Specification Document & README

**Goal:** A self-contained specification document (this plan + the original JSON spec) and a README for the `vnes/` module.

### Files

| File | Role |
|------|------|
| `vnes/SPEC.md` | Full VNES specification (the original design doc merged with this plan) |
| `vnes/PLAN.md` | This file — implementation tracking |
| `vnes/README.md` | Quick-start for users of the vnes module |
| `vnes/spec_v0.1.json` | The canonical JSON spec as a standalone schema |
| `vnes/spec_schema.json` | JSON Schema for validation |

### README Outline

```markdown
# VNES — Van Neumann Ecosystem Substrate

A modular ecological construction framework built around the
Regular Truncated Octahedron (Kelvin Cell).

## Variants
- **VNE-A** Structural Core (Schwarz P, load-bearing)
- **VNE-B** Harvest Surface (gradient gyroid, plant production)
- **VNE-C** Universal Rooting (open-cell gyroid, hydroponics)
- **VNE-D** Transport Spine (vascular trunks, fluid/air backbone)
- **VNE-E** Utility Service (sensors, cables, maintenance)

## Quick Start
    # C++ generation
    cmake -B build && cmake --build build
    ./build/vnes-gen spec.json -o cell.stl

    # Python (pure)
    pip install vnes-generator
    vnes generate spec.json -o lattice.stl

## Assembly
Place cells via genotype rules → validated lattice → simulation or fabrication.

## License
MIT (same as Van_Nueman_Engine)
```

**Est. effort:** 1–2 days

---

## Dependency Graph

```
Track 1 (SDF → Mesh)
    └── needed by Track 3 (placement engine feeds cell count)
    └── needed by Track 4 (simulation consumes voxelized cells)
    └── needed by Track 5 (Python bindings wrap C++)

Track 2 (Metadata Headers)
    └── needed by Track 1 (variant config for SDF generation)
    └── needed by Track 3 (variant metadata for rules)
    └── needed by Track 4 (variant properties for simulation)

Track 3 (Placement Engine)
    └── needed by Track 5 (Python CLI entry point)

Track 4 (SVO Integration)
    └── depends on Track 1 + 2 (cell geometry + metadata)

Track 5 (Python Package)
    └── depends on Track 1 + 3 (bindings to C++)

Track 6 (Docs)
    └── depends on nothing, updates as tracks progress
```

---

## Milestones

| Milestone | Tracks | Deliverable | Target |
|-----------|--------|-------------|--------|
| M1 | 2 | All 5 variant metadata headers + registry | Week 1 |
| M2 | 1 | SDF primitives + Kelvin hull + marching cubes | Week 2–3 |
| M3 | 1 | All 5 variant SDF generators | Week 4 |
| M4 | 3 | Placement engine with rule system | Week 3–4 |
| M5 | 1 | STL/PLY export + mesh cleanup | Week 5 |
| M6 | 4 | SVO bridge + fluid flow simulation | Week 5–6 |
| M7 | 5 | Python package (bindings + CLI) | Week 6–7 |
| M8 | 6 | All docs merged and published | Week 7 |

---

## Resource Requirements

### C++
- Eigen3 or custom vec3/mat3 (already in `include/`)
- Marching Cubes reference implementation (can be standalone)
- nlohmann/json (already in codebase)
- fmtlib (already in codebase)

### Python
- numpy, scikit-image (marching_cubes)
- trimesh (STL/PLY export + visualization)
- click (CLI)
- jsonschema (spec validation)
- pybind11 (C++ bindings)

### GPU (optional, future)
- CUDA Marching Cubes for high-resolution cell generation
- CUDA SDF sampling (grid generation is embarrassingly parallel)

---

## Risks & Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| Marching Cubes artifacts (non-manifold) | Failed 3D prints | Mesh cleanup pass with manifold repair |
| SDF gradient discontinuities at variant boundaries | Visible seams | CSG blend/smooth operations at interfaces |
| Snap-fit tolerance variance across FDM printers | Poor assembly | Parametric clearance in JSON spec (0.25 mm default) |
| Large STL files (250³ cell → ~1M triangles) | Slow slicing | Simplify to ~0.5M tris for printing; full resolution for simulation |
| Python marching cubes too slow for lattice generation | Poor UX | Fall back to C++ via pybind11; pure-Python for single cells only |

---

## Future Evolution (Phase 2+)

- **Constraint optimization:** Genetic algorithms / simulated annealing for optimal cell placement
- **Sensor-driven adaptive placement:** Input from moisture, temperature, growth-rate sensors → layout recommendations
- **Multi-material printing:** Rigid PLA frame + flexible TPU gaskets in a single cell
- **Bio-integration:** Embedded mycelial inoculation channels, bacterial biofilm scaffold regions
- **CNC-compatible STEP export:** For subtractive manufacturing from foam/wood blocks
- **Distributed computing substrate:** VNE-E cells with embedded RP2040/ESP32 nodes for ecological monitoring mesh
