#pragma once
#include "FractalNode.h"
#include "QuantumSeed.h"
#include "ArenaAllocator.h"
#include <cstdint>
#include <cmath>
#include <cstring>
#include <cfloat>

namespace vn {
namespace fll {

// ── Iteration Matrix ─────────────────────────────────────────────────
// M = f(S, i) — the pure mathematical output of the condensation
// transformation. Fully determines the geometry of a FractalNode at
// depth i-1 from a QuantumSeed S at depth i.
//
// Scale-invariance property:
//   transform(S, i).scale == phi^{-i}
//   transform(S, i).harmonic_a[k] / transform(S, i-1).harmonic_a[k]
//     ≈ phi^{-1} · exp(i · twist_delta)  (structural self-similarity)
//
// Layout: geometric glyph (80 bytes) + depth context (16 bytes) = 96
// 16-byte aligned for SIMD-friendly access.

struct alignas(16) IterationMatrix {
    // ── Geometric Glyph ──────────────────────────────────────────
    float center_x, center_y;
    float rotation;
    float scale;

    float harmonic_a[4];
    float harmonic_p[4];

    float twist;
    float asymmetry;
    float inner_radius;
    float outer_radius;

    uint32_t symmetry_n;

    // ── Depth Context (reuses GeometricCoefficients padding + extra) ─
    uint32_t depth;               // child depth this matrix defines
    float    condensation_norm;   // L2 norm of condensation output
    float    structural_entropy;  // -Σ(p_k · log p_k) over harmonic energies
    uint32_t flags;

    // ── Apply to a FractalNode ───────────────────────────────────
    void apply_to(FractalNode& node) const {
        node.geometry.center_x      = center_x;
        node.geometry.center_y      = center_y;
        node.geometry.rotation      = rotation;
        node.geometry.scale         = scale;
        for (uint32_t k = 0; k < 4; k++) {
            node.geometry.harmonic_a[k] = harmonic_a[k];
            node.geometry.harmonic_p[k] = harmonic_p[k];
        }
        node.geometry.twist        = twist;
        node.geometry.asymmetry    = asymmetry;
        node.geometry.inner_radius = inner_radius;
        node.geometry.outer_radius = outer_radius;
        node.geometry.symmetry_n   = symmetry_n;
        node.depth                 = depth;
        node.zoom_threshold_min    = depth_to_zoom_threshold_min(depth);
        node.zoom_threshold_max    = depth_to_zoom_threshold_max(depth);
    }

    // ── Structural self-similarity with another matrix ───────────
    // Returns a value in [0,1]: 1 = perfectly scale-invariant,
    // 0 = completely different structure.
    float self_similarity(const IterationMatrix& other) const {
        float e_sum = 0.0f, o_sum = 0.0f;
        for (uint32_t k = 0; k < 4; k++) {
            e_sum += harmonic_a[k];
            o_sum += other.harmonic_a[k];
        }
        if (e_sum < FLT_MIN) e_sum = 1.0f;
        if (o_sum < FLT_MIN) o_sum = 1.0f;

        float dot = 0.0f, en = 0.0f, on = 0.0f;
        for (uint32_t k = 0; k < 4; k++) {
            float e_norm = harmonic_a[k] / e_sum;
            float o_norm = other.harmonic_a[k] / o_sum;
            dot += e_norm * o_norm;
            en  += e_norm * e_norm;
            on  += o_norm * o_norm;
        }

        float cos_angle = dot / (std::sqrt(en) * std::sqrt(on) + FLT_MIN);
        if (cos_angle < 0.0f) cos_angle = 0.0f;
        if (cos_angle > 1.0f) cos_angle = 1.0f;

        float depth_delta = static_cast<float>(
            static_cast<int>(depth) - static_cast<int>(other.depth));
        float ideal_scale_ratio = std::pow(FLL_PHI, std::abs(depth_delta));
        float actual_ratio = (scale + FLT_MIN) / (other.scale + FLT_MIN);
        if (actual_ratio < 1.0f) actual_ratio = 1.0f / actual_ratio;
        float scale_factor = (actual_ratio < ideal_scale_ratio * 2.0f &&
                              actual_ratio > ideal_scale_ratio * 0.5f) ? 1.0f : 0.5f;

        return cos_angle * scale_factor;
    }
};

static_assert(sizeof(IterationMatrix) == 96,
    "IterationMatrix must be 96 bytes (80 geometric + 16 context)");

// ── Embedding Statistics ─────────────────────────────────────────────
// Descriptive statistics for a raw embedding vector, used for
// preprocessing before condensation.

struct EmbeddingStats {
    float mean;
    float variance;
    float stddev;
    float min_val;
    float max_val;
    float sparsity;     // fraction of values near zero
    float energy;        // L2 norm squared
    uint32_t dim;

