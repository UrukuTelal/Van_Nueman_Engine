#pragma once
#include "FractalNode.h"
#include "QuantumSeed.h"
#include "ArenaAllocator.h"
#include "FLLShaders.h"
#include "FLLGraphTraversal.h"
#include <cstdint>
#include <vector>
#include <cmath>

namespace vn {
namespace fll {

// ── FLL Renderer ────────────────────────────────────────────────────
// Manages the Vulkan rendering pipeline for fractal glyph visualization.
//
// Responsibilities:
//   - Maintains the GPU node buffer (mapped from arena)
//   - Updates push constants (zoom matrix, depth transition, time)
//   - Interfaces with the engine's Vulkan renderer for descriptor sets
//   - Manages the zoom matrix: as zoom changes, transitions between
//     depth layers seamlessly

class FLLRenderer {
public:
    FLLRenderer();
    ~FLLRenderer();

    FLLRenderer(const FLLRenderer&) = delete;
    FLLRenderer& operator=(const FLLRenderer&) = delete;

    // ── Lifecycle ─────────────────────────────────────────────
    bool init(ArenaAllocator* arena);
    void shutdown();

    bool is_initialized() const { return initialized_; }

    // ── Per-frame update ──────────────────────────────────────
    void update(float dt, float mouse_x, float mouse_y);

    // ── Camera / Navigation ───────────────────────────────────
    void set_view_center(float x, float y);
    void set_zoom_level(float zoom);
    void zoom_to(float target_zoom, float duration_sec = 0.5f);
    void set_active_depth(uint32_t depth);

    // ── Push constant access ──────────────────────────────────
    const FLLPushConstants& push_constants() const { return pc_; }
    FLLPushConstants& push_constants() { return pc_; }

    // ── Node buffer access ────────────────────────────────────
    FractalNode** render_list() { return render_list_; }
    uint32_t render_list_count() const { return render_list_count_; }
    void rebuild_render_list();

    // ── Zoom Matrix ───────────────────────────────────────────
    // The zoom matrix maps viewing scale to depth layer.
    // As zoom increases, deeper (higher-depth) nodes become visible.
    // Transitions are cross-faded using depth_blend.
    struct ZoomMatrix {
        float base_zoom;           // starting zoom level
        float target_zoom;         // target zoom level (animated)
        float transition_t;        // normalized transition progress [0..1]
        float transition_speed;    // transition speed multiplier

        uint32_t min_depth;        // minimum depth currently visible
        uint32_t max_depth;        // maximum depth currently visible
        float depth_exit_scale;    // scale at which a depth layer exits
        float depth_enter_scale;   // scale at which a depth layer enters
    };

    const ZoomMatrix& zoom_matrix() const { return zoom_; }
    ZoomMatrix& zoom_matrix() { return zoom_; }

    void update_zoom_matrix(float dt);

    // ── Stats ─────────────────────────────────────────────────
    struct FrameStats {
        uint32_t visible_nodes;
        uint32_t rendered_pixels;
        float    avg_glyph_eval_us;
        uint32_t active_depth;
        float    current_zoom;
    };

    const FrameStats& frame_stats() const { return stats_; }

private:
    bool initialized_ = false;

    ArenaAllocator* arena_ = nullptr;

    // ── Render list ───────────────────────────────────────────
    static constexpr uint32_t MAX_RENDER_NODES = 65536;
    FractalNode* render_list_[MAX_RENDER_NODES];
    uint32_t render_list_count_ = 0;

    // ── Push constants (updated per frame) ────────────────────
    FLLPushConstants pc_{};

    // ── Zoom matrix state ─────────────────────────────────────
    ZoomMatrix zoom_{};

    // ── Animation state ───────────────────────────────────────
    float time_ = 0.0f;
    float camera_x_ = 0.0f;
    float camera_y_ = 0.0f;
    float current_zoom_ = 1.0f;

