# VNES Specification v0.1

**Van Neumann Ecosystem Substrate** — a modular ecological construction framework.

## 1. Overview

The VNES defines a single universal geometric primitive: the **Regular Truncated Octahedron (Kelvin Cell)** at 250 mm circumscribed diameter, specialized into 5 functional variants. These cells snap together on a Body-Centered Cubic (BCC) lattice to form arbitrary 3D structures for controlled-environment agriculture, mycology, hydroponics, and bioregenerative life support.

### Design Principles

1. **One cell, many functions** — A single geometry variantized by interior architecture
2. **Deterministic assembly** — Placement follows explicit compatibility rules, no trial-and-error
3. **Fabrication-ready** — Each variant is supportless for FDM printing with min wall > 1.2 mm
4. **Simulation-native** — Cells map directly to the SVO octree for physics simulation
5. **Open standard** — MIT-licensed, JSON spec, both C++ and Python implementations

## 2. Geometry

### Kelvin Cell

The Regular Truncated Octahedron has:
- **14 faces:** 6 squares + 8 regular hexagons
- **24 vertices**
- **36 edges**
- **Circumscribed sphere:** 250 mm diameter (radius = 125 mm)
- **Edge length:** ~79.06 mm
- **Volume:** ~5.58 L
- **Frame thickness:** 15 mm (hollow shell)

### BCC Lattice

Cells pack on a Body-Centered Cubic lattice:

| Property | Value |
|----------|-------|
| Spacing (square-face center to center) | 158.11 mm |
| Half-spacing | 79.055 mm |
| Square-face offsets (cardinal) | (±2,0,0), (0,±2,0), (0,0,±2) |
| Hex-face offsets (diagonal) | (±1,±1,±1) — 8 combinations |
| Neighbors per cell | 14 (6 square + 8 hex) |

Coordinate system:
- All integer (i, j, k) coordinates are valid lattice positions
- Parity = (i + j + k) mod 2 distinguishes the two interpenetrating sublattices
- World position: (i × 79.055, j × 79.055, k × 79.055) mm

### Snap-Fit Peg System

Each square face has 4 pegs/sockets arranged in a 2×2 pattern:

| Property | Value |
|----------|-------|
| Peg diameter | 8.0 mm |
| Peg depth | 12.0 mm |
| Clearance | 0.25 mm |
| Sockets per face | 4 |

## 3. Variants

### VNE-A-250: Structural Core

The load-bearing skeleton. Interior filled with a Schwarz P triply periodic minimal surface (TPMS) with 1–3 mm pores.

| Field | Value |
|-------|-------|
| Load-bearing | yes |
| Compressive priority | 0.95 |
| Fluid priority | 0.15 |
| Bio support | Mycelium, Bacteria |
| Pore size | 1–3 mm (uniform) |
| Flow: airflow | no |
| Flow: waterflow | yes |
| Flow: root passage | no |
| Flow: mycelium passage | yes |
| Square face role | LoadBearing |
| Hex face role | CapillaryFlow |

**Use cases:**
- Structural skeleton for multi-cell assemblies
- Mycelial network substrate
- Moisture wicking (capillary action)

### VNE-B-250: Harvest Surface

Plant production cell with density-gradient gyroid — denser at the core, more open toward the growth face. Supports the full biological chain (roots, mycelium, bacteria, invertebrates).

| Field | Value |
|-------|-------|
| Load-bearing | no |
| Compressive priority | 0.40 |
| Fluid priority | 0.60 |
| Bio support | Mycelium, Bacteria, PlantRoots, Invertebrates |
| Pore size | 3–6 mm (gradient toward growth face) |
| Flow: airflow | yes |
| Flow: waterflow | yes |
| Flow: root passage | no |
| Flow: mycelium passage | yes |
| Square face role | StructuralFlange |
| Hex face role | FluidAirExchange |

**Growth face openings:** 20–30 mm diameter, on one hexagonal face.

**Use cases:**
- Food production surface
- Vermicomposting habitat
- Aeroponic spray chamber

### VNE-C-250: Universal Rooting

Hydroponics and microbial habitat cell with uniform open-cell gyroid (3–6 mm pores). All flow modes active (air, water, roots, mycelium).

