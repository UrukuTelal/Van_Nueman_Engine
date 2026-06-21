// ── FLL CUDA-Q Host-Side Wrappers ──────────────────────────────────
// This file provides host-callable wrappers for the quantum-accelerated
// FLL resonance mapping subsystem.
//
// Two compilation modes:
//   1. nvq++ (CUDA-Q compiler): __NVQPP__ defined → real QPU execution
//   2. MSVC/GCC (standard): stubs return false/-1 → classical fallback
//
// The classical fallback uses QuantumSeed::encode_to_amplitudes() and
// QuantumSeed::swap_test_fidelity(), which are pure CPU implementations.
// When CUDA is available, the raw CUDA kernels in fll_resonance.cu
// provide GPU-accelerated classical simulation.

#include "../include/FLLQPU.h"

// ====================================================================
// nvq++ COMPILATION PATH — Real CUDA-Q QPU execution
// ====================================================================
#if defined(VN_USE_CUDA_QUANTUM) && defined(__NVQPP__)

#include <cudaq.h>
#include <cudaq/algorithms/get_state.h>
#include <vector>
#include <cstring>
#include <cmath>

namespace vn {
namespace fll {
namespace qpu {

bool encode_amplitudes_qpu(
    const float* latent_vector,
    uint32_t latent_dim,
    uint32_t qubit_count,
    float* amplitudes_out,
    uint32_t amp_capacity)
{
    if (!latent_vector || !amplitudes_out) return false;
    if (qubit_count == 0 || qubit_count > 12) return false;

    uint32_t n_pairs = (latent_dim + 1) / 2;
    uint32_t n_qubits = (qubit_count < n_pairs) ? qubit_count : n_pairs;
    if (n_qubits == 0) return false;

    uint32_t amp_count = 1u << n_qubits;
    if (amp_capacity < amp_count * 2) return false;

    // Prepare angle data [θ₀, φ₀, θ₁, φ₁, ...] (normalized [0,1])
    std::vector<float> angles(static_cast<size_t>(n_qubits) * 2);
    for (uint32_t i = 0; i < n_qubits; i++) {
        float x = latent_vector[2 * i];
        float y = (2 * i + 1 < latent_dim) ? latent_vector[2 * i + 1] : 0.0f;
        if (x < 0.0f) x = 0.0f;
        if (x > 1.0f) x = 1.0f;
        if (y < 0.0f) y = 0.0f;
        if (y > 1.0f) y = 1.0f;
        angles[static_cast<size_t>(i) * 2]     = x;
        angles[static_cast<size_t>(i) * 2 + 1] = y;
    }

    try {
        // Execute angle encoding kernel and extract full state vector
        auto state = cudaq::get_state(fll_angle_encode_kernel{},
                                      angles.data(), static_cast<int>(n_qubits));
        auto num_amps = 1ull << n_qubits;
        std::memset(amplitudes_out, 0,
                    static_cast<size_t>(amp_count) * 2 * sizeof(float));
        for (std::size_t i = 0; i < num_amps && i < static_cast<std::size_t>(amp_count); i++) {
            auto amp = state[i];
            amplitudes_out[2 * i]     = static_cast<float>(amp.real());
            amplitudes_out[2 * i + 1] = static_cast<float>(amp.imag());
        }
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

float swap_test_fidelity_qpu(
    const float* latent_a,
    const float* latent_b,
    uint32_t latent_dim,
    uint32_t qubit_count)
{
    if (!latent_a || !latent_b) return -1.0f;
    if (qubit_count == 0 || qubit_count > 12) return -1.0f;

    uint32_t n_pairs = (latent_dim + 1) / 2;
    uint32_t n_qubits = (qubit_count < n_pairs) ? qubit_count : n_pairs;
    if (n_qubits == 0) return -1.0f;

    // Prepare angle data for both states
    std::vector<float> angles_a(static_cast<size_t>(n_qubits) * 2);
    std::vector<float> angles_b(static_cast<size_t>(n_qubits) * 2);

    auto fill = [&](const float* latent, std::vector<float>& out) {
        for (uint32_t i = 0; i < n_qubits; i++) {
            float x = latent[2 * i];
            float y = (2 * i + 1 < latent_dim) ? latent[2 * i + 1] : 0.0f;
            if (x < 0.0f) x = 0.0f;
            if (x > 1.0f) x = 1.0f;
            if (y < 0.0f) y = 0.0f;
            if (y > 1.0f) y = 1.0f;
            out[static_cast<size_t>(i) * 2]     = x;
            out[static_cast<size_t>(i) * 2 + 1] = y;
        }
    };
    fill(latent_a, angles_a);
    fill(latent_b, angles_b);

    try {
        // Extract both states, then compute overlap on the accelerator
        auto state_a = cudaq::get_state(fll_angle_encode_kernel{},
                                        angles_a.data(), static_cast<int>(n_qubits));
        auto state_b = cudaq::get_state(fll_angle_encode_kernel{},
                                        angles_b.data(), static_cast<int>(n_qubits));
        auto olap = state_a.overlap(state_b);
        float fidelity = static_cast<float>(
            olap.real() * olap.real() + olap.imag() * olap.imag());
        if (fidelity < 0.0f) fidelity = 0.0f;
        if (fidelity > 1.0f) fidelity = 1.0f;
        return fidelity;
    } catch (const std::exception&) {
        return -1.0f;
    }
}

void resonance_matrix_qpu(
    const QuantumSeed seeds[],
    uint32_t seed_count,
    float* matrix_out)
{
    if (!seeds || !matrix_out || seed_count == 0) return;
    for (uint32_t i = 0; i < seed_count; i++) {
        matrix_out[i * seed_count + i] = 1.0f;
        for (uint32_t j = i + 1; j < seed_count; j++) {
            float f = swap_test_fidelity_qpu(
                seeds[i].latent_vector,
                seeds[j].latent_vector,
                (seeds[i].latent_dim < seeds[j].latent_dim)
                    ? seeds[i].latent_dim : seeds[j].latent_dim,
                (seeds[i].qubit_count < seeds[j].qubit_count)
                    ? seeds[i].qubit_count : seeds[j].qubit_count);
            if (f < 0.0f) {
                f = QuantumSeed::swap_test_fidelity(seeds[i], seeds[j]);
            }
            matrix_out[i * seed_count + j] = f;
            matrix_out[j * seed_count + i] = f;
        }
    }
}

} // namespace qpu
} // namespace fll
} // namespace vn

// ====================================================================
// STANDARD C++ COMPILATION PATH — Stubs → classical fallback
// ====================================================================
#else

namespace vn {
namespace fll {
namespace qpu {

bool encode_amplitudes_qpu(
    const float*, uint32_t, uint32_t, float*, uint32_t)
{
    return false;
}

float swap_test_fidelity_qpu(
    const float*, const float*, uint32_t, uint32_t)
{
    return -1.0f;
}

void resonance_matrix_qpu(
    const QuantumSeed[], uint32_t, float*)
{
}

} // namespace qpu
} // namespace fll
} // namespace vn

#endif
