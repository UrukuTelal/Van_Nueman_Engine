#pragma once

#include "rule_matcher.h"
#include <cstdio>
#include <vector>

// ── PlacementConstraint ────────────────────────────────────
// Describes a desired cell placement: position, variant (or
// role-based selection), and optional orientation/tag.
// If `required_metadata` is nullptr, the solver auto-selects
// the most appropriate variant.

struct PlacementConstraint {
    LatticeCoord coord;
    const VariantMetadata* required_metadata;
    uint8_t orientation;
    uint32_t tag;

    PlacementConstraint()
        : required_metadata(nullptr), orientation(0), tag(0) {}

    PlacementConstraint(LatticeCoord coord_, const VariantMetadata* md_,
                        uint8_t orientation_ = 0, uint32_t tag_ = 0)
        : coord(coord_), required_metadata(md_), orientation(orientation_), tag(tag_) {}
};

// ── PlacementResult ────────────────────────────────────────

enum class PlacementResult : uint8_t {
    Success,
    Occupied,
    IncompatibleNeighbor,
    InvalidVariant,
    NoSuitableVariant,
};

// ── ConstraintResult ───────────────────────────────────────
// Outcome of attempting a single placement.

struct ConstraintResult {
    LatticeCoord coord;
    PlacementResult result;
    const VariantMetadata* placed_metadata;
    uint8_t conflicting_face;
    LatticeCoord conflicting_coord;
    uint32_t neighbor_count;
    uint32_t incompat_count;

    ConstraintResult()
        : result(PlacementResult::Success)
        , placed_metadata(nullptr)
        , conflicting_face(0xFF)
        , neighbor_count(0)
        , incompat_count(0) {}
};

// ── ConstraintSolver ───────────────────────────────────────
// Greedy spatial constraint solver. Places cells one by one,
// checking compatibility with all existing neighbors.

class ConstraintSolver {
public:
    // Place a single cell. Validates against all existing neighbors.
    ConstraintResult place_cell(LatticeCoord coord, const VariantMetadata* md,
                                uint8_t orientation = 0, uint32_t tag = 0)
    {
        ConstraintResult cr;
        cr.coord = coord;

        if (!md) {
            cr.result = PlacementResult::InvalidVariant;
            return cr;
        }

        if (lattice_.is_occupied(coord)) {
            cr.result = PlacementResult::Occupied;
            return cr;
        }

        // Check compatibility with each existing neighbor
        uint32_t incompat = 0;
        for (uint8_t f = 0; f < 14; f++) {
            LatticeCoord n_coord = coord.neighbor(f);
            const auto* n_cell = lattice_.cell_at(n_coord);
            if (!n_cell) continue;

            LatticeCell temp_cell(coord, md, orientation);
            if (!can_connect(temp_cell, f, *n_cell)) {
                incompat++;
                if (cr.conflicting_face == 0xFF) {
                    cr.conflicting_face = f;
                    cr.conflicting_coord = n_coord;
                }
            }
        }

        if (incompat > 0) {
            cr.result = PlacementResult::IncompatibleNeighbor;
            cr.incompat_count = incompat;
            return cr;
        }

        // Place
        LatticeCell* inserted = lattice_.add_cell(
            LatticeCell(coord, md, orientation, tag));
        if (!inserted) {
            cr.result = PlacementResult::Occupied;
            return cr;
        }

        cr.result = PlacementResult::Success;
        cr.placed_metadata = md;

        auto nbrs = lattice_.neighbors(coord);
        cr.neighbor_count = (uint32_t)nbrs.size();
        return cr;
    }

    // ── Smart placement (auto-select variant) ─────────────
    // Attempts to place a cell at `coord`, trying all variants
    // until one is compatible with all existing neighbors.
    // Tries the preferred variant first if specified.

    ConstraintResult place_cell_auto(LatticeCoord coord,
                                     const VariantMetadata* preferred = nullptr,
                                     uint8_t orientation = 0, uint32_t tag = 0)
    {
        ConstraintResult cr;
        cr.coord = coord;

        if (lattice_.is_occupied(coord)) {
            cr.result = PlacementResult::Occupied;
            return cr;
        }

        const VariantMetadata* candidates[6];
        uint32_t n_candidates = 0;

        if (preferred)
            candidates[n_candidates++] = preferred;

        for (uint32_t v = 0; v < VNES_VARIANT_COUNT && n_candidates < 6; v++) {
            const VariantMetadata* md = VNES_ALL_VARIANTS[v];
            if (md && md != preferred)
                candidates[n_candidates++] = md;
        }

        for (uint32_t ci = 0; ci < n_candidates; ci++) {
            auto* md = candidates[ci];
            if (!md) continue;

            // Test compatibility
            bool ok = true;
            uint8_t bad_face = 0xFF;
            LatticeCoord bad_coord;
            uint32_t incompat = 0;

            for (uint8_t f = 0; f < 14; f++) {
                LatticeCoord n_coord = coord.neighbor(f);
                const auto* n_cell = lattice_.cell_at(n_coord);
                if (!n_cell) continue;

                LatticeCell temp_cell(coord, md, orientation);
                if (!can_connect(temp_cell, f, *n_cell)) {
                    incompat++;
                    if (bad_face == 0xFF) {
                        bad_face = f;
                        bad_coord = n_coord;
                    }
                    ok = false;
                    break;  // early exit, try next variant
                }
            }

            if (!ok) continue;

            // Compatible — place it
            LatticeCell* inserted = lattice_.add_cell(
                LatticeCell(coord, md, orientation, tag));
            if (!inserted) {
                cr.result = PlacementResult::Occupied;
                return cr;
            }

            cr.result = PlacementResult::Success;
            cr.placed_metadata = md;
            auto nbrs = lattice_.neighbors(coord);
            cr.neighbor_count = (uint32_t)nbrs.size();
            return cr;
        }

        cr.result = PlacementResult::NoSuitableVariant;
        return cr;
    }

    // ── Batch placement ───────────────────────────────────
    // Places cells from a constraint array. Returns count of
    // successfully placed cells.

    uint32_t plan_placement(const PlacementConstraint* constraints, uint32_t count,
                            bool abort_on_fail = false)
    {
        uint32_t placed = 0;
        for (uint32_t i = 0; i < count; i++) {
            const auto& c = constraints[i];
            ConstraintResult r;

            if (c.required_metadata) {
                r = place_cell(c.coord, c.required_metadata, c.orientation, c.tag);
            } else {
                r = place_cell_auto(c.coord, nullptr, c.orientation, c.tag);
            }

            if (r.result == PlacementResult::Success) {
                placed++;
            } else if (abort_on_fail) {
                break;
            }
        }
        return placed;
    }

    // Access
    Lattice& lattice() { return lattice_; }
    const Lattice& lattice() const { return lattice_; }
    void reset() { lattice_.clear(); }

    // Report
    void print_report() const {
        printf("Lattice: %llu cells, %u incompatibilities\n",
               (unsigned long long)lattice_.size(),
               (unsigned)validate_all(lattice_));
    }

private:
    Lattice lattice_;
};
