#pragma once

#include "variant_registry.h"
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

// ── Face classification ────────────────────────────────────
// A Kelvin (truncated octahedron) cell has 14 faces:
//   6 square faces (indices 0-5) — cardinal directions
//   8 hexagonal faces (indices 6-13) — diagonal directions

inline constexpr uint8_t VNES_FACE_COUNT         = 14;
inline constexpr uint8_t VNES_SQUARE_FACE_COUNT  = 6;
inline constexpr uint8_t VNES_HEX_FACE_COUNT     = 8;

inline bool is_square_face(uint8_t face_idx) { return face_idx < 6; }
inline bool is_hex_face(uint8_t face_idx)   { return face_idx >= 6; }

// ── Neighbor offset table ──────────────────────────────────
// Offsets in BCC lattice coordinates (i, j, k) for each face.
// Face 0-5 (square): ±2 along one axis.
// Face 6-13 (hex): ±1 along all three axes.

inline constexpr int8_t VNES_FACE_OFFSETS[14][3] = {
    { 2, 0, 0},   //  0: +X square
    {-2, 0, 0},   //  1: -X square
    { 0, 2, 0},   //  2: +Y square
    { 0,-2, 0},   //  3: -Y square
    { 0, 0, 2},   //  4: +Z square
    { 0, 0,-2},   //  5: -Z square
    { 1, 1, 1},   //  6: (+X,+Y,+Z) hex
    { 1, 1,-1},   //  7: (+X,+Y,-Z) hex
    { 1,-1, 1},   //  8: (+X,-Y,+Z) hex
    { 1,-1,-1},   //  9: (+X,-Y,-Z) hex
    {-1, 1, 1},   // 10: (-X,+Y,+Z) hex
    {-1, 1,-1},   // 11: (-X,+Y,-Z) hex
    {-1,-1, 1},   // 12: (-X,-Y,+Z) hex
    {-1,-1,-1},   // 13: (-X,-Y,-Z) hex
};

// Opposite-face mapping: the face index on the neighbor cell
// that corresponds to the shared face.
inline constexpr uint8_t VNES_OPPOSITE_FACE[14] = {
    1, 0, 3, 2, 5, 4,     // square: 0↔1, 2↔3, 4↔5
    13, 12, 11, 10, 9, 8, 7, 6  // hex: 6↔13, 7↔12, 8↔11, 9↔10
};

// ── LatticeCoord ───────────────────────────────────────────
// Integer coordinates in the BCC (body-centered cubic) lattice.
// All integer (i,j,k) are valid. Parity = (i+j+k)%2 distinguishes
// the two BCC sublattices.
//
// World position: (i * VNES_HALF_SPACING_MM, j * VNES_HALF_SPACING_MM, k * VNES_HALF_SPACING_MM)
//
// Square faces connect cells of the SAME parity (offset ±2).
// Hex faces connect cells of OPPOSITE parity (offset ±1 in each axis).

struct LatticeCoord {
    int32_t i, j, k;

    LatticeCoord() : i(0), j(0), k(0) {}
    LatticeCoord(int32_t i_, int32_t j_, int32_t k_) : i(i_), j(j_), k(k_) {}

    bool operator==(const LatticeCoord& o) const { return i == o.i && j == o.j && k == o.k; }
    bool operator!=(const LatticeCoord& o) const { return !(*this == o); }

    bool operator<(const LatticeCoord& o) const {
        if (i != o.i) return i < o.i;
        if (j != o.j) return j < o.j;
        return k < o.k;
    }

    // Convert to world-space position in mm
    void to_world(float& x, float& y, float& z) const {
        x = i * VNES_HALF_SPACING_MM;
        y = j * VNES_HALF_SPACING_MM;
        z = k * VNES_HALF_SPACING_MM;
    }

    // Parity: 0 = sublattice A (even), 1 = sublattice B (odd)
    int32_t parity() const { return (i + j + k) & 1; }

