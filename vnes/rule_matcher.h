#pragma once

#include "lattice.h"
#include <cstdint>
#include <utility>
#include <vector>

// ── Face compatibility check ───────────────────────────────
// Given two adjacent cells and the face of cell_a that connects
// to cell_b, determine if the interface is valid.
//
// The matching face on cell_b is derived from the geometry:
// if cell_a's face `f` faces toward cell_b, then cell_b's
// corresponding face index is VNES_OPPOSITE_FACE[f].
//
// Compatibility is determined by the FaceRole assigned to each
// face by its variant metadata, checked against the Interface
// Compatibility Matrix (faces_compatible in variant_registry.h).

inline bool can_connect(const LatticeCell& cell_a, uint8_t face_a,
                        const LatticeCell& cell_b)
{
    if (!cell_a.metadata || !cell_b.metadata) return false;

    uint8_t face_b = VNES_OPPOSITE_FACE[face_a];
    FaceRole role_a = cell_a.face_role(face_a);
    FaceRole role_b = cell_b.face_role(face_b);

    return faces_compatible(role_a, role_b);
}

// ── Overload taking coords (looks up cells from lattice) ───
inline bool can_connect(const Lattice& lattice,
                        LatticeCoord coord_a, uint8_t face_a,
                        LatticeCoord coord_b)
{
    const auto* a = lattice.cell_at(coord_a);
    const auto* b = lattice.cell_at(coord_b);
    if (!a || !b) return false;
    return can_connect(*a, face_a, *b);
}

// ── Neighborhood validation ────────────────────────────────
// Returns the number of incompatible neighbor connections
// around the cell at `coord`. Optionally populates `bad_faces`
// with (face_idx, neighbor_coord) pairs.

inline uint32_t validate_neighborhood(const Lattice& lattice,
                                       LatticeCoord coord,
                                       std::vector<std::pair<uint8_t, LatticeCoord>>* bad_faces = nullptr)
{
    const auto* cell = lattice.cell_at(coord);
    if (!cell) return 0;

    uint32_t incompat = 0;
    for (uint8_t f = 0; f < 14; f++) {
        LatticeCoord n_coord = coord.neighbor(f);
        const auto* n_cell = lattice.cell_at(n_coord);
        if (!n_cell) continue;

        if (!can_connect(*cell, f, *n_cell)) {
            incompat++;
            if (bad_faces)
                bad_faces->push_back({f, n_coord});
        }
    }
    return incompat;
}

// ── Full lattice validation ────────────────────────────────
// Returns the total number of incompatible connections in the
// entire lattice.

inline uint32_t validate_all(const Lattice& lattice)
{
    uint32_t total = 0;
    for (auto& [coord, cell] : lattice.cells()) {
        (void)cell;
        total += validate_neighborhood(lattice, coord);
    }
    return total / 2;  // each pair counted twice
}

// ── Find connecting face ───────────────────────────────────
// Returns the face index on `a` that connects to `b`, or 0xFF
// if the two coords are not adjacent.

inline uint8_t find_connecting_face(const LatticeCoord& a, const LatticeCoord& b)
{
    return a.face_to(b);
}