    // ── Frame stats ───────────────────────────────────────────
    FrameStats stats_{};
};

// ── Implementation ─────────────────────────────────────────────────

inline FLLRenderer::FLLRenderer() {
    std::memset(render_list_, 0, sizeof(render_list_));
    std::memset(&pc_, 0, sizeof(pc_));
    std::memset(&zoom_, 0, sizeof(zoom_));

    // Default push constants
    pc_.resonance_threshold = 0.1f;
    pc_.glyph_intensity     = 1.0f;
    pc_.color_hue_shift     = 0.0f;
    pc_.max_display_depth   = FLL_MAX_DEPTH;
    pc_.depth_blend         = 0.0f;
    pc_.depth_fade          = 1.0f;
    pc_.debug_show_edges    = 0.0f;
    pc_.debug_show_quantum  = 0.0f;
    pc_.view_center_x       = 0.0f;
    pc_.view_center_y       = 0.0f;
    pc_.zoom_level          = 1.0f;
    pc_.time                = 0.0f;
    pc_.active_depth        = 0;

    // Default zoom matrix
    zoom_.base_zoom         = 1.0f;
    zoom_.target_zoom       = 1.0f;
    zoom_.transition_t      = 1.0f;
    zoom_.transition_speed  = 2.0f;
    zoom_.min_depth         = 0;
    zoom_.max_depth         = 3;
    zoom_.depth_exit_scale  = 0.5f;
    zoom_.depth_enter_scale = 2.0f;
}

inline FLLRenderer::~FLLRenderer() {
    shutdown();
}

inline bool FLLRenderer::init(ArenaAllocator* arena) {
    if (initialized_) return false;
    if (!arena || !arena->is_initialized()) return false;

    arena_ = arena;
    initialized_ = true;
    return true;
}

inline void FLLRenderer::shutdown() {
    initialized_ = false;
    arena_ = nullptr;
    render_list_count_ = 0;
}

inline void FLLRenderer::set_view_center(float x, float y) {
    camera_x_ = x;
    camera_y_ = y;
    pc_.view_center_x = x;
    pc_.view_center_y = y;
}

inline void FLLRenderer::set_zoom_level(float zoom) {
    zoom_.base_zoom = current_zoom_;
    zoom_.target_zoom = zoom;
    zoom_.transition_t = 0.0f;
}

inline void FLLRenderer::zoom_to(float target_zoom, float duration_sec) {
    zoom_.base_zoom = current_zoom_;
    zoom_.target_zoom = target_zoom;
    zoom_.transition_t = 0.0f;
    // Convert duration to speed (completing in duration_sec at 60fps)
    zoom_.transition_speed = 1.0f / (duration_sec * 60.0f);
}

inline void FLLRenderer::set_active_depth(uint32_t depth) {
    pc_.active_depth = depth;
    if (depth > pc_.max_display_depth) {
        pc_.max_display_depth = depth + 2;
    }
}

inline void FLLRenderer::rebuild_render_list() {
    if (!arena_) return;

    render_list_count_ = 0;

    FractalNode* root = arena_->get_node(0);
    if (!root) {
        pc_.node_count = 0;
        stats_.visible_nodes = 0;
        return;
    }

    TraversalFilter filter;
    filter.min_depth = zoom_.min_depth;
    filter.max_depth = zoom_.max_depth;
    filter.require_flags = NODE_ACTIVE | NODE_RENDERABLE;

    render_list_count_ = collect_nodes(*arena_, root, render_list_,
                                        MAX_RENDER_NODES,
                                        TraversalOrder::BFS, filter);

    pc_.node_count = render_list_count_;
    stats_.visible_nodes = render_list_count_;
}

inline void FLLRenderer::update_zoom_matrix(float dt) {
    // Animate zoom transition
    if (zoom_.transition_t < 1.0f) {
        zoom_.transition_t += dt * 60.0f * zoom_.transition_speed;
        if (zoom_.transition_t > 1.0f) zoom_.transition_t = 1.0f;

        // Ease-in-out cubic
        float t = zoom_.transition_t;
        float eased = t * t * (3.0f - 2.0f * t);
        current_zoom_ = zoom_.base_zoom + (zoom_.target_zoom - zoom_.base_zoom) * eased;
    }

    pc_.zoom_level = current_zoom_;

    // Determine active depth from zoom level
    // log_phi(zoom) gives the depth layer
    float depth_f = std::log(current_zoom_) / std::log(FLL_PHI);
    int32_t depth_i = static_cast<int32_t>(std::round(depth_f));
    if (depth_i < 0) depth_i = 0;
    if (depth_i > static_cast<int32_t>(FLL_MAX_DEPTH))
        depth_i = static_cast<int32_t>(FLL_MAX_DEPTH);

    pc_.active_depth = static_cast<uint32_t>(depth_i);

    // Depth blend: fractional part for cross-fade
    float fractional = depth_f - std::floor(depth_f);
    pc_.depth_blend = fractional;
    pc_.depth_fade  = 1.0f;

    // Update visible depth range
    uint32_t half_range = 3;
    zoom_.min_depth = (pc_.active_depth > half_range)
                        ? pc_.active_depth - half_range : 0;
    zoom_.max_depth = pc_.active_depth + half_range;
    if (zoom_.max_depth > FLL_MAX_DEPTH) zoom_.max_depth = FLL_MAX_DEPTH;

    pc_.max_display_depth = zoom_.max_depth;
}

inline void FLLRenderer::update(float dt, float mouse_x, float mouse_y) {
    if (!initialized_) return;

    time_ += dt;
    pc_.time = time_;

    update_zoom_matrix(dt);

    rebuild_render_list();

    // Update stats
    stats_.active_depth = pc_.active_depth;
    stats_.current_zoom = current_zoom_;
}

} // namespace fll
} // namespace vn