    // Neighbor coordinate in the given face direction
    LatticeCoord neighbor(uint8_t face_idx) const {
        return LatticeCoord(
            i + VNES_FACE_OFFSETS[face_idx][0],
            j + VNES_FACE_OFFSETS[face_idx][1],
            k + VNES_FACE_OFFSETS[face_idx][2]
        );
    }

    // Find which face of THIS cell connects to `other`.
    // Returns 0-13 on success, 0xFF if not adjacent.
    uint8_t face_to(const LatticeCoord& other) const {
        int32_t di = other.i - i;
        int32_t dj = other.j - j;
        int32_t dk = other.k - k;
        for (uint8_t f = 0; f < 14; f++) {
            if (VNES_FACE_OFFSETS[f][0] == di &&
                VNES_FACE_OFFSETS[f][1] == dj &&
                VNES_FACE_OFFSETS[f][2] == dk)
                return f;
        }
        return 0xFF;
    }
};

// ── LatticeCoord hash ──────────────────────────────────────
// Packs i,j,k into a 64-bit key (21 bits per component).

struct LatticeCoordHash {
    size_t operator()(const LatticeCoord& c) const {
        return (size_t)(
            ((uint64_t)(uint32_t)c.i << 42) |
            ((uint64_t)(uint32_t)c.j << 21) |
            ((uint64_t)(uint32_t)c.k)
        );
    }
};

// ── LatticeCell ────────────────────────────────────────────
// One VNES cell placed in the lattice.

struct LatticeCell {
    LatticeCoord coord;
    const VariantMetadata* metadata;
    uint8_t orientation;      // 0 = default; reserved for octahedral rotation
    uint32_t tag;             // user-defined grouping ID

    LatticeCell()
        : metadata(nullptr), orientation(0), tag(0) {}

    LatticeCell(LatticeCoord coord_, const VariantMetadata* md_,
                uint8_t orientation_ = 0, uint32_t tag_ = 0)
        : coord(coord_), metadata(md_), orientation(orientation_), tag(tag_) {}

    // FaceRole for a given face index (from the variant metadata)
    FaceRole face_role(uint8_t face_idx) const {
        return is_square_face(face_idx)
            ? metadata->square_face_role
            : metadata->hex_face_role;
    }
};

// ── Lattice container ──────────────────────────────────────
// Hash map of cells keyed by LatticeCoord.

class Lattice {
public:
    using CellMap = std::unordered_map<LatticeCoord, LatticeCell, LatticeCoordHash>;

    // Add a cell. Returns pointer to the inserted cell, or nullptr if occupied.
    LatticeCell* add_cell(const LatticeCell& cell) {
        auto [it, inserted] = cells_.insert({cell.coord, cell});
        return inserted ? &it->second : nullptr;
    }

    // Remove a cell. Returns true if it existed.
    bool remove_cell(LatticeCoord coord) {
        return cells_.erase(coord) > 0;
    }

    // Query by coordinate (returns nullptr if not found)
    const LatticeCell* cell_at(LatticeCoord coord) const {
        auto it = cells_.find(coord);
        return it != cells_.end() ? &it->second : nullptr;
    }

    LatticeCell* cell_at(LatticeCoord coord) {
        auto it = cells_.find(coord);
        return it != cells_.end() ? &it->second : nullptr;
    }

    // Check if a coordinate is occupied
    bool is_occupied(LatticeCoord coord) const {
        return cells_.find(coord) != cells_.end();
    }

    // Per-face neighbor query
    struct FaceNeighbor {
        uint8_t face_idx;
        LatticeCell* cell;
    };

    std::vector<FaceNeighbor> neighbors(LatticeCoord coord) {
        std::vector<FaceNeighbor> result;
        auto it = cells_.find(coord);
        if (it == cells_.end()) return result;
        for (uint8_t f = 0; f < 14; f++) {
            LatticeCoord n = coord.neighbor(f);
            auto nit = cells_.find(n);
            if (nit != cells_.end())
                result.push_back({f, &nit->second});
        }
        return result;
    }

    // Iteration
    const CellMap& cells() const { return cells_; }
    size_t size() const { return cells_.size(); }
    void clear() { cells_.clear(); }

private:
    CellMap cells_;
};
