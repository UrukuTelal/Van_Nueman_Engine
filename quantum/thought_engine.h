#pragma once

#include "wht_tokenizer.h"
#include "bcc_thought_env.h"
#include "../include/HopfPID.h"
#include <cstdlib>

namespace vn {
namespace quantum {

// ── ThoughtEngine ──────────────────────────────────────────
// Hopf-PID cognitive loop with explicit access to the 480D
// fiber space for creative divergence.
//
// The 512D thought vector has two subspaces:
//   - Base (dimensions 0-31):  the 32D membrane that grounds
//     cognition in the pillar state
//   - Fiber (dimensions 32-511): the 480D nullspace where
//     divergent/creative patterns emerge
//
// Pipeline:
//   PSV → WHT Tokenizer → load frames → Hopf encode →
//   diverge (fiber injection) → Hopf tick → project → output PSV

struct ThoughtEngine {
    HopfPIDState state;
    HopfProjectionMatrix proj;
    WHTTokenizer tokenizer;
    float creativity;    // [0,1] fiber divergence strength
    float coherence;     // [0,1] fiber coherence toward base

    // Optional BCC spatial environment for grounded cognition
    BCCThoughtEnvironment* bcc_env;
    bool use_spatial;

    ThoughtEngine() : creativity(0.1f), coherence(0.9f), bcc_env(nullptr), use_spatial(false) {
        state.init();
        proj.init_hopf();
    }

    // Attach a BCC thought environment for spatial context
    void set_bcc_env(BCCThoughtEnvironment* env) {
        bcc_env = env;
        use_spatial = (env != nullptr);
    }

    // Initialize all 32 frames from a PSV (direct continuous path)
    void init_from_psv(const PillarStateVector& psv) {
        for (int f = 0; f < HOPF_FRAME_COUNT; f++) {
            for (int p = 0; p < NumPillars; p++) {
                state.frames[f][p] = psv.pillars[p];
            }
        }
        state.encode_all_frames();
        rebuild_membrane();
    }

    // Initialize from a WHT token (discrete token path)
    void init_from_token(int token_id) {
        PillarStateVector token_psv = tokenizer.decode(token_id);
        init_from_psv(token_psv);
    }

    // Initialize from a PSV via the tokenizer (token-constrained path)
    // This discretizes the input through the vocabulary before loading
    void init_from_psv_tokenized(const PillarStateVector& psv) {
        int token_id = tokenizer.encode(psv);
        init_from_token(token_id);
    }

    // ── Creative Divergence ─────────────────────────────────
    // Inject variation into the 480D fiber dimensions of the
    // thought vector. Higher strength = more creative exploration.
    // The fiber holds 30 blocks × 16 pillars = 480 degrees of
    // freedom where emergent patterns can form independently of
    // the 32D base membrane.

    void diverge(float strength) {
        if (strength <= 0.0f) return;

        for (int f = 0; f < HOPF_FRAME_COUNT; f++) {
            for (int p = 0; p < NumPillars; p++) {
                // Apply small Bloch rotation to each pillar in each frame.
                // The rotation direction and magnitude vary per frame/pillar
                // to create divergent patterns in the fiber space.
                float phase = 2.0f * 3.14159265f *
                    ((float)(f * NumPillars + p) / (float)(HOPF_FRAME_COUNT * NumPillars));
                float noise = (float)(rand() % 1000) / 1000.0f - 0.5f;
                float delta = strength * (0.5f * std::sin(phase) + 0.5f * noise);
                vn::fp20_t current(state.frames[f][p]);
                state.frames[f][p] = bloch_rotate(current, vn::fp20_t(delta)).to_float();
            }
        }
        state.encode_all_frames();
        rebuild_membrane();
    }

    // ── Creative Convergence ────────────────────────────────
    // Pull fibers back toward the base membrane. Higher rate means
    // faster convergence to grounded cognition.

    void converge(float rate) {
        if (rate <= 0.0f) return;

        for (int f = 0; f < HOPF_FRAME_COUNT; f++) {
            for (int p = 0; p < NumPillars; p++) {
                float current = state.frames[f][p];
                // Base reference: the membrane projected back to pillar space
                // For now, use frame 0 as the anchor (the "fundamental" frame)
                float anchor = state.frames[0][p];
                float theta_current = bloch_value_to_theta(vn::fp20_t(current)).to_float();
                float theta_anchor = bloch_value_to_theta(vn::fp20_t(anchor)).to_float();
                float pull = (theta_anchor - theta_current) * rate;
                if (std::fabs(pull) > 1e-8f) {
                    state.frames[f][p] = bloch_rotate(
                        vn::fp20_t(current), vn::fp20_t(pull)
                    ).to_float();
                }
            }
        }
        state.encode_all_frames();
        rebuild_membrane();
    }

