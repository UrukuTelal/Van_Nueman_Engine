"""
VNES mesh generation — SDF → triangulation → export.

Uses C++ acceleration when available (vnes_cpp), otherwise
falls back to pure-Python SDFs with scikit-image marching cubes.
"""

from __future__ import annotations
import numpy as np
import os
from . import _HAS_CPP, _cpp
from .metadata import VARIANT_METADATA, lookup_variant, CELL_RADIUS_MM


def generate_mesh(variant_id: str,
                  resolution: int = 128,
                  cleanup: bool = True):
    """Generate a triangle mesh for a VNES variant.

    Returns (vertices, triangles, normals) as numpy arrays.
    vertices: (N, 3) float32
    triangles: (M, 3) uint32
    normals: (N, 3) float32
    """
    if variant_id not in VARIANT_METADATA:
        raise ValueError(f"Unknown variant: {variant_id}")

    # C++ accelerated path
    if _HAS_CPP:
        return _cpp.generate_mesh(variant_id, resolution, cleanup)

    # Pure-Python fallback
    return _generate_mesh_py(variant_id, resolution, cleanup)


def _generate_mesh_py(variant_id, resolution, cleanup):
    """Pure-Python mesh generation using numpy + scikit-image."""
    try:
        from skimage.measure import marching_cubes
    except ImportError:
        raise ImportError(
            "scikit-image required for pure-Python mesh generation. "
            "Install: pip install scikit-image"
        )

    md = lookup_variant(variant_id)
    if md is None:
        raise ValueError(f"Unknown variant: {variant_id}")

    radius = CELL_RADIUS_MM
    diameter = radius * 2
    origin = -radius
    spacing = diameter / resolution

    # Build coordinate grid
    axis = np.linspace(origin, radius, resolution)
    x, y, z = np.meshgrid(axis, axis, axis, indexing="ij")
    grid = np.stack([x, y, z], axis=-1)

    # Evaluate SDF
    sdf = _variant_sdf_py(grid, variant_id)

    # Marching cubes
    verts, faces, normals, _ = marching_cubes(
        sdf, level=0, spacing=(spacing, spacing, spacing)
    )

    # Offset vertices back to world space
    verts += origin

    return (
        verts.astype(np.float32),
        faces.astype(np.uint32),
        normals.astype(np.float32),
    )


def _variant_sdf_py(p, variant_id):
    """Pure-Python SDF evaluator for a given variant."""
    scale = 2 * np.pi / 6.0
    t = 0.12  # thickness parameter (approximate for 15mm frame)

    if variant_id == "VNE-A-250":
        # Schwarz P minimal surface
        g = (np.cos(scale * p[..., 0]) +
             np.cos(scale * p[..., 1]) +
             np.cos(scale * p[..., 2]))
        return np.abs(g) - t

    elif variant_id in ("VNE-B-250", "VNE-C-250"):
        # Gyroid
        g = (np.sin(scale * p[..., 0]) * np.cos(scale * p[..., 1]) +
             np.sin(scale * p[..., 1]) * np.cos(scale * p[..., 2]) +
             np.sin(scale * p[..., 2]) * np.cos(scale * p[..., 0]))
        return np.abs(g) - t

    elif variant_id == "VNE-D-250":
        # Three intersecting vascular trunks
        r = np.sqrt(p[..., 0]**2 + p[..., 1]**2)
        trunk1 = r - 10.0
        r = np.sqrt(p[..., 0]**2 + p[..., 2]**2)
        trunk2 = r - 10.0
        r = np.sqrt(p[..., 1]**2 + p[..., 2]**2)
        trunk3 = r - 10.0
        interior = np.minimum(np.minimum(trunk1, trunk2), trunk3)
        # Kelvin hull bounds
        k = (np.abs(p[..., 0]) + np.abs(p[..., 1]) + np.abs(p[..., 2]) +
             np.maximum(np.maximum(np.abs(p[..., 0]),
                                   np.abs(p[..., 1])),
                        np.abs(p[..., 2]))) - radius
        return np.maximum(interior, k)

    elif variant_id == "VNE-E-250":
        # Cable conduits + open gyroid
        g = (np.sin(scale * p[..., 0]) * np.cos(scale * p[..., 1]) +
             np.sin(scale * p[..., 1]) * np.cos(scale * p[..., 2]) +
             np.sin(scale * p[..., 2]) * np.cos(scale * p[..., 0]))
        gyroid = np.abs(g) - t
        conduit_r = np.sqrt(p[..., 0]**2 + p[..., 1]**2)
        conduits = conduit_r - 4.0
        return np.minimum(gyroid, conduits)

    else:
        raise ValueError(f"Unknown variant: {variant_id}")


def export_stl(vertices, triangles, filename: str, binary: bool = True):
    """Export mesh to STL file."""
    if binary:
        _export_stl_binary(vertices, triangles, filename)
    else:
        _export_stl_ascii(vertices, triangles, filename)


def _export_stl_binary(verts, tris, filename):
    """Write binary STL."""
    n_tris = tris.shape[0]
    header = bytearray(80)
    with open(filename, "wb") as f:
        f.write(header)
        f.write(n_tris.to_bytes(4, "little"))
        for i in range(n_tris):
            v0 = verts[tris[i, 0]]
            v1 = verts[tris[i, 1]]
            v2 = verts[tris[i, 2]]
            # Face normal (flat)
            edge1 = v1 - v0
            edge2 = v2 - v0
            n = np.cross(edge1, edge2)
            norm = np.linalg.norm(n)
            if norm > 0:
                n = n / norm
            else:
                n = np.array([0.0, 0.0, 1.0])
            # Write normal
            for val in n:
                f.write(bytearray(struct.pack("<f", float(val))))
            # Write vertices
            for v in (v0, v1, v2):
                for val in v:
                    f.write(bytearray(struct.pack("<f", float(val))))
            # Attribute byte count
            f.write(b"\x00\x00")


def _export_stl_ascii(verts, tris, filename):
    """Write ASCII STL."""
    with open(filename, "w") as f:
        f.write(f"solid vnes\n")
        for i in range(tris.shape[0]):
            v0 = verts[tris[i, 0]]
            v1 = verts[tris[i, 1]]
            v2 = verts[tris[i, 2]]
            edge1 = v1 - v0
            edge2 = v2 - v0
            n = np.cross(edge1, edge2)
            norm = np.linalg.norm(n)
            if norm > 0:
                n = n / norm
            f.write(f"  facet normal {n[0]:.6f} {n[1]:.6f} {n[2]:.6f}\n")
            f.write(f"    outer loop\n")
            for v in (v0, v1, v2):
                f.write(f"      vertex {v[0]:.6f} {v[1]:.6f} {v[2]:.6f}\n")
            f.write(f"    endloop\n")
            f.write(f"  endfacet\n")
        f.write(f"endsolid vnes\n")


# Need struct module for binary export
import struct
