#pragma once
// ── FLL Quantum Processing Unit (QPU) API ──────────────────────────
// This header provides the API for the CUDA-Q-accelerated quantum
// resonance mapping subsystem. The actual __qpu__ kernel implementations
// are in fll/shaders/fll_qpu_kernel_defs.cpp (compiled by nvq++).
// Host-side wrappers are in fll/shaders/fll_qpu_kernels.cpp.
//
// When compiled without nvq++ (standard MSVC/GCC), the stubs return
// false/-1, triggering the classical fallback path (CPU or raw CUDA).

#include "FractalNode.h"
#include "QuantumSeed.h"
#include <cstdint>

namespace vn {
namespace fll {
namespace qpu {

// ── Angle-encode a latent vector onto a quantum register ───────────
// Encodes latent_vector as 2^n_qubit amplitudes via Ry(θ)Rz(φ) per qubit.
// Writes interleaved [re0,im0,re1,im1,...] to amplitudes_out.
// Returns true if QPU encoding succeeded, false → use classical fallback.
bool encode_amplitudes_qpu(
    const float* latent_vector,
    uint32_t latent_dim,
    uint32_t qubit_count,
    float* amplitudes_out,
    uint32_t amp_capacity);

// ── Swap Test fidelity entirely on the accelerator layer ──────────
// Returns |⟨ψ_A|ψ_B⟩|² ∈ [0,1], or -1.0 on failure → use classical.
float swap_test_fidelity_qpu(
    const float* latent_a,
    const float* latent_b,
    uint32_t latent_dim,
    uint32_t qubit_count);

// ── Batch resonance matrix via QPU ─────────────────────────────────
// Computes R_{ij} = |⟨ψ_i|ψ_j⟩|² for all pairs. Falls back pairwise.
void resonance_matrix_qpu(
    const QuantumSeed seeds[],
    uint32_t seed_count,
    float* matrix_out);

} // namespace qpu
} // namespace fll
} // namespace vn
