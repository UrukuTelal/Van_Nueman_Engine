#pragma once

#include "variant_types.h"

// ── Variant count ─────────────────────────────────────────
inline constexpr uint32_t VNES_VARIANT_COUNT = 5;

// ── All variants ──────────────────────────────────────────
// Extern declarations so they can be used from .cpp files
// without ODR issues. The definitions live in variant_types.h
// as constexpr, but this array provides runtime iteration.

extern const VariantMetadata* const VNES_ALL_VARIANTS[VNES_VARIANT_COUNT];

inline const VariantMetadata* lookup_variant(const VariantClassID& id) {
    for (uint32_t i = 0; i < VNES_VARIANT_COUNT; i++) {
        if (VNES_ALL_VARIANTS[i]->class_id == id)
            return VNES_ALL_VARIANTS[i];
    }
    return nullptr;
}

inline const VariantMetadata* lookup_variant(const char* id_str) {
    for (uint32_t i = 0; i < VNES_VARIANT_COUNT; i++) {
        VariantClassID candidate = VNES_ALL_VARIANTS[i]->class_id;
        if (std::memcmp(candidate.id, id_str, 16) == 0)
            return VNES_ALL_VARIANTS[i];
    }
    return nullptr;
}

inline const VariantMetadata* lookup_variant_by_role(bool load_bearing,
                                                      bool needs_root_zone,
                                                      bool needs_transport,
                                                      bool needs_harvest,
                                                      bool needs_utility) {
    // Priority-ordered lookup matching the Assembly Genome Concept.
    // Rules are evaluated highest-priority first.
    if (load_bearing)                     return &VNE_A_250;
    if (needs_transport)                  return &VNE_D_250;
    if (needs_root_zone)                  return &VNE_C_250;
    if (needs_harvest)                    return &VNE_B_250;
    if (needs_utility)                    return &VNE_E_250;
    return &VNE_A_250;  // default structural
}

// ── Face compatibility matrix ─────────────────────────────
// Returns true if two FaceRole values can mate.
// Implementation of the Interface Compatibility Matrix from
// the VNES specification.

inline bool faces_compatible(FaceRole a, FaceRole b) {
    // Normalize order (symmetric)
    uint8_t ia = static_cast<uint8_t>(a);
    uint8_t ib = static_cast<uint8_t>(b);
    if (ia > ib) { uint8_t t = ia; ia = ib; ib = t; }

    // Square-square interfaces
    if (ia == static_cast<uint8_t>(FaceRole::LoadBearing) &&
        ib == static_cast<uint8_t>(FaceRole::LoadBearing))
        return true;  // A–A full load bearing
    if (ia == static_cast<uint8_t>(FaceRole::LoadBearing) &&
        ib == static_cast<uint8_t>(FaceRole::StructuralFlange))
        return true;  // A–B/C structural flange
    if (ia == static_cast<uint8_t>(FaceRole::LoadBearing) &&
        ib == static_cast<uint8_t>(FaceRole::StaticStructural))
        return true;  // A–D static structural
    if (ia == static_cast<uint8_t>(FaceRole::LoadBearing) &&
        ib == static_cast<uint8_t>(FaceRole::StructuralSeal))
        return true;  // A–E sealed structural
    if (ia == static_cast<uint8_t>(FaceRole::StructuralFlange) &&
        ib == static_cast<uint8_t>(FaceRole::StructuralFlange))
        return true;  // B–C / C–C structural flange
    if (ia == static_cast<uint8_t>(FaceRole::StructuralFlange) &&
        ib == static_cast<uint8_t>(FaceRole::StaticStructural))
        return true;  // B/C–D
    if (ia == static_cast<uint8_t>(FaceRole::StructuralFlange) &&
        ib == static_cast<uint8_t>(FaceRole::StructuralSeal))
        return true;  // B/C–E
    if (ia == static_cast<uint8_t>(FaceRole::StaticStructural) &&
        ib == static_cast<uint8_t>(FaceRole::StaticStructural))
        return true;  // D–D
    if (ia == static_cast<uint8_t>(FaceRole::StaticStructural) &&
        ib == static_cast<uint8_t>(FaceRole::StructuralSeal))
        return true;  // D–E
    if (ia == static_cast<uint8_t>(FaceRole::StructuralSeal) &&
        ib == static_cast<uint8_t>(FaceRole::StructuralSeal))
        return true;  // E–E

    // Hex-hex interfaces (transport faces)
    if (ia >= 4 && ib >= 4) {
        // All hex faces are mutually compatible — they differ
        // only in flow characteristics, not mechanical fit.
        return true;
    }

    return false;  // square-hex mismatch
}

// ── Physics constants ─────────────────────────────────────
// Derived from the 250 mm circumscribed sphere.
// These are used by the simulation and SDF generator.

inline constexpr float VNES_CELL_DIAMETER_MM   = 250.0f;
inline constexpr float VNES_CELL_RADIUS_MM     = 125.0f;
inline constexpr float VNES_FRAME_THICKNESS_MM = 15.0f;
inline constexpr float VNES_EDGE_LENGTH_MM     = 79.06f;  // approximate
inline constexpr float VNES_VOLUME_LITERS      = 5.58f;   // approximate

// Snap-fit peg geometry
inline constexpr float VNES_PEG_DIAMETER_MM    = 8.0f;
inline constexpr float VNES_PEG_DEPTH_MM       = 12.0f;
inline constexpr float VNES_PEG_CLEARANCE_MM   = 0.25f;
inline constexpr uint32_t VNES_SOCKETS_PER_FACE = 4;

// Growth face openings (VNE-B)
inline constexpr float VNES_GROWTH_OPENING_MIN_MM = 20.0f;
inline constexpr float VNES_GROWTH_OPENING_MAX_MM = 30.0f;

// Marching cubes default resolution
inline constexpr uint32_t VNES_DEFAULT_GRID_RES = 128;  // cells per axis

// BCC lattice spacing (Track 3 — placement engine)
// Derived from the truncated octahedron geometry:
//   edge_length = 2*R/sqrt(5) ≈ 111.8mm
//   square-face center-to-center = sqrt(2)*edge_length ≈ 158.11mm
inline constexpr float VNES_CELL_SPACING_MM = 158.11f;
inline constexpr float VNES_HALF_SPACING_MM = 79.055f;
