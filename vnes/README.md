# VNES — Van Neumann Ecosystem Substrate

A modular ecological construction framework built around the **Regular Truncated Octahedron (Kelvin Cell)** — a single universal 250 mm primitive with 5 functional variants for controlled-environment agriculture, mycology, and bioregenerative life support.

## Variants

| ID | Name | Role | Interior | Square Face | Hex Face |
|----|------|------|----------|-------------|----------|
| VNE-A-250 | Structural Core | Load-bearing skeleton | Schwarz P minimal surface (1–3 mm pores) | LoadBearing | CapillaryFlow |
| VNE-B-250 | Harvest Surface | Plant production | Gradient gyroid (3–6 mm, denser at core) | StructuralFlange | FluidAirExchange |
| VNE-C-250 | Universal Rooting | Hydroponics + habitat | Open-cell gyroid (3–6 mm uniform) | StructuralFlange | HydroponicFlow |
| VNE-D-250 | Transport Spine | Fluid/air backbone | 3× 20 mm vascular trunks + gyroid fill | StaticStructural | MainVascularPath |
| VNE-E-250 | Utility Service | Sensors, cables, I/O | Gyroid + 4× cable/2× fluid conduits + 6 sensor mounts | StructuralSeal | UtilityTrunk |

## Quick Start

### C++ (full engine integration)

```bash
# From Van_Nueman_Engine root
cmake -B build && cmake --build build

# The vnvnes library is linked into van-nueman-game
# Include vnes/vnes_generator.h for SDF → mesh pipeline
# Include vnes/lattice.h / constraint_solver.h for placement
```

### Python (pure, no C++ toolchain needed)

```bash
cd vnes/python
pip install -e .

# CLI usage
vnes info VNE-A-250
vnes generate VNE-A-250 -o output.stl
vnes lattice --width 3 --height 3 --depth 1
vnes validate VNE-A-250
vnes list-variants
```

### Python (with C++ acceleration)

Requires MSVC toolchain (Windows) or gcc/clang (Linux). Build via the main engine CMake:

```bash
cmake -B build -DVN_BUILD_VNES_PYTHON=ON
cmake --build build
# The vnes_cpp.pyd extension will be in build/python/
```

The pure-Python fallback activates automatically if the C++ extension is unavailable.

## Architecture

```
vnes/                       Header-only C++ library
  ├── variant_types.h       Metadata structs + all 5 variant definitions
  ├── variant_registry.h    Runtime lookup + interface compatibility matrix
  ├── sdf_primitives.h      SDF primitives (sphere, box, gyroid, Schwarz P, etc.)
  ├── kelvin_hull.h         Kelvin cell hull SDF with snap-fit pegs
  ├── variant_sdf.h         All 5 variant SDF generators (dispatch by class ID)
  ├── marching_cubes.h      Grid-based isosurface extraction
  ├── mesh_cleanup.h        Vertex dedup + degenerate tri removal
  ├── export.h              STL (binary/ASCII) + PLY export
  ├── vnes_generator.h      Orchestrator: config → mesh
  ├── lattice.h             BCC lattice placement engine
  ├── rule_matcher.h        Face compatibility + neighborhood validation
  ├── constraint_solver.h   Greedy spatial constraint solver
  ├── vnes_svo_bridge.h     VNES ↔ SVO octree bridge for simulation
  ├── vnes_library.cpp      Translation unit (registry array + MC tables)
  ├── python/               Python package (pybind11 + pure-Python fallback)
  └── PLAN.md               Implementation plan

CMake target: vnvnes (static library), linked by van-nueman-game
```

## Face Compatibility Matrix

Each Kelvin cell has 14 faces (6 square + 8 hexagonal). Faces are compatible only if they share the same role category:

| | LoadBearing | StructuralFlange | StaticStructural | StructuralSeal | Hex (all) |
|---|---|---|---|---|---|
| **LoadBearing** | ✓ | ✓ | ✓ | ✓ | ✗ |
| **StructuralFlange** | ✓ | ✓ | ✓ | ✓ | ✗ |
| **StaticStructural** | ✓ | ✓ | ✓ | ✓ | ✗ |
| **StructuralSeal** | ✓ | ✓ | ✓ | ✓ | ✗ |
| **Hex (any)** | ✗ | ✗ | ✗ | ✗ | ✓ |

All hex faces are mutually compatible (they differ only in flow function).

## BCC Lattice

Cells are placed on a Body-Centered Cubic lattice:
- Edge length: ~158.11 mm (square-face center to center)
- 14 neighbors per cell (6 square + 8 hexagonal)
- All integer (i, j, k) coordinates valid; parity = (i+j+k)%2
- Position: (i × 79.055, j × 79.055, k × 79.055) mm

## License

MIT (same as Van_Nueman_Engine)
