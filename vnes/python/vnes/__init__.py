"""
VNES — Van Neumann Ecosystem Substrate

A modular ecological construction framework built around the
Regular Truncated Octahedron (Kelvin Cell).

Variants:
  VNE-A: Structural Core (Schwarz P, load-bearing)
  VNE-B: Harvest Surface (gradient gyroid, plant production)
  VNE-C: Universal Rooting (open-cell gyroid, hydroponics)
  VNE-D: Transport Spine (vascular trunks, fluid/air backbone)
  VNE-E: Utility Service (sensors, cables, maintenance)
"""

__version__ = "0.1.0"

# Try to import the C++ acceleration module
# (compiled from src/bindings.cpp via setup.py)
_HAS_CPP = False
_cpp = None
try:
    import vnes_cpp as _cpp
    _HAS_CPP = True
except ImportError:
    try:
        from . import vnes_cpp as _cpp
        _HAS_CPP = True
    except ImportError:
        pass

from .metadata import (
    FaceRole,
    VARIANT_IDS,
    VARIANT_METADATA,
    lookup_variant,
    faces_compatible,
    CELL_RADIUS_MM,
    CELL_SPACING_MM,
    FRAME_THICKNESS_MM,
)

__all__ = [
    "FaceRole",
    "VARIANT_IDS",
    "VARIANT_METADATA",
    "lookup_variant",
    "faces_compatible",
    "CELL_RADIUS_MM",
    "CELL_SPACING_MM",
    "FRAME_THICKNESS_MM",
]
