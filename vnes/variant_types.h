#pragma once

#include <cstdint>
#include <cstring>

// ── Face roles ────────────────────────────────────────────
// Each face on a Kelvin cell (6 square, 8 hexagonal) has a role
// determined by the variant. The compatibility matrix in the VNES
// spec defines which face pairs can mate.

enum class FaceRole : uint8_t {
    // Square-face roles
    LoadBearing       = 0,   // VNE-A: full structural support
    StructuralFlange  = 1,   // VNE-B/C: flange for square-face connection
    StaticStructural  = 2,   // VNE-D: rigid but not load-bearing primary
    StructuralSeal    = 3,   // VNE-E: sealed structural interface

    // Hex-face roles
    CapillaryFlow     = 4,   // VNE-A: wicking / moisture transport
    FluidAirExchange  = 5,   // VNE-B: airflow + water exchange
    HydroponicFlow    = 6,   // VNE-C: nutrient solution flow
    MainVascularPath  = 7,   // VNE-D: primary conduit trunk
    UtilityTrunk      = 8,   // VNE-E: cable / sensor / service trunk
};

// ── Biological support flags ──────────────────────────────
// Bitmask of biological colonization types a variant supports.

enum class BioSupport : uint8_t {
    None         = 0,
    Mycelium     = 1 << 0,
    Bacteria     = 1 << 1,
    PlantRoots   = 1 << 2,
    Invertebrates = 1 << 3,
};

