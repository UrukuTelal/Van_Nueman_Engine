#pragma once
#include "FractalNode.h"
#include <cstdint>
#include <cmath>
#include <complex>

#ifdef __CUDACC__
#include <cuda_runtime.h>
#endif

namespace vn {
namespace fll {

// Forward declarations for QPU integration
namespace qpu {
    bool  encode_amplitudes_qpu(const float*, uint32_t, uint32_t, float*, uint32_t);
    float swap_test_fidelity_qpu(const float*, const float*, uint32_t, uint32_t);
} // namespace qpu

// ── QuantumSeed ────────────────────────────────────────────────────
// A quantum semantic token: dense latent vector encoded as amplitudes
// of an N-qubit entangled quantum state.
//
// The seed S = (latent_vector, kernel_params, depth) fully determines
// the geometric coefficients of its FractalNode via the condensation
// transformation f(S, i) → M_{i-1}.

struct alignas(64) QuantumSeed {
    static constexpr uint32_t MAX_LATENT_DIM = 64;

    // ── Latent Semantic Vector ──────────────────────────────────
    // Dense embedding that defines "meaning" of the seed.
    // Encoded as quantum amplitudes via angle encoding.
    float latent_vector[MAX_LATENT_DIM];
    uint32_t latent_dim;                     // actual dimension used (≤ MAX_LATENT_DIM)
    uint32_t qubit_count;                    // number of qubits for encoding

    // ── Kernel Parameters ───────────────────────────────────────
    // These control how the seed transforms into geometric coefficients
    // during the condensation process.
    float kernel_temperature;                // 0.0 = deterministic, 1.0 = stochastic
    float kernel_resonance;                  // resonance coupling strength
    float kernel_phase;                      // global phase offset
    float kernel_harmonic_decay;             // harmonic amplitude decay per order

    // ── Depth Context ───────────────────────────────────────────
    uint32_t seed_depth;                     // depth at which this seed was created
    uint32_t parent_seed_id;                 // parent seed UID in condensation chain
    uint32_t flags;
    float pad;

    // ── Derived: quantum state descriptor ───────────────────────
    // Computed amplitude vector for the encoded state |ψ⟩.
    // Stored as interleaved [re0, im0, re1, im1, ...] x 2^n_qubits.
    static constexpr uint32_t MAX_AMPLITUDES = 256;
    float amplitudes[MAX_AMPLITUDES * 2];

    // ── Methods ─────────────────────────────────────────────────
    uint32_t amplitude_count() const {
        return 1u << qubit_count;
    }

    // Angle-encode latent_vector[0..latent_dim) into amplitudes.
    // Each pair of values maps to one qubit's Bloch sphere angles (θ, φ).
    // Attempts QPU-accelerated encoding first (nvq++), falls back to classical.
    void encode_to_amplitudes() {
        // Try QPU path for amplitude encoding
        if (qpu::encode_amplitudes_qpu(latent_vector, latent_dim, qubit_count,
                                        amplitudes, MAX_AMPLITUDES * 2)) {
            return; // QPU encoding succeeded
        }

        // Classical angle encoding fallback
        const uint32_t n = amplitude_count();
        const uint32_t pairs = (latent_dim + 1) / 2;

        // Zero-fill
        for (uint32_t i = 0; i < n * 2; i++) {
            amplitudes[i] = 0.0f;
        }

        // Encode first basis state |0...0⟩
        amplitudes[0] = 1.0f;
        amplitudes[1] = 0.0f;

        // Apply angle encoding: for each pair (x, y) of latent values,
        // rotate corresponding qubit by θ = π·x, φ = 2π·y.
        for (uint32_t q = 0; q < qubit_count && q < pairs; q++) {
            float x = latent_vector[2 * q];
            float y = (2 * q + 1 < latent_dim) ? latent_vector[2 * q + 1] : 0.0f;

            // Clamp to [0, 1] then map to angles
            if (x < 0.0f) x = 0.0f;
            if (x > 1.0f) x = 1.0f;
            if (y < 0.0f) y = 0.0f;
            if (y > 1.0f) y = 1.0f;

            float theta = x * 3.1415926535f;  // [0, π]
            float phi   = y * 6.2831853071f;  // [0, 2π]

            // Apply rotation to all basis states involving qubit q
            const uint32_t stride = 1u << q;
            for (uint32_t i = 0; i < n; i += 2 * stride) {
                for (uint32_t j = 0; j < stride; j++) {
                    const uint32_t idx0 = i + j;
                    const uint32_t idx1 = i + j + stride;

                    float re0 = amplitudes[2 * idx0];
                    float im0 = amplitudes[2 * idx0 + 1];
                    float re1 = amplitudes[2 * idx1];
                    float im1 = amplitudes[2 * idx1 + 1];

                    float ct = std::cos(theta * 0.5f);
                    float st = std::sin(theta * 0.5f);
                    float cp = std::cos(phi);
                    float sp = std::sin(phi);

                    // Single-qubit rotation: R_y(θ) R_z(φ)
                    // |0⟩ → cos(θ/2)|0⟩ + sin(θ/2)e^{iφ}|1⟩
                    // |1⟩ → -sin(θ/2)e^{-iφ}|0⟩ + cos(θ/2)|1⟩
                    amplitudes[2 * idx0]     = ct * re0 - st * ( cp * re1 + sp * im1);
                    amplitudes[2 * idx0 + 1] = ct * im0 - st * ( cp * im1 - sp * re1);
                    amplitudes[2 * idx1]     = st * ( cp * re0 - sp * im0) + ct * re1;
                    amplitudes[2 * idx1 + 1] = st * ( cp * im0 + sp * re0) + ct * im1;
                }
            }
        }

        // Normalize
        float norm = 0.0f;
        for (uint32_t i = 0; i < n * 2; i++) {
            norm += amplitudes[i] * amplitudes[i];
        }
        norm = std::sqrt(norm);
        if (norm > 1e-12f) {
            float inv_norm = 1.0f / norm;
            for (uint32_t i = 0; i < n * 2; i++) {
                amplitudes[i] *= inv_norm;
            }
        }
    }