    static EmbeddingStats compute(const float* embedding, uint32_t dim) {
        EmbeddingStats s;
        s.dim = dim;
        s.min_val = FLT_MAX;
        s.max_val = -FLT_MAX;
        s.mean = 0.0f;
        s.energy = 0.0f;

        uint32_t near_zero = 0;
        for (uint32_t i = 0; i < dim; i++) {
            float v = embedding[i];
            s.mean += v;
            s.energy += v * v;
            if (v < s.min_val) s.min_val = v;
            if (v > s.max_val) s.max_val = v;
            if (std::abs(v) < 1e-4f) near_zero++;
        }
        s.mean /= static_cast<float>(dim);
        s.sparsity = static_cast<float>(near_zero) / static_cast<float>(dim);

        s.variance = 0.0f;
        for (uint32_t i = 0; i < dim; i++) {
            float d = embedding[i] - s.mean;
            s.variance += d * d;
        }
        s.variance /= static_cast<float>(dim);
        s.stddev = std::sqrt(s.variance);

        return s;
    }
};

// ── Semantic Compiler (Condensation Layer) ─────────────────────────
//
// The condensation transformation:
//   f(S, i) → M_{i-1}
//
// Given a Quantum/Semantic Seed S at depth level i, deterministically
// generates the iteration matrix M for the next recursive depth layer
// (i-1). The mapping decomposes into three stages:
//
//   1. Harmonic Extraction — latent vector projected onto orthogonal
//      harmonic basis via Gaussian-weighted coupling matrix W_{kj}
//
//   2. Depth Modulation — harmonic coefficients scaled by phi^{-i}
//      and phase-twisted by the golden-angle depth rotation
//
//   3. Glyph Assembly — modulated harmonics packed into geometric
//      coefficients with derived radii, symmetry, and twist
//
// Scale invariance:
//   f(S, i) = phi^{-1} · R(i · phi^{-1}) · f(S, i-1)
//   where R(θ) applies a phase rotation to each harmonic component.
//   This ensures self-similar structure across depth layers.

class SemanticCompiler {
public:
    static constexpr uint32_t MAX_HARMONICS = 4;
    static constexpr float    MIN_SCALE     = 1e-6f;
    static constexpr float    INV_PHI       = 1.0f / FLL_PHI;
    static constexpr uint32_t MAX_EMBED_DIM = QuantumSeed::MAX_LATENT_DIM;

    SemanticCompiler() = default;

    // ── Pure Transformation ──────────────────────────────────────
    // f(S, i) → M_{i-1}
    // Accepts a QuantumSeed and a target depth, returns the
    // deterministic iteration matrix for that depth.
    IterationMatrix transform(const QuantumSeed& seed, uint32_t child_depth) const;

    // ── Primary Condensation ─────────────────────────────────────
    // Given a parent QuantumSeed and depth, fill in a child
    // FractalNode's geometry and semantics. Delegates to transform().
    void condense(const QuantumSeed& seed,
                  FractalNode& child_node,
                  uint32_t child_depth) const;

