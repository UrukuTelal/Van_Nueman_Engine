#pragma once
#include <cstdint>
#include <cmath>
#include <cstring>
#include <type_traits>

namespace vn {
namespace fll {

// ── Geometric Coefficients ──────────────────────────────────────────
// Each FractalNode encodes its visual glyph as a set of scale-invariant
// geometric coefficients. These define a resolution-independent vector
// glyph via procedural signed-distance-field construction.
//
// Target: fit in 2 GPU cache lines (128 bytes). We use 80 bytes for
// rich glyph definition with room for symmetry metadata.

struct alignas(16) GeometricCoefficients {
    float center_x;            // glyph center (normalized -1..1)
    float center_y;
    float rotation;            // radian rotation
    float scale;               // base scale factor

    float harmonic_a[4];       // 4 radial harmonic amplitudes
    float harmonic_p[4];       // 4 radial harmonic phase offsets

    float twist;               // angular twist per depth layer
    float asymmetry;           // asymmetry factor (0 = symmetric)
    float inner_radius;        // inner cutout radius
    float outer_radius;        // outer boundary radius

    uint32_t symmetry_n;       // rotational symmetry order
    float padding[3];          // explicit padding to 80 bytes
};

static_assert(sizeof(GeometricCoefficients) == 80,
    "GeometricCoefficients must be exactly 80 bytes");

// ── Quantum Resonance State ─────────────────────────────────────────
// Stores the quantum amplitude vector for Swap Test cross-node resonance.
// Two floats per complex amplitude: [re0, im0, re1, im1, ...].
// 2048 bytes amplitudes + 8 bytes metadata + 8 bytes pad = 2064 total.

struct alignas(16) QuantumResonanceState {
    static constexpr uint32_t MAX_QUBITS = 8;
    static constexpr uint32_t AMPLITUDE_PAIRS = 1u << MAX_QUBITS; // 256
    float amplitudes[AMPLITUDE_PAIRS * 2]; // 2048 bytes
    float fidelity;                        // 4 bytes
    uint32_t qubit_count;                  // 4 bytes
    float pad[2];                          // 8 bytes padding to 2064
};

static_assert(sizeof(QuantumResonanceState) == 2064,
    "QuantumResonanceState must be 2064 bytes (2048 + 16)");

// ── Semantic Descriptor ─────────────────────────────────────────────
// Dense latent embedding vector that serves as the "meaning" of this node.

struct alignas(16) SemanticDescriptor {
    static constexpr uint32_t EMBEDDING_DIM = 64;
    float embedding[EMBEDDING_DIM];
};

static_assert(sizeof(SemanticDescriptor) == 256,
    "SemanticDescriptor must be 256 bytes (64 floats)");

// ── FractalNode ─────────────────────────────────────────────────────
// Core 64-byte aligned node in the FLL graph. Each node represents a
// scale-invariant semantic glyph at a specific depth level.
// Children define the next recursive depth layer (M_{i-1}).
//
// Layout:
//   offset    0: uid (8) + depth (4) + child_count (4)  = 16
//   offset   16: GeometricCoefficients                   = 80  → 96
//   offset   96: SemanticDescriptor                      = 256 → 352
//   offset  352: QuantumResonanceState                   = 2064 → 2416
//   offset 2416: child_uids[8] (64) + parent_uid (8)    = 72  → 2488
//   offset 2488: zoom_thresholds (8) + flags (4) + meta (16) = 28 → 2516
//   padded to 64-byte alignment:                         = 2560

struct alignas(64) FractalNode {
    // ── Identity (16 bytes) ────────────────────────────────────
    uint64_t uid;
    uint32_t depth;
    uint32_t child_count;

    // ── Geometric Glyph Definition (80 bytes) ──────────────────
    GeometricCoefficients geometry;

    // ── Semantic Content (256 bytes) ───────────────────────────
    SemanticDescriptor semantics;

    // ── Quantum Resonance (2064 bytes) ─────────────────────────
    QuantumResonanceState resonance;

    // ── Child Pointers (72 bytes) ──────────────────────────────
    static constexpr uint32_t MAX_CHILDREN = 8;
    uint64_t child_uids[MAX_CHILDREN];
    uint64_t parent_uid;

    // ── Scale / Navigation (28 bytes) ──────────────────────────
    float zoom_threshold_min;
    float zoom_threshold_max;
    uint32_t flags;
    float metadata[4];

    // ── Methods ────────────────────────────────────────────────
    bool is_root() const { return parent_uid == 0; }
    bool has_children() const { return child_count > 0; }
    bool is_leaf() const { return child_count == 0; }

    uint32_t active_child_count() const {
        uint32_t count = 0;
        for (uint32_t i = 0; i < MAX_CHILDREN; i++) {
            if (child_uids[i] != 0) count++;
        }
        return count;
    }

    void clear_children() {
        std::memset(child_uids, 0, sizeof(child_uids));
        child_count = 0;
    }

    bool add_child(uint64_t child_uid) {
        for (uint32_t i = 0; i < MAX_CHILDREN; i++) {
            if (child_uids[i] == 0) {
                child_uids[i] = child_uid;
                child_count++;
                return true;
            }
        }
        return false;
    }
};

static_assert(sizeof(FractalNode) == 2560,
    "FractalNode must be exactly 2560 bytes");

// ── Node Flags ──────────────────────────────────────────────────────
enum NodeFlags : uint32_t {
    NODE_ACTIVE        = 1u << 0,
    NODE_DIRTY         = 1u << 1,
    NODE_RESONANT      = 1u << 2,
    NODE_COMPILED      = 1u << 3,
    NODE_RENDERABLE    = 1u << 4,
    NODE_ZOOMED        = 1u << 5,
    NODE_LOCKED        = 1u << 6,
};

// ── Scale-Invariant Depth Constants ──────────────────────────────────
// The FLL uses a geometric depth progression: each level i has scale
// factor phi^{-i} where phi is the golden ratio.

constexpr float FLL_PHI = 1.6180339887498948482045868343656f;
constexpr uint32_t FLL_MAX_DEPTH = 32;

inline float depth_to_scale(uint32_t depth) {
    return std::pow(FLL_PHI, -static_cast<float>(depth));
}

inline float depth_to_zoom_threshold_min(uint32_t depth) {
    return depth_to_scale(depth) * 0.5f;
}

inline float depth_to_zoom_threshold_max(uint32_t depth) {
    return depth_to_scale(depth) * 2.0f;
}

} // namespace fll
} // namespace vn
