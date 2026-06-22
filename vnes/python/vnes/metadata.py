"""
VNES variant metadata — pure-Python variant database.

Provides the same constants and lookup functions as the C++
variant_registry.h and variant_types.h.
"""

from enum import IntEnum


class FaceRole(IntEnum):
    # Square-face roles
    LoadBearing = 0
    StructuralFlange = 1
    StaticStructural = 2
    StructuralSeal = 3
    # Hex-face roles
    CapillaryFlow = 4
    FluidAirExchange = 5
    HydroponicFlow = 6
    MainVascularPath = 7
    UtilityTrunk = 8


# ── Physics constants ──────────────────────────────────────
CELL_RADIUS_MM = 125.0
CELL_DIAMETER_MM = 250.0
FRAME_THICKNESS_MM = 15.0
CELL_SPACING_MM = 158.11
HALF_SPACING_MM = 79.055
EDGE_LENGTH_MM = 79.06
PEG_DIAMETER_MM = 8.0
PEG_DEPTH_MM = 12.0

# ── Variant database ───────────────────────────────────────

VARIANT_METADATA = {
    "VNE-A-250": {
        "class_id": "VNE-A-250",
        "load_bearing": True,
        "compressive_priority": 0.95,
        "fluid_priority": 0.15,
        "bio_support": 0b0011,
        "square_face_role": FaceRole.LoadBearing,
        "hex_face_role": FaceRole.CapillaryFlow,
        "frame_thickness_mm": 15.0,
        "supportless": True,
        "min_wall_mm": 1.2,
        "min_feature_mm": 0.8,
        "description": "Structural Core Cell — load-bearing skeleton",
    },
    "VNE-B-250": {
        "class_id": "VNE-B-250",
        "load_bearing": False,
        "compressive_priority": 0.40,
        "fluid_priority": 0.60,
        "bio_support": 0b1111,
        "square_face_role": FaceRole.StructuralFlange,
        "hex_face_role": FaceRole.FluidAirExchange,
        "frame_thickness_mm": 15.0,
        "supportless": True,
        "min_wall_mm": 1.2,
        "min_feature_mm": 0.8,
        "description": "Harvest Surface Cell — plant production",
    },
    "VNE-C-250": {
        "class_id": "VNE-C-250",
        "load_bearing": False,
        "compressive_priority": 0.40,
        "fluid_priority": 0.60,
        "bio_support": 0b1111,
        "square_face_role": FaceRole.StructuralFlange,
        "hex_face_role": FaceRole.HydroponicFlow,
        "frame_thickness_mm": 15.0,
        "supportless": True,
        "min_wall_mm": 1.2,
        "min_feature_mm": 0.8,
        "description": "Universal Rooting Cell — hydroponics",
    },
    "VNE-D-250": {
        "class_id": "VNE-D-250",
        "load_bearing": False,
        "compressive_priority": 0.30,
        "fluid_priority": 0.90,
        "bio_support": 0b0011,
        "square_face_role": FaceRole.StaticStructural,
        "hex_face_role": FaceRole.MainVascularPath,
        "frame_thickness_mm": 15.0,
        "supportless": True,
        "min_wall_mm": 1.2,
        "min_feature_mm": 0.8,
        "description": "Transport Spine Cell — vascular trunk",
    },
    "VNE-E-250": {
        "class_id": "VNE-E-250",
        "load_bearing": False,
        "compressive_priority": 0.25,
        "fluid_priority": 0.50,
        "bio_support": 0b0011,
        "square_face_role": FaceRole.StructuralSeal,
        "hex_face_role": FaceRole.UtilityTrunk,
        "frame_thickness_mm": 15.0,
        "supportless": True,
        "min_wall_mm": 1.2,
        "min_feature_mm": 0.8,
        "description": "Utility Service Cell — sensors, cables",
    },
}

VARIANT_IDS = list(VARIANT_METADATA.keys())


def lookup_variant(variant_id: str):
    """Look up variant metadata by ID string. Returns dict or None."""
    return VARIANT_METADATA.get(variant_id)


# ── Interface Compatibility Matrix ─────────────────────────
_COMPATIBLE_PAIRS = {
    (FaceRole.LoadBearing, FaceRole.LoadBearing),
    (FaceRole.LoadBearing, FaceRole.StructuralFlange),
    (FaceRole.LoadBearing, FaceRole.StaticStructural),
    (FaceRole.LoadBearing, FaceRole.StructuralSeal),
    (FaceRole.StructuralFlange, FaceRole.StructuralFlange),
    (FaceRole.StructuralFlange, FaceRole.StaticStructural),
    (FaceRole.StructuralFlange, FaceRole.StructuralSeal),
    (FaceRole.StaticStructural, FaceRole.StaticStructural),
    (FaceRole.StaticStructural, FaceRole.StructuralSeal),
    (FaceRole.StructuralSeal, FaceRole.StructuralSeal),
}


def faces_compatible(a: FaceRole, b: FaceRole) -> bool:
    """Check if two FaceRole values can mate."""
    # Normalize order
    if a.value > b.value:
        a, b = b, a
    # Square-square pairs
    if a.value < 4 and b.value < 4:
        return (a, b) in _COMPATIBLE_PAIRS
    # Hex-hex pairs (all compatible)
    if a.value >= 4 and b.value >= 4:
        return True
    # Square-hex mismatch
    return False