    // Compute |⟨this|other⟩|² — the Swap Test fidelity between two seeds.
    static float swap_test_fidelity(const QuantumSeed& a, const QuantumSeed& b) {
        const uint32_t n = a.amplitude_count();
        const uint32_t nb = b.amplitude_count();
        const uint32_t m = (n < nb) ? n : nb;

        // Overlap = Σ_i conj(a_i) * b_i
        float re = 0.0f, im = 0.0f;
        for (uint32_t i = 0; i < m; i++) {
            float are = a.amplitudes[2 * i];
            float aim = a.amplitudes[2 * i + 1];
            float bre = b.amplitudes[2 * i];
            float bim = b.amplitudes[2 * i + 1];
            re += are * bre + aim * bim;
            im += are * bim - aim * bre;
        }

        // Fidelity = |⟨ψ|φ⟩|² = re² + im²
        return re * re + im * im;
    }
};

// ── Quantum Resonance Mapping ──────────────────────────────────────
// Maps a set of QuantumSeeds through pairwise Swap Tests to compute
// the resonance matrix R_{ij} = |⟨ψ_i|ψ_j⟩|².

struct ResonanceMatrix {
    static constexpr uint32_t MAX_SEEDS = 64;
    float M[MAX_SEEDS][MAX_SEEDS];          // pairwise fidelity matrix
    uint32_t seed_count;
    float avg_resonance;
    float min_resonance;
    float max_resonance;

    void compute(const QuantumSeed seeds[], uint32_t count) {
        seed_count = (count > MAX_SEEDS) ? MAX_SEEDS : count;
        avg_resonance = 0.0f;
        min_resonance = 1.0f;
        max_resonance = 0.0f;

        uint64_t pair_count = 0;
        for (uint32_t i = 0; i < seed_count; i++) {
            M[i][i] = 1.0f;  // self-resonance is always 1
            for (uint32_t j = i + 1; j < seed_count; j++) {
                // Try QPU-accelerated Swap Test first; fallback to classical
                float f = qpu::swap_test_fidelity_qpu(
                    seeds[i].latent_vector,
                    seeds[j].latent_vector,
                    (seeds[i].latent_dim < seeds[j].latent_dim)
                        ? seeds[i].latent_dim : seeds[j].latent_dim,
                    (seeds[i].qubit_count < seeds[j].qubit_count)
                        ? seeds[i].qubit_count : seeds[j].qubit_count);
                if (f < 0.0f) {
                    f = QuantumSeed::swap_test_fidelity(seeds[i], seeds[j]);
                }
                M[i][j] = f;
                M[j][i] = f;
                avg_resonance += f;
                pair_count++;
                if (f < min_resonance) min_resonance = f;
                if (f > max_resonance) max_resonance = f;
            }
        }
        if (pair_count > 0) {
            avg_resonance /= static_cast<float>(pair_count);
        }
    }
};

} // namespace fll
} // namespace vn
