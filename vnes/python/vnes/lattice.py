"""
VNES lattice placement — genotype → placement grid.

Uses C++ acceleration when available (vnes_cpp), otherwise
runs a pure-Python placement engine.
"""

from . import _HAS_CPP, _cpp
from .metadata import VARIANT_METADATA, lookup_variant


class LatticeCoord:
    """BCC lattice coordinate."""
    def __init__(self, i: int = 0, j: int = 0, k: int = 0):
        self.i = i
        self.j = j
        self.k = k

    def __repr__(self):
        return f"<LatticeCoord {self.i},{self.j},{self.k}>"

    def __eq__(self, other):
        if not isinstance(other, LatticeCoord):
            return NotImplemented
        return self.i == other.i and self.j == other.j and self.k == other.k

    def __hash__(self):
        return hash((self.i, self.j, self.k))

    def neighbor(self, face_idx: int) -> "LatticeCoord":
        offsets = [
            (2, 0, 0), (-2, 0, 0), (0, 2, 0), (0, -2, 0),
            (0, 0, 2), (0, 0, -2),
            (1, 1, 1), (1, 1, -1), (1, -1, 1), (1, -1, -1),
            (-1, 1, 1), (-1, 1, -1), (-1, -1, 1), (-1, -1, -1),
        ]
        if 0 <= face_idx < len(offsets):
            di, dj, dk = offsets[face_idx]
            return LatticeCoord(self.i + di, self.j + dj, self.k + dk)
        raise ValueError(f"Invalid face index: {face_idx}")

    def parity(self) -> int:
        return (self.i + self.j + self.k) & 1

    def to_world(self, half_spacing: float = 79.055):
        return (self.i * half_spacing, self.j * half_spacing, self.k * half_spacing)


class ConstraintSolver:
    """Greedy constraint solver for VNES lattice placement."""

    def __init__(self):
        self._cells = {}  # LatticeCoord -> variant_id
        if _HAS_CPP:
            self._cpp_solver = _cpp.ConstraintSolver()
        else:
            self._cpp_solver = None

    def place_cell(self, i: int, j: int, k: int, variant_id: str) -> dict:
        coord = LatticeCoord(i, j, k)
        if coord in self._cells:
            return {"success": False, "error": "occupied"}
        if variant_id not in VARIANT_METADATA:
            return {"success": False, "error": "unknown variant"}

        # Check C++ acceleration
        if self._cpp_solver is not None:
            return dict(self._cpp_solver.place_cell(i, j, k, variant_id))

        # Pure-Python compatibility check
        md = lookup_variant(variant_id)
        if md is None:
            return {"success": False, "error": "unknown variant"}

        for face_idx in range(14):
            n_coord = coord.neighbor(face_idx)
            if n_coord in self._cells:
                n_vid = self._cells[n_coord]
                n_md = lookup_variant(n_vid)
                if n_md is None:
                    continue
                # Simple check: square-hex mismatch is always incompatible
                # For now, accept if both have the same face type
                f_a = md["square_face_role"] if face_idx < 6 else md["hex_face_role"]
                opposite = [1, 0, 3, 2, 5, 4,
                            13, 12, 11, 10, 9, 8, 7, 6]
                n_face = opposite[face_idx]
                f_b = (n_md["square_face_role"] if n_face < 6
                       else n_md["hex_face_role"])
                from .metadata import faces_compatible as fc
                if not fc(f_a, f_b):
                    return {
                        "success": False,
                        "error": "incompatible",
                        "conflicting_face": face_idx,
                        "conflicting_coord": (n_coord.i, n_coord.j, n_coord.k),
                    }

        self._cells[coord] = variant_id
        return {"success": True}

    @property
    def cell_count(self) -> int:
        if self._cpp_solver is not None:
            return self._cpp_solver.cell_count
        return len(self._cells)

    def reset(self):
        self._cells.clear()
        if self._cpp_solver is not None:
            self._cpp_solver.reset()

    def get_cells(self) -> dict:
        return dict(self._cells)


def place_lattice(width: int, height: int, depth: int,
                  variant_id: str = "VNE-A-250") -> ConstraintSolver:
    """Create a rectangular grid of VNES cells using BCC lattice."""
    solver = ConstraintSolver()

    for z in range(depth):
        for y in range(height):
            for x in range(width):
                result = solver.place_cell(x * 2, y * 2, z * 2, variant_id)
                if not result.get("success"):
                    raise RuntimeError(
                        f"Failed to place cell at ({x}, {y}, {z}): "
                        f"{result.get('error')}"
                    )
    return solver