    // ── Hopf Tick ───────────────────────────────────────────
    // Run one full Hopf-PID cognitive cycle.
    // Manages state internally (no entity-to-entity coupling).

    void tick(float dt) {
        // ── Spatial Phase ────────────────────────────────────
        // If spatial environment is active, scatter the current
        // thought into the BCC grid before processing, then
        // gather spatial context to influence the tick.

        if (use_spatial && bcc_env) {
            PillarStateVector current_psv = output_psv();
            bcc_env->store_at_agent(current_psv);
            bcc_env->scatter(current_psv, 1);

            // Gather spatial context and blend with current state
            PillarStateVector spatial_context = bcc_env->gather(2);
            for (int p = 0; p < NumPillars; p++) {
                float blended = state.frames[0][p] * 0.7f + spatial_context.pillars[p] * 0.3f;
                for (int f = 0; f < HOPF_FRAME_COUNT; f++) {
                    float theta_cur = bloch_value_to_theta(vn::fp20_t(state.frames[f][p])).to_float();
                    float theta_ctx = bloch_value_to_theta(vn::fp20_t(blended)).to_float();
                    float pull = (theta_ctx - theta_cur) * 0.1f;
                    if (std::fabs(pull) > 1e-8f) {
                        state.frames[f][p] = bloch_rotate(
                            vn::fp20_t(state.frames[f][p]), vn::fp20_t(pull)
                        ).to_float();
                    }
                }
            }
            state.encode_all_frames();
        }

        // ── Core PID Phase ───────────────────────────────────
        state.relational_complexity = compute_relational_complexity(state);

        if (detect_rupture_condition(state)) {
            trigger_rupture(state);
            return;
        }

        rebuild_membrane();

        if (creativity > 0.0f) {
            diverge(creativity * dt * 0.1f);
        }
        if (coherence < 1.0f) {
            converge((1.0f - coherence) * dt * 0.1f);
        }

        // ── Encode Spatial Phase ─────────────────────────────
        // After processing, encode the full thought vector into
        // the BCC grid for persistent spatial memory.
        if (use_spatial && bcc_env) {
            float thought[HOPF_FIBER_DIM];
            get_thought(thought);
            bcc_env->encode_thought(thought);
        }
    }

    // ── Full Cognitive Cycle ────────────────────────────────
    // Initialize from input PSV, run N ticks with creative
    // divergence, then project to output PSV.

    PillarStateVector think(
        const PillarStateVector& input,
        int ticks = 1,
        float dt = 0.016f,
        bool use_tokenizer = false,
        BCCThoughtEnvironment* spatial = nullptr
    ) {
        if (spatial) set_bcc_env(spatial);
        if (use_tokenizer) {
            init_from_psv_tokenized(input);
        } else {
            init_from_psv(input);
        }

        // First divergence seeds the creative space
        if (creativity > 0.0f && ticks > 0) {
            diverge(creativity);
        }

        for (int i = 0; i < ticks; i++) {
            tick(dt);
        }

        return output_psv();
    }

    // ── Output ──────────────────────────────────────────────
    // Project current membrane → PSV

    PillarStateVector output_psv() const {
        PillarStateVector result;
        decode_pillar_vector(state.membrane, result.pillars);
        for (int i = 0; i < NumPillars; i++) {
            if (result.pillars[i] < 0.0f) result.pillars[i] = 0.0f;
            if (result.pillars[i] > 1.0f) result.pillars[i] = 1.0f;
        }
        return result;
    }

    // Access the full 512D thought vector
    void get_thought(float thought[HOPF_FIBER_DIM]) const {
        for (int f = 0; f < HOPF_FRAME_COUNT; f++) {
            for (int p = 0; p < NumPillars; p++) {
                thought[f * NumPillars + p] = state.frames[f][p];
            }
        }
    }

    // Access the 32D membrane
    void get_membrane(float membrane[HOPF_BASE_DIM]) const {
        std::memcpy(membrane, state.membrane, HOPF_BASE_DIM * sizeof(float));
    }

    // Get the creative divergence in the fiber space:
    // measures std-dev across frames for each pillar
    float fiber_divergence() const {
        float total_var = 0.0f;
        for (int p = 0; p < NumPillars; p++) {
            float mean = 0.0f;
            for (int f = 0; f < HOPF_FRAME_COUNT; f++) {
                mean += state.frames[f][p];
            }
            mean /= (float)HOPF_FRAME_COUNT;
            float var = 0.0f;
            for (int f = 0; f < HOPF_FRAME_COUNT; f++) {
                float d = state.frames[f][p] - mean;
                var += d * d;
            }
            total_var += var / (float)HOPF_FRAME_COUNT;
        }
        return std::sqrt(total_var / (float)NumPillars);
    }

private:
    void rebuild_membrane() {
        float thought[HOPF_FIBER_DIM];
        get_thought(thought);
        proj.project(thought, state.membrane);
    }
};

}} // namespace vn::quantum