| Field | Value |
|-------|-------|
| Load-bearing | no |
| Compressive priority | 0.40 |
| Fluid priority | 0.60 |
| Bio support | Mycelium, Bacteria, PlantRoots, Invertebrates |
| Pore size | 3–6 mm (uniform) |
| Flow: airflow | yes |
| Flow: waterflow | yes |
| Flow: root passage | yes |
| Flow: mycelium passage | yes |
| Square face role | StructuralFlange |
| Hex face role | HydroponicFlow |

**Use cases:**
- Deep-water hydroponics
- Aeroponic root chamber
- Microbial biofilm reactor
- Worm-based composting

### VNE-D-250: Transport Spine

Fluid and air backbone with 3 intersecting 20 mm diameter vascular trunks arranged orthogonally, meeting at a Steinmetz-blended junction in the cell center. Gyroid lattice fills remaining volume.

| Field | Value |
|-------|-------|
| Load-bearing | no |
| Compressive priority | 0.30 |
| Fluid priority | 0.90 |
| Bio support | Mycelium, Bacteria |
| Pore size | 3–6 mm (uniform) |
| Flow: airflow | yes |
| Flow: waterflow | yes |
| Flow: root passage | no |
| Flow: mycelium passage | yes |
| Square face role | StaticStructural |
| Hex face role | MainVascularPath |

**Conduit spec:**
- 3 trunks at 20 mm diameter
- Steinmetz junction blend radius: 4.0 mm
- Trunks align with hex-face axes (main vascular paths)

**Use cases:**
- Main water/nutrient supply line
- Air circulation backbone
- Drainage collection

### VNE-E-250: Utility Service

Instrumentation and utility cell with dedicated cable conduits, fluid sampling channels, and standardized sensor mounting interfaces.

| Field | Value |
|-------|-------|
| Load-bearing | no |
| Compressive priority | 0.25 |
| Fluid priority | 0.50 |
| Bio support | Mycelium, Bacteria |
| Pore size | 3–6 mm (uniform) |
| Flow: airflow | yes |
| Flow: waterflow | yes |
| Flow: root passage | no |
| Flow: mycelium passage | yes |
| Square face role | StructuralSeal |
| Hex face role | UtilityTrunk |

**Utility spec:**
- 4 cable conduits at 8 mm diameter
- 2 fluid sampling channels at 6 mm diameter
- 6 sensor mounts (12 mm bores):
  - Moisture, temperature, humidity, pH, flow rate, CO₂
- 1 inspection port at 30 mm diameter

**Use cases:**
- Sensor network nodes
- Cable routing and junction
- Fluid sampling stations

## 4. Interface Compatibility Matrix

### Square-Face Compatibility

| A ↓ / B → | LoadBearing (A) | StructuralFlange (B/C) | StaticStructural (D) | StructuralSeal (E) |
|---|---|---|---|---|
| **LoadBearing (A)** | ✓ Full load transfer | ✓ Structural support | ✓ Rigid coupling | ✓ Sealed interface |
| **StructuralFlange (B/C)** | ✓ | ✓ Flange-to-flange | ✓ Mixed structural | ✓ Mixed utility |
| **StaticStructural (D)** | ✓ | ✓ | ✓ D–D rigid | ✓ D–E transition |
| **StructuralSeal (E)** | ✓ | ✓ | ✓ | ✓ E–E sealed |

### Hex-Face Compatibility

All hex faces are mechanically and dimensionally identical — they differ only in flow function. Therefore **all hex face roles are mutually compatible**.

| | CapillaryFlow | FluidAirExchange | HydroponicFlow | MainVascularPath | UtilityTrunk |
|---|---|---|---|---|---|
| **CapillaryFlow** | ✓ | ✓ | ✓ | ✓ | ✓ |
| **FluidAirExchange** | ✓ | ✓ | ✓ | ✓ | ✓ |
| **HydroponicFlow** | ✓ | ✓ | ✓ | ✓ | ✓ |
| **MainVascularPath** | ✓ | ✓ | ✓ | ✓ | ✓ |
| **UtilityTrunk** | ✓ | ✓ | ✓ | ✓ | ✓ |

### Square-Hex Incompatibility

Square faces and hex faces can never mate — their geometries are fundamentally different (4-sided vs 6-sided).

## 5. Placement Rules

The Assembly Genome Concept maps functional requirements to variant placement using a priority-ordered rule system:

| Priority | Condition | Place |
|----------|-----------|-------|
| 10 | load_bearing == true | VNE-A-250 |
| 8 | transport_required == true | VNE-D-250 |
| 7 | root_zone == true | VNE-C-250 |
| 6 | harvest_surface == true | VNE-B-250 |
| 5 | instrumentation == true | VNE-E-250 |
| 1 | (default) | VNE-A-250 |

The greedy solver places cells sequentially, validating each placement against existing neighbors. If direct placement fails, `place_cell_auto()` tries all 5 variants in priority order.

## 6. Physics Constants

| Constant | Value | Description |
|----------|-------|-------------|
| VNES_CELL_DIAMETER_MM | 250.0 | Circumscribed sphere diameter |
| VNES_CELL_RADIUS_MM | 125.0 | Circumscribed sphere radius |
| VNES_FRAME_THICKNESS_MM | 15.0 | Hollow shell wall thickness |
| VNES_EDGE_LENGTH_MM | 79.06 | Kelvin cell edge length (approx.) |
| VNES_VOLUME_LITERS | 5.58 | Cell interior volume (approx.) |
| VNES_PEG_DIAMETER_MM | 8.0 | Snap-fit peg diameter |
| VNES_PEG_DEPTH_MM | 12.0 | Snap-fit peg engagement depth |
| VNES_PEG_CLEARANCE_MM | 0.25 | Print tolerance clearance |
| VNES_SOCKETS_PER_FACE | 4 | Pegs per square face |
| VNES_CELL_SPACING_MM | 158.11 | BCC square-face center distance |
| VNES_HALF_SPACING_MM | 79.055 | BCC half-spacing |
| VNES_DEFAULT_GRID_RES | 128 | Marching cubes grid cells/axis |
| VNES_GROWTH_OPENING_MIN_MM | 20.0 | VNE-B growth face min opening |
| VNES_GROWTH_OPENING_MAX_MM | 30.0 | VNE-B growth face max opening |

## 7. Marching Cubes

Default configuration:
- Grid resolution: 128³ cells per axis
- Iso-level: 0.0 (SDF zero crossing)
- Output: triangle mesh with per-vertex normals
- Resolution per cell: ~250 mm ÷ 128 ≈ 1.95 mm/voxel

### Tables

Standard 16-cube Marching Cubes lookup table (256 entries), stored in flattened form in `vnes_library.cpp`.

## 8. SVO Integration

VNES cells map to the SVO octree at 2 mm/voxel resolution:
- 1 VNES cell ≈ 63 SVO voxels per dimension
- Material ID 10–14 encodes variant type (A–E)
- Leaf nodes store variant metadata packed as RGBA

Physics simulation channels:
| Simulation | Voxel Resolution | Resolution per cell |
|------------|-----------------|---------------------|
| Structural load | ~10 mm | 25³ / cell |
| Fluid flow | ~2 mm | 125³ / cell |
| Biological growth | ~1 mm | 250³ / cell |
| Light penetration | ~5 mm | 50³ / cell |

## 9. Fabrication Constraints

All variants are designed for FDM printing with:
- **Min wall thickness:** 1.2 mm
- **Min feature size:** 0.8 mm
- **Supportless:** yes (no overhangs > 45°)
- **Snap-fit clearance:** 0.25 mm

## 10. File Formats

### STL Export
- Binary STL (default) — 80-byte header + triangle count + 50-byte records
- ASCII STL (on request) — `solid`/`endsolid` format
- Normals computed from SDF gradient

### PLY Export
- ASCII PLY with vertex positions + normals

## 11. Python Package

The `vnes` Python package provides 3 access levels:

| Level | Capabilities | Requirements |
|-------|-------------|--------------|
| Pure Python | info, validate, list-variants, basic mesh (scikit-image) | numpy, scikit-image, click |
| C++ accelerated (pip) | All of above + fast MC mesh generation + full lattice placement | pybind11, MSVC/gcc toolchain |
| C++ accelerated (engine) | Full integration with SVO, physics, agent framework | Van_Nueman_Engine build |

The `vnes` CLI entry point is:
```
vnes info <variant-id>        — Show variant metadata
vnes generate <variant-id>    — Generate mesh, export STL
vnes lattice <width> <height> <depth> — Place and export lattice
vines validate <variant-id>   — Validate instance
vines list-variants           — List all 5 variants
```

## 12. License

MIT License — see `LICENSE` in the Van_Nueman_Engine root.