    // ── Batch Condensation ───────────────────────────────────────
    // Generate all children for a parent node from a single seed.
    void condense_children(const QuantumSeed& seed,
                           FractalNode& parent_node,
                           ArenaAllocator& arena) const;

    // ── Full Graph Construction ──────────────────────────────────
    // From a root seed, recursively build the full FLL node tree
    // up to max_depth. Returns root node pointer.
    FractalNode* build_from_seed(const QuantumSeed& root_seed,
                                  ArenaAllocator& arena,
                                  uint32_t max_depth = FLL_MAX_DEPTH) const;

    // ── Seed from raw embedding ──────────────────────────────────
    // Convert a raw float embedding vector into a QuantumSeed,
    // auto-selecting qubit count and kernel params based on
    // embedding statistics.
    static QuantumSeed seed_from_embedding(
        const float* embedding,
        uint32_t dim,
        uint32_t depth = 0,
        float temperature = 0.15f,
        float resonance = 0.5f);

    // ── Seed from embedding with automatic pre-processing ────────
    // Normalizes and analyzes the embedding, then creates a seed
    // with tuned parameters based on embedding statistics.
    static QuantumSeed seed_from_embedding_auto(
        const float* embedding,
        uint32_t dim,
        uint32_t depth = 0);

    // ── Embedding Preprocessing ──────────────────────────────────
    // In-place L2 normalization of embedding vector.
    static void normalize_l2(float* embedding, uint32_t dim);

    // In-place min-max normalization to [0, 1].
    static void normalize_minmax(float* embedding, uint32_t dim);

    // Compute descriptive statistics for an embedding.
    static EmbeddingStats compute_stats(const float* embedding, uint32_t dim);

    // Extract dominant frequency components via naive DFT.
    // Only first half of output coefficients are meaningful.
    static void extract_spectral_features(const float* embedding, uint32_t dim,
                                           float* spectrum_out, uint32_t spectrum_dim);

    // ── Scale Invariance Verification ────────────────────────────
    // Compare two iteration matrices at different depths and return
    // a self-similarity score ∈ [0,1]. 1.0 means the structural
    // pattern is perfectly preserved across the depth transition.
    static float check_scale_invariance(const IterationMatrix& m_deeper,
                                         const IterationMatrix& m_shallower);

    // Verify the full condensation chain f(S, i) preserves structure
    // across consecutive depths. Returns average self-similarity.
    float verify_condensation_chain(const QuantumSeed& seed,
                                     uint32_t start_depth,
                                     uint32_t chain_length) const;

private:
    // ── Stage 1: Harmonic Extraction ────────────────────────────
    // H_k = Σ_j W_{kj} · latent_j · exp(ij·0.1)
    void extract_harmonics(const QuantumSeed& seed,
                           float harmonics_real[MAX_HARMONICS],
                           float harmonics_imag[MAX_HARMONICS]) const;

    // ── Stage 2: Depth Modulation ───────────────────────────────
    // H'_k = phi^{-i} · R(i · phi^{-1}) · H_k + noise(τ, k, i)
    void modulate_harmonics(float harmonics_real[MAX_HARMONICS],
                            float harmonics_imag[MAX_HARMONICS],
                            uint32_t depth,
                            float temperature,
                            float phase) const;

    // ── Stage 3: Glyph Assembly ─────────────────────────────────
    // Pack modulated harmonics + depth into GeometricCoefficients.
    void assemble_glyph(const float harmonics_real[MAX_HARMONICS],
                        const float harmonics_imag[MAX_HARMONICS],
                        uint32_t depth,
                        IterationMatrix& mat) const;

