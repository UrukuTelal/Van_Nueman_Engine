#pragma once
#include <cstdint>
#include "FractalNode.h"

namespace vn {
namespace fll {

// ── Push Constants: Zoom Matrix ─────────────────────────────────────
// These push constants control the camera zoom matrix that determines
// which depth levels of the FLL graph are visible and how transitions
// between scales are handled.

struct FLLPushConstants {
    // ── Camera / Navigation ────────────────────────────────────
    float view_center_x;            // camera target center (normalized)
    float view_center_y;
    float zoom_level;               // current zoom (1.0 = base)
    float time;                     // elapsed time for animations

    // ── Depth Transition ───────────────────────────────────────
    float depth_blend;              // blend factor for current depth transition 0..1
    float depth_fade;               // cross-fade between parent/child layers
    uint32_t active_depth;          // current active depth layer
    uint32_t max_display_depth;     // maximum depth to render

    // ── Rendering Controls ─────────────────────────────────────
    float resonance_threshold;      // minimum resonance to render a node
    float glyph_intensity;          // overall glyph brightness
    float color_hue_shift;          // hue shift for depth-based coloring
    uint32_t node_count;            // number of nodes in the render buffer

    // ── Debug / Overlay ────────────────────────────────────────
    float debug_show_edges;         // 0.0 = hide, 1.0 = show node connection edges
    float debug_show_quantum;       // 0.0 = hide, 1.0 = overlay quantum resonance
    float pad0;
    float pad1;
};

static_assert(sizeof(FLLPushConstants) == 64,
    "FLLPushConstants must be exactly 64 bytes (one VkPushConstantRange)");

// ── Descriptor Set Layout (current — only bindings 0 and 2 are in use) ──
// Binding 0: FractalNode buffer (storage buffer, readonly)
// Binding 2: Output image (storage image, RGBA32F)

enum FLLDescriptorBinding : uint32_t {
    FLL_BIND_NODE_BUFFER      = 0,
    FLL_BIND_OUTPUT_IMAGE     = 2,
    FLL_BIND_COUNT            = 3
};

// ── Vertex Input (if using full pipeline with vertex shader) ───────
// Not used in the compute-shader-only path. The glyph is rendered
// procedurally from a fullscreen quad.

// ── GPU FractalNode Layout (mirrors fll_glyph.comp) ─────────────────
// Packed 96-byte structure for GPU node buffer uploads.
// Matches the GLSL FractalNodeGPU struct exactly.
#pragma pack(push, 1)
struct FractalNodeGPU {
    uint64_t uid;                        // offset  0, size  8
    uint32_t depth;                      // offset  8, size  4
    uint32_t child_count;                // offset 12, size  4
    GeometricCoefficients geo;           // offset 16, size 80
};                                       // total  96 bytes
#pragma pack(pop)
static_assert(sizeof(FractalNodeGPU) == 96,
    "FractalNodeGPU must be exactly 96 bytes (matches shader stride)");

// ── Specialization Constants ───────────────────────────────────────
struct FLLSpecialization {
    uint32_t workgroup_size_x = 8;
    uint32_t workgroup_size_y = 8;
    uint32_t max_depth = 32;
    float    phi = 1.6180339887f;
};

} // namespace fll
} // namespace vn
