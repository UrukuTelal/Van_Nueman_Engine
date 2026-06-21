#pragma once
#include "SemanticCompiler.h"
#include "FLLGraphTraversal.h"
#include "Entity.h"
#include "HopfPID.h"
#include <cmath>

namespace vn {
namespace fll {

// ── ThoughtGlyphBridge ──────────────────────────────────────────────
// Maps a cognitive state (PillarStateVector or 512D thought vector)
// to an FLL fractal glyph tree via the SemanticCompiler.
//
// This is the final output stage of the cognitive pipeline: instead of
// emitting text tokens, the thought state manifests as a geometric glyph
// that evolves in real-time as the agent reasons.
//
// Pipeline:
//   PSV → ThoughtEngine → 512D thought vector → 64D embedding →
//   QuantumSeed → SemanticCompiler → FractalNode tree → Vulkan render
//
// The 512D thought vector maps to the 64D QuantumSeed embedding as:
//   - embedding[0..31]  = membrane (32D grounded cognitive state)
//   - embedding[32..63] = fiber downsampled (480D creative space → 32D)

struct ThoughtGlyphBridge {
    SemanticCompiler compiler;

    // ── Thought vector (512D) → 64D embedding ─────────────────
    // Membrane (dimensions 0-31) maps directly.
    // Fiber (dimensions 32-511) is stride-15 downsampled to 32 values.
    static void thought_to_embedding(
        const float thought[HOPF_FIBER_DIM],
        float embedding[64])
    {
        // First 32: membrane (grounded cognitive state)
        for (int i = 0; i < HOPF_BASE_DIM && i < 32; i++) {
            embedding[i] = thought[i];
        }

        // Next 32: fiber dimensions, stride-15 from position 32
        // 480 / 15 = 32 samples covering the creative space
        for (int i = 0; i < 32; i++) {
            int fiber_idx = HOPF_BASE_DIM + i * 15;
            if (fiber_idx < HOPF_FIBER_DIM) {
                embedding[32 + i] = thought[fiber_idx];
            } else {
                embedding[32 + i] = 0.5f;
            }
        }
    }

    // ── BCC spatial context → 64D embedding ──────────────────
    // Gathers PSV from agent position in BCC environment and
    // expands to 64D by repeating the 16 pillars.
    static void bcc_to_embedding(
        const PillarStateVector& spatial_psv,
        float embedding[64])
    {
        for (int i = 0; i < 64; i++) {
            embedding[i] = spatial_psv.pillars[i % NumPillars];
        }
    }

    // ── PSV → 64D embedding (zero-padded) ────────────────────
    static void psv_to_embedding(
        const PillarStateVector& psv,
        float embedding[64])
    {
        std::memset(embedding, 0, 64 * sizeof(float));
        for (int p = 0; p < NumPillars; p++) {
            embedding[p] = psv.pillars[p];
        }
    }

    // ── Build FLL tree from a 64D embedding ──────────────────
    // Creates a QuantumSeed via seed_from_embedding_auto(), then
    // builds the full fractal tree. Returns root node.
    FractalNode* build_from_embedding(
        float embedding[64],
        ArenaAllocator& arena,
        uint32_t max_depth = 6,
        float temperature = 0.15f,
        float resonance = 0.5f)
    {
        QuantumSeed seed = SemanticCompiler::seed_from_embedding_auto(
            embedding, 64);
        seed.encode_to_amplitudes();
        return compiler.build_from_seed(seed, arena, max_depth);
    }

    // ── Build FLL tree from a 512D thought vector ────────────
    FractalNode* build_from_thought(
        const float thought[HOPF_FIBER_DIM],
        ArenaAllocator& arena,
        uint32_t max_depth = 6)
    {
        float embed[64];
        thought_to_embedding(thought, embed);
        return build_from_embedding(embed, arena, max_depth);
    }

    // ── Build FLL tree from a PSV ────────────────────────────
    FractalNode* build_from_psv(
        const PillarStateVector& psv,
        ArenaAllocator& arena,
        uint32_t max_depth = 6)
    {
        float embed[64];
        psv_to_embedding(psv, embed);
        return build_from_embedding(embed, arena, max_depth);
    }

    // ── Update root geometry from thought (no rebuild) ───────
    // Updates only the root node's GeometricCoefficients from
    // the current thought vector. Children retain their fractal
    // structure from the original build. Call between rebuilds
    // for smooth visual updates.
    static void update_root_from_thought(
        FractalNode* root,
        const float thought[HOPF_FIBER_DIM])
    {
        if (!root) return;

        float embed[64];
        thought_to_embedding(thought, embed);

        // Generate a single iteration matrix for depth 0
        QuantumSeed seed = SemanticCompiler::seed_from_embedding_auto(embed, 64);
        IterationMatrix mat = SemanticCompiler().transform(seed, 0);

        // Apply to root node
        mat.apply_to(*root);
    }

    // ── Update root geometry from PSV (no rebuild) ───────────
    static void update_root_from_psv(
        FractalNode* root,
        const PillarStateVector& psv)
    {
        if (!root) return;

        float embed[64];
        psv_to_embedding(psv, embed);

        QuantumSeed seed = SemanticCompiler::seed_from_embedding_auto(embed, 64);
        IterationMatrix mat = SemanticCompiler().transform(seed, 0);
        mat.apply_to(*root);
    }
};

}} // namespace vn::fll