    // ── Helper: compute harmonic coupling weight ────────────────
    // W_{kj} = exp(-(k-j)² · (0.5 + 2δ)) · (1 + ρ · cos((k+j)·φ))
    static float harmonic_weight(uint32_t k, uint32_t j,
                                  float decay, float resonance);
};

// ═════════════════════════════════════════════════════════════════════
// IMPLEMENTATION
// ═════════════════════════════════════════════════════════════════════

// ── Pure Transformation: f(S, i) → M_{i-1} ─────────────────────────

inline IterationMatrix SemanticCompiler::transform(
    const QuantumSeed& seed, uint32_t child_depth) const
{
    float harmonics_real[MAX_HARMONICS];
    float harmonics_imag[MAX_HARMONICS];

    // Stage 1: Project latent vector onto harmonic basis
    extract_harmonics(seed, harmonics_real, harmonics_imag);

    // Stage 2: Scale and phase-twist for target depth
    modulate_harmonics(harmonics_real, harmonics_imag,
                       child_depth,
                       seed.kernel_temperature,
                       seed.kernel_phase);

    // Stage 3: Pack into iteration matrix
    IterationMatrix mat;
    assemble_glyph(harmonics_real, harmonics_imag, child_depth, mat);

    // Compute condensation norms
    float l2 = 0.0f;
    for (uint32_t k = 0; k < MAX_HARMONICS; k++) {
        l2 += harmonics_real[k] * harmonics_real[k]
            + harmonics_imag[k] * harmonics_imag[k];
    }
    mat.condensation_norm = std::sqrt(l2);

    // Structural entropy: -Σ p_k log p_k over normalized harmonic energies
    float e_total = 0.0f;
    for (uint32_t k = 0; k < MAX_HARMONICS; k++) e_total += mat.harmonic_a[k];
    if (e_total < FLT_MIN) e_total = 1.0f;
    float entropy = 0.0f;
    for (uint32_t k = 0; k < MAX_HARMONICS; k++) {
        float p = mat.harmonic_a[k] / e_total;
        if (p > FLT_MIN) entropy -= p * std::log(p);
    }
    mat.structural_entropy = entropy;
    mat.flags = 0;

    return mat;
}

// ── Condense (delegates to transform) ───────────────────────────────

inline void SemanticCompiler::condense(
    const QuantumSeed& seed,
    FractalNode& child_node,
    uint32_t child_depth) const
{
    IterationMatrix mat = transform(seed, child_depth);
    mat.apply_to(child_node);

    // Copy semantic descriptor from seed
    uint32_t copy_dim = seed.latent_dim < SemanticDescriptor::EMBEDDING_DIM
                          ? seed.latent_dim : SemanticDescriptor::EMBEDDING_DIM;
    std::memcpy(child_node.semantics.embedding, seed.latent_vector,
                copy_dim * sizeof(float));

    child_node.flags = NODE_ACTIVE | NODE_COMPILED | NODE_RENDERABLE;
}

// ── Batch Children ──────────────────────────────────────────────────

inline void SemanticCompiler::condense_children(
    const QuantumSeed& seed,
    FractalNode& parent_node,
    ArenaAllocator& arena) const
{
    const uint32_t child_depth = parent_node.depth + 1;
    if (child_depth > FLL_MAX_DEPTH) return;

    uint32_t n_children = (parent_node.geometry.symmetry_n / 2) + 1;
    if (n_children > FractalNode::MAX_CHILDREN) {
        n_children = FractalNode::MAX_CHILDREN;
    }

    parent_node.clear_children();

    for (uint32_t i = 0; i < n_children; i++) {
        FractalNode* child = arena.alloc_node();
        if (!child) break;

        QuantumSeed child_seed = seed;
        float offset = static_cast<float>(i) / static_cast<float>(n_children);
        child_seed.kernel_phase = seed.kernel_phase + offset * 6.2831853071f;
        child_seed.kernel_resonance = seed.kernel_resonance * (0.5f + offset * 0.5f);
        child_seed.seed_depth = child_depth;

        condense(child_seed, *child, child_depth);

        child->parent_uid = parent_node.uid;
        parent_node.add_child(child->uid);
    }
}

// ── Full Graph Construction ─────────────────────────────────────────

inline FractalNode* SemanticCompiler::build_from_seed(
    const QuantumSeed& root_seed,
    ArenaAllocator& arena,
    uint32_t max_depth) const
{
    if (!arena.is_initialized()) return nullptr;

    FractalNode* root = arena.alloc_node();
    if (!root) return nullptr;

    condense(root_seed, *root, 0);

    static constexpr uint32_t MAX_QUEUE = 4096;
    FractalNode* queue[MAX_QUEUE];
    uint32_t head = 0, tail = 0;

    queue[tail++] = root;

    while (head < tail && tail < MAX_QUEUE) {
        FractalNode* current = queue[head++];

        if (current->depth >= max_depth) continue;
        if (current->depth >= FLL_MAX_DEPTH) continue;

        QuantumSeed node_seed = SemanticCompiler::seed_from_embedding(
            current->semantics.embedding,
            SemanticDescriptor::EMBEDDING_DIM,
            current->depth,
            root_seed.kernel_temperature,
            root_seed.kernel_resonance
        );
        node_seed.encode_to_amplitudes();
        std::memcpy(current->resonance.amplitudes, node_seed.amplitudes,
                    node_seed.amplitude_count() * 2 * sizeof(float));
        current->resonance.qubit_count = node_seed.qubit_count;

        condense_children(node_seed, *current, arena);

        for (uint32_t i = 0; i < FractalNode::MAX_CHILDREN; i++) {
            if (current->child_uids[i] != 0) {
                FractalNode* child = arena.get_node_by_uid(current->child_uids[i]);
                if (child && tail < MAX_QUEUE) {
                    queue[tail++] = child;
                }
            }
        }
    }

    return root;
}

// ── Seed from raw embedding ─────────────────────────────────────────

inline QuantumSeed SemanticCompiler::seed_from_embedding(
    const float* embedding,
    uint32_t dim,
    uint32_t depth,
    float temperature,
    float resonance)
{
    QuantumSeed seed;
    std::memset(&seed, 0, sizeof(seed));

    uint32_t copy_dim = dim < QuantumSeed::MAX_LATENT_DIM
                          ? dim : QuantumSeed::MAX_LATENT_DIM;
    std::memcpy(seed.latent_vector, embedding, copy_dim * sizeof(float));
    seed.latent_dim = copy_dim;

    uint32_t pairs = (copy_dim + 1) / 2;
    seed.qubit_count = 1;
    while ((1u << seed.qubit_count) < pairs && seed.qubit_count < 8) {
        seed.qubit_count++;
    }

    seed.kernel_temperature    = temperature;
    seed.kernel_resonance      = resonance;
    seed.kernel_phase          = static_cast<float>(depth) * INV_PHI * 6.2831853071f;
    seed.kernel_harmonic_decay = 0.3f + resonance * 0.4f;
    seed.seed_depth            = depth;
    seed.parent_seed_id        = 0;
    seed.flags                 = 0;

    return seed;
}

// ── Seed from embedding with auto preprocessing ─────────────────────

inline QuantumSeed SemanticCompiler::seed_from_embedding_auto(
    const float* embedding,
    uint32_t dim,
    uint32_t depth)
{
    // Normalize embedding
    float normalized[MAX_EMBED_DIM];
    uint32_t cd = dim < MAX_EMBED_DIM ? dim : MAX_EMBED_DIM;
    std::memcpy(normalized, embedding, cd * sizeof(float));
    normalize_l2(normalized, cd);

    // Analyze statistics
    EmbeddingStats stats = compute_stats(normalized, cd);

    // Tune kernel parameters from statistics
    float temperature = stats.stddev < 0.1f ? 0.3f : 0.15f;
    float resonance   = 0.3f + stats.energy * 0.3f;
    if (resonance > 0.9f) resonance = 0.9f;

    return seed_from_embedding(normalized, cd, depth, temperature, resonance);
}

// ── Embedding Preprocessing ─────────────────────────────────────────

inline void SemanticCompiler::normalize_l2(float* embedding, uint32_t dim) {
    float norm = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        norm += embedding[i] * embedding[i];
    }
    norm = std::sqrt(norm);
    if (norm > FLT_MIN) {
        float inv = 1.0f / norm;
        for (uint32_t i = 0; i < dim; i++) embedding[i] *= inv;
    }
}

inline void SemanticCompiler::normalize_minmax(float* embedding, uint32_t dim) {
    float mn = FLT_MAX, mx = -FLT_MAX;
    for (uint32_t i = 0; i < dim; i++) {
        if (embedding[i] < mn) mn = embedding[i];
        if (embedding[i] > mx) mx = embedding[i];
    }
    float range = mx - mn;
    if (range > FLT_MIN) {
        float inv = 1.0f / range;
        for (uint32_t i = 0; i < dim; i++) {
            embedding[i] = (embedding[i] - mn) * inv;
        }
    }
}

inline EmbeddingStats SemanticCompiler::compute_stats(
    const float* embedding, uint32_t dim)
{
    return EmbeddingStats::compute(embedding, dim);
}

inline void SemanticCompiler::extract_spectral_features(
    const float* embedding, uint32_t dim,
    float* spectrum_out, uint32_t spectrum_dim)
{
    uint32_t n = dim < spectrum_dim * 2 ? dim : spectrum_dim * 2;
    for (uint32_t k = 0; k < spectrum_dim; k++) {
        float re = 0.0f, im = 0.0f;
        for (uint32_t j = 0; j < n; j++) {
            float angle = -2.0f * 3.1415926535f * static_cast<float>(k * j)
                        / static_cast<float>(n);
            re += embedding[j] * std::cos(angle);
            im += embedding[j] * std::sin(angle);
        }
        spectrum_out[k] = std::sqrt(re * re + im * im) / static_cast<float>(n);
    }
}

// ── Scale Invariance Verification ───────────────────────────────────

inline float SemanticCompiler::check_scale_invariance(
    const IterationMatrix& m_deeper,
    const IterationMatrix& m_shallower)
{
    return m_deeper.self_similarity(m_shallower);
}

inline float SemanticCompiler::verify_condensation_chain(
    const QuantumSeed& seed,
    uint32_t start_depth,
    uint32_t chain_length) const
{
    if (chain_length < 2) return 1.0f;

    float total_sim = 0.0f;
    uint32_t count = 0;

    IterationMatrix prev = transform(seed, start_depth);
    for (uint32_t i = 1; i < chain_length; i++) {
        IterationMatrix cur = transform(seed, start_depth + i);
        total_sim += check_scale_invariance(cur, prev);
        count++;
        prev = cur;
    }

    return total_sim / static_cast<float>(count);
}

// ── Stage 1: Harmonic Extraction ────────────────────────────────────

inline void SemanticCompiler::extract_harmonics(
    const QuantumSeed& seed,
    float harmonics_real[MAX_HARMONICS],
    float harmonics_imag[MAX_HARMONICS]) const
{
    std::memset(harmonics_real, 0, MAX_HARMONICS * sizeof(float));
    std::memset(harmonics_imag, 0, MAX_HARMONICS * sizeof(float));

    const float decay = seed.kernel_harmonic_decay;
    const float res  = seed.kernel_resonance;
    const uint32_t dim = (seed.latent_dim < MAX_EMBED_DIM)
                           ? seed.latent_dim : MAX_EMBED_DIM;

    for (uint32_t k = 0; k < MAX_HARMONICS; k++) {
        for (uint32_t j = 0; j < dim; j++) {
            float w = harmonic_weight(k, j, decay, res);
            float v = seed.latent_vector[j];
            harmonics_real[k] += w * v * std::cos(static_cast<float>(j) * 0.1f);
            harmonics_imag[k] += w * v * std::sin(static_cast<float>(j) * 0.1f);
        }
    }
}

// ── Harmonic Weight ─────────────────────────────────────────────────

inline float SemanticCompiler::harmonic_weight(uint32_t k, uint32_t j,
                                                 float decay, float resonance) {
    float dx = static_cast<float>(static_cast<int>(k) - static_cast<int>(j));
    float g = std::exp(-dx * dx * (0.5f + decay * 2.0f));
    float r = 1.0f + resonance * std::cos(static_cast<float>(k + j) * FLL_PHI);
    return g * r;
}

// ── Stage 2: Depth Modulation ───────────────────────────────────────

inline void SemanticCompiler::modulate_harmonics(
    float harmonics_real[MAX_HARMONICS],
    float harmonics_imag[MAX_HARMONICS],
    uint32_t depth,
    float temperature,
    float phase) const
{
    const float scale = depth_to_scale(depth);
    const float twist = phase + static_cast<float>(depth) * INV_PHI;

    float ct = std::cos(twist);
    float st = std::sin(twist);

    for (uint32_t k = 0; k < MAX_HARMONICS; k++) {
        float re = harmonics_real[k];
        float im = harmonics_imag[k];

        harmonics_real[k] = (re * ct - im * st) * scale;
        harmonics_imag[k] = (re * st + im * ct) * scale;

        if (temperature > MIN_SCALE) {
            float noise = temperature * (static_cast<float>(k) * 0.001f);
            harmonics_real[k] += noise * std::cos(static_cast<float>(k) * FLL_PHI * 2.0f);
            harmonics_imag[k] += noise * std::sin(static_cast<float>(k) * FLL_PHI * 2.0f);
        }
    }
}

// ── Stage 3: Glyph Assembly ─────────────────────────────────────────

inline void SemanticCompiler::assemble_glyph(
    const float harmonics_real[MAX_HARMONICS],
    const float harmonics_imag[MAX_HARMONICS],
    uint32_t depth,
    IterationMatrix& mat) const
{
    std::memset(&mat, 0, sizeof(IterationMatrix));

    float phase = static_cast<float>(depth) * INV_PHI;

    mat.center_x = harmonics_real[0] * 0.5f + 0.5f;
    mat.center_y = harmonics_imag[0] * 0.5f + 0.5f;

    if (mat.center_x < -0.9f) mat.center_x = -0.9f;
    if (mat.center_x >  0.9f) mat.center_x =  0.9f;
    if (mat.center_y < -0.9f) mat.center_y = -0.9f;
    if (mat.center_y >  0.9f) mat.center_y =  0.9f;

    mat.rotation = phase * 6.2831853071f;
    mat.scale    = depth_to_scale(depth);
    if (mat.scale < MIN_SCALE) mat.scale = MIN_SCALE;

    for (uint32_t k = 0; k < MAX_HARMONICS; k++) {
        mat.harmonic_a[k] = std::sqrt(harmonics_real[k] * harmonics_real[k]
                                    + harmonics_imag[k] * harmonics_imag[k]);
        mat.harmonic_p[k] = std::atan2(harmonics_imag[k], harmonics_real[k]);
    }

    mat.twist     = phase;
    mat.asymmetry = harmonics_real[1] * harmonics_real[1];

    float r0 = mat.harmonic_a[0];
    if (r0 < MIN_SCALE) r0 = 0.1f;
    mat.inner_radius = std::abs(harmonics_real[2]) * 0.3f + 0.05f;
    mat.outer_radius = r0 * 0.5f + 0.2f;
    if (mat.inner_radius >= mat.outer_radius) {
        mat.inner_radius = mat.outer_radius * 0.3f;
    }

    float sym = std::abs(harmonics_real[3]) * 8.0f + 1.0f;
    if (sym < 1.0f) sym = 1.0f;
    if (sym > 12.0f) sym = 12.0f;
    mat.symmetry_n = static_cast<uint32_t>(sym);

    mat.depth = depth;
}

} // namespace fll
} // namespace vn
