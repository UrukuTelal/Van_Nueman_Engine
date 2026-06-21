// ── FLL CUDA-Q Kernel Definitions — COMPILE WITH nvq++ ONLY ─────────
// This file contains the __qpu__ kernel definitions for the FLL Engine's
// quantum-accelerated angle encoding and Swap Test circuits.
//
// Compilation:
//   nvq++ --target nvidia-sim -c fll_qpu_kernel_defs.cpp \
//          -o fll_qpu_kernel_defs.o
//
// Link with host code:
//   nvq++ fll_qpu_kernel_defs.o <host_objects> -o <output>
//
// The resulting object file must be linked into the main executable
// alongside CUDAQ_LIBS. When compiled by MSVC (without __NVQPP__),
// this file produces empty stubs — the CUDA-Q runtime path will not
// be available, and the classical fallback (CPU or raw CUDA) will be used.

#include "../include/FLLQPU.h"
#include <cstdint>
#include <vector>
#include <cudaq.h>

#ifdef VN_USE_CUDA_QUANTUM

// ── Angle Encoding Kernel ───────────────────────────────────────────
// Encodes a latent vector as a quantum state via per-qubit Ry(θ)Rz(φ).
//   data: interleaved [θ₀, φ₀, θ₁, φ₁, ...] where θ = x·π, φ = y·2π
//   n_qubits: number of qubits to encode (≤ 8 typically)
struct fll_angle_encode_kernel {
    void operator()(std::vector<float> data, int n_qubits) __qpu__ {
        cudaq::qvector q(n_qubits);
        for (int i = 0; i < n_qubits; i++) {
            float theta = data[2 * i] * 3.1415926535f;
            float phi   = data[2 * i + 1] * 6.2831853071f;
            ry(theta, q[i]);
            rz(phi, q[i]);
        }
    }
};

// ── Swap Test Kernel ────────────────────────────────────────────────
// Full quantum Swap Test circuit to compute |⟨ψ_A|ψ_B⟩|².
// Circuit: H|c⟩ · cswap(|ψ_A⟩, |ψ_B⟩) · H|c⟩ → measure |c⟩
// Result: P(|c⟩=|0⟩) = (1 + |⟨ψ_A|ψ_B⟩|²) / 2
struct fll_swap_test_kernel {
    void operator()(std::vector<float> data_a, std::vector<float> data_b, int n_qubits) __qpu__ {
        cudaq::qubit ancilla;
        cudaq::qvector q_a(n_qubits);
        cudaq::qvector q_b(n_qubits);

        // Encode state A
        for (int i = 0; i < n_qubits; i++) {
            float theta = data_a[2 * i] * 3.1415926535f;
            float phi   = data_a[2 * i + 1] * 6.2831853071f;
            ry(theta, q_a[i]);
            rz(phi, q_a[i]);
        }

        // Encode state B
        for (int i = 0; i < n_qubits; i++) {
            float theta = data_b[2 * i] * 3.1415926535f;
            float phi   = data_b[2 * i + 1] * 6.2831853071f;
            ry(theta, q_b[i]);
            rz(phi, q_b[i]);
        }

        // Swap Test
        h(ancilla);
        for (int i = 0; i < n_qubits; i++) {
            swap<cudaq::ctrl>(ancilla, q_a[i], q_b[i]);
        }
        h(ancilla);

        // Measure ancilla — P(0) encodes fidelity
        mz(ancilla);
    }
};

// ── Extern "C" linkage for host-side dispatch ───────────────────────
// These allow the host-side runtime wrappers in fll_qpu_kernels.cpp
// to invoke the kernels by name through the CUDA-Q runtime.

extern "C" {

void fll_angle_encode(float* data, int n_qubits) {
    std::vector<float> vec(data, data + 2 * n_qubits);
    auto& platform = cudaq::get_platform();
    auto context = cudaq::ExecutionContext("extract-state");
    platform.with_execution_context(context, [&]() {
        fll_angle_encode_kernel kernel;
        kernel(vec, n_qubits);
    });
}

void fll_swap_test(float* data_a, float* data_b, int n_qubits, int shots) {
    std::vector<float> vec_a(data_a, data_a + 2 * n_qubits);
    std::vector<float> vec_b(data_b, data_b + 2 * n_qubits);
    auto counts = cudaq::sample(shots, fll_swap_test_kernel{},
                                vec_a, vec_b, n_qubits);
}

} // extern "C"

#endif // VN_USE_CUDA_QUANTUM