constexpr BioSupport operator|(BioSupport a, BioSupport b) {
    return static_cast<BioSupport>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
constexpr BioSupport operator&(BioSupport a, BioSupport b) {
    return static_cast<BioSupport>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}
constexpr bool has_flag(BioSupport value, BioSupport flag) {
    return (static_cast<uint8_t>(value) & static_cast<uint8_t>(flag)) != 0;
}

// ── Flow characteristics ──────────────────────────────────
// What can pass through this variant's interior.

struct FlowCharacteristics {
    bool airflow          : 1;
    bool waterflow        : 1;
    bool root_passage     : 1;
    bool mycelium_passage : 1;
};

// ── Pore geometry ─────────────────────────────────────────
// The interior TPMS (triply periodic minimal surface) pore
// size range, in mm.

struct PoreGeometry {
    float min_size_mm;   // smallest pore diameter
    float max_size_mm;   // largest pore diameter
    float distribution;  // 0 = uniform, 1 = gradient toward growth face
};

// ── Conduit spec (VNE-D) ──────────────────────────────────
// Vascular trunk geometry for the Transport Spine variant.

struct ConduitSpec {
    uint32_t count;          // number of trunks (3 for VNE-D)
    float diameter_mm;       // trunk diameter
    float junction_radius;   // Steinmetz blend radius at intersection
};

// ── Utility spec (VNE-E) ──────────────────────────────────
// Corridor and mount geometry for the Utility Service variant.

struct SensorMount {
    float x_mm, y_mm, z_mm; // position relative to cell center
    float diameter_mm;       // sensor bore diameter
    const char* sensor_type; // null-terminated name string
};

struct UtilitySpec {
    uint32_t cable_conduits;       // number of cable channels
    float cable_conduit_diameter_mm;
    uint32_t fluid_channel_count;
    float fluid_channel_diameter_mm;
    uint32_t sensor_mount_count;
    SensorMount sensor_mounts[8];  // fixed max for constexpr
    bool has_inspection_port;
    float inspection_port_diameter_mm;
};

// ── Variant class ID ──────────────────────────────────────
// Fixed-width 16-byte identifier, e.g. "VNE-A-250\0...".

struct VariantClassID {
    char id[16];

    bool operator==(const VariantClassID& other) const {
        return std::memcmp(id, other.id, 16) == 0;
    }
    bool operator!=(const VariantClassID& other) const {
        return !(*this == other);
    }
};

// ── Variant metadata ──────────────────────────────────────
// Complete description of one VNES cell variant. Every field
// is constexpr-ready for compile-time instantiation.

struct VariantMetadata {
    VariantClassID class_id;

    // Structural
    bool load_bearing;
    float compressive_priority;  // 0..1
    float fluid_priority;        // 0..1

    // Biological
    BioSupport bio_support;

    // Pores
    PoreGeometry pore;

    // Flow
    FlowCharacteristics flow;

    // Face roles
    FaceRole square_face_role;
    FaceRole hex_face_role;

    // Optional subsystems (default-initialized to "none")
    bool has_conduits;
    ConduitSpec conduit;

    bool has_utility;
    UtilitySpec utility;

    // Manufacturing
    float frame_thickness_mm;
    bool supportless;
    float min_wall_mm;
    float min_feature_mm;
};

// ── Variant A: Structural Core Cell ───────────────────────
// Triply periodic Schwarz P minimal surface.
// Load-bearing skeleton for the ecosystem.
// Square = LoadBearing, Hex = CapillaryFlow.

inline constexpr VariantMetadata VNE_A_250 = {
    VariantClassID{"VNE-A-250"},
    true,          // load_bearing
    0.95f,         // compressive_priority
    0.15f,         // fluid_priority
    BioSupport::Mycelium | BioSupport::Bacteria,  // bio_support
    {1.0f, 3.0f, 0.0f},  // pore (uniform)
    {false, true, false, true},  // flow: water + mycelium
    FaceRole::LoadBearing,
    FaceRole::CapillaryFlow,
    false,         // has_conduits
    {0, 0.0f, 0.0f},
    false,         // has_utility
    {},
    15.0f,         // frame_thickness_mm
    true,          // supportless
    1.2f,          // min_wall_mm
    0.8f,          // min_feature_mm
};

// ── Variant B: Harvest Surface Cell ───────────────────────
// Density-gradient gyroid with open growth-face chambers.
// Square = StructuralFlange, Hex = FluidAirExchange.

inline constexpr VariantMetadata VNE_B_250 = {
    VariantClassID{"VNE-B-250"},
    false,         // load_bearing
    0.40f,         // compressive_priority
    0.60f,         // fluid_priority
    BioSupport::Mycelium | BioSupport::Bacteria |
        BioSupport::PlantRoots | BioSupport::Invertebrates,
    {3.0f, 6.0f, 1.0f},  // pore (gradient toward growth face)
    {true, true, false, true},  // flow: air + water + mycelium
    FaceRole::StructuralFlange,
    FaceRole::FluidAirExchange,
    false,
    {0, 0.0f, 0.0f},
    false,
    {},
    15.0f,
    true,
    1.2f,
    0.8f,
};

// ── Variant C: Universal Rooting Cell ─────────────────────
// Open-cell gyroid architecture for hydroponics and microbial
// habitat. Square = StructuralFlange, Hex = HydroponicFlow.

inline constexpr VariantMetadata VNE_C_250 = {
    VariantClassID{"VNE-C-250"},
    false,
    0.40f,
    0.60f,
    BioSupport::Mycelium | BioSupport::Bacteria |
        BioSupport::PlantRoots | BioSupport::Invertebrates,
    {3.0f, 6.0f, 0.0f},  // pore (uniform, 3-6 mm)
    {true, true, true, true},  // flow: all
    FaceRole::StructuralFlange,
    FaceRole::HydroponicFlow,
    false,
    {0, 0.0f, 0.0f},
    false,
    {},
    15.0f,
    true,
    1.2f,
    0.8f,
};

// ── Variant D: Transport Spine Cell ───────────────────────
// Three intersecting 20 mm vascular trunks with gyroid support
// lattice. Square = StaticStructural, Hex = MainVascularPath.

inline constexpr ConduitSpec VNE_D_CONDUITS = {3, 20.0f, 4.0f};

inline constexpr VariantMetadata VNE_D_250 = {
    VariantClassID{"VNE-D-250"},
    false,
    0.30f,
    0.90f,
    BioSupport::Mycelium | BioSupport::Bacteria,
    {3.0f, 6.0f, 0.0f},
    {true, true, false, true},
    FaceRole::StaticStructural,
    FaceRole::MainVascularPath,
    true,          // has_conduits
    VNE_D_CONDUITS,
    false,
    {},
    15.0f,
    true,
    1.2f,
    0.8f,
};

// ── Variant E: Utility Service Cell ───────────────────────
// Gyroid lattice with dedicated cable conduits, fluid channels,
// and sensor mounting interfaces.
// Square = StructuralSeal, Hex = UtilityTrunk.

inline constexpr UtilitySpec VNE_E_UTILITY = {
    4,              // cable_conduits
    8.0f,           // cable_conduit_diameter_mm
    2,              // fluid_channel_count
    6.0f,           // fluid_channel_diameter_mm
    6,              // sensor_mount_count
    {               // sensor_mounts[8]
        {-40.0f, -40.0f, 0.0f, 12.0f, "moisture"},
        {-40.0f,  40.0f, 0.0f, 12.0f, "temperature"},
        { 40.0f, -40.0f, 0.0f, 12.0f, "humidity"},
        { 40.0f,  40.0f, 0.0f, 12.0f, "pH"},
        {  0.0f, -60.0f, 0.0f, 12.0f, "flow"},
        {  0.0f,  60.0f, 0.0f, 12.0f, "CO2"},
    },
    true,           // has_inspection_port
    30.0f,          // inspection_port_diameter_mm
};

inline constexpr VariantMetadata VNE_E_250 = {
    VariantClassID{"VNE-E-250"},
    false,
    0.25f,
    0.50f,
    BioSupport::Mycelium | BioSupport::Bacteria,
    {3.0f, 6.0f, 0.0f},
    {true, true, false, true},
    FaceRole::StructuralSeal,
    FaceRole::UtilityTrunk,
    false,
    {0, 0.0f, 0.0f},
    true,           // has_utility
    VNE_E_UTILITY,
    15.0f,
    true,
    1.2f,
    0.8f,
};
