// ── FLL Quantum Resonance Kernel (CUDA) ─────────────────────────────
// Computes the pairwise Swap Test fidelity matrix for a set of
// QuantumSeed amplitude vectors on the GPU.
//
// Each thread block computes one row of the resonance matrix,
// with each thread computing one column entry.

#include <cuda_runtime.h>
#include <cstdint>

// ── Kernel: Resonance Matrix ────────────────────────────────────────
// Computes R_{ij} = |⟨ψ_i|ψ_j⟩|² for all pairs of quantum seeds.
//
// Input:
//   amplitudes  - flat array: [seed_0_re0, seed_0_im0, ..., seed_0_reN, seed_0_imN,
//                               seed_1_re0, seed_1_im0, ...]
//   seed_count  - number of seeds
//   amp_stride  - number of complex amplitude pairs per seed
//
// Output:
//   resonance_M - seed_count x seed_count float matrix, row-major

__global__ void resonance_matrix_kernel(
    const float* __restrict__ amplitudes,
    float* __restrict__ resonance_M,
    uint32_t seed_count,
    uint32_t amp_stride)
{
    // Each block handles a row i
    uint32_t i = blockIdx.x;
    if (i >= seed_count) return;

    const float* seed_i = amplitudes + i * amp_stride * 2;

    for (uint32_t j = threadIdx.x; j < seed_count; j += blockDim.x) {
        if (j > i) {
            const float* seed_j = amplitudes + j * amp_stride * 2;

            // Compute ⟨ψ_i|ψ_j⟩ = Σ_k conj(a_i_k) * a_j_k
            float re = 0.0f;
            float im = 0.0f;

            // Vectorized loop: process 4 complex pairs per iteration
            for (uint32_t k = 0; k < amp_stride * 2; k += 4) {
                // Load 4 floats from each seed (2 complex numbers per load)
                float2 a0 = *reinterpret_cast<const float2*>(seed_i + k);
                float2 a1 = *reinterpret_cast<const float2*>(seed_i + k + 2);
                float2 b0 = *reinterpret_cast<const float2*>(seed_j + k);
                float2 b1 = *reinterpret_cast<const float2*>(seed_j + k + 2);

                // conj(a) * b for first complex pair
                re += a0.x * b0.x + a0.y * b0.y;
                im += a0.x * b0.y - a0.y * b0.x;

                // conj(a) * b for second complex pair
                re += a1.x * b1.x + a1.y * b1.y;
                im += a1.x * b1.y - a1.y * b1.x;
            }

            float fidelity = re * re + im * im;

            // Store symmetric
            resonance_M[i * seed_count + j] = fidelity;
            resonance_M[j * seed_count + i] = fidelity;
        } else if (j == i) {
            resonance_M[i * seed_count + i] = 1.0f;  // self-resonance
        }
    }
}

// ── Kernel: Quantum Seed Encoding ──────────────────────────────────
// Encodes a batch of latent vectors into quantum amplitudes via
// angle encoding on the GPU.

__global__ void encode_seeds_kernel(
    const float* __restrict__ latent_vectors,
    float* __restrict__ amplitudes_out,
    uint32_t seed_count,
    uint32_t latent_dim,
    uint32_t qubit_count,
    float temperature)
{
    uint32_t seed_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (seed_idx >= seed_count) return;

    uint32_t amp_count = 1u << qubit_count;
    const float* latent = latent_vectors + seed_idx * latent_dim;
    float* amps = amplitudes_out + seed_idx * amp_count * 2;

    // Initialize |0...0⟩
    for (uint32_t i = 0; i < amp_count * 2; i++) {
        amps[i] = 0.0f;
    }
    amps[0] = 1.0f;

    // Apply angle encoding for each qubit
    uint32_t pairs = (latent_dim < 2) ? 1 : (latent_dim + 1) / 2;
    uint32_t n_qubits = (qubit_count < pairs) ? qubit_count : pairs;

    for (uint32_t q = 0; q < n_qubits; q++) {
        float x = latent[2 * q];
        float y = (2 * q + 1 < latent_dim) ? latent[2 * q + 1] : 0.0f;

        // Clamp and map to angles
        if (x < 0.0f) x = 0.0f;
        if (x > 1.0f) x = 1.0f;
        if (y < 0.0f) y = 0.0f;
        if (y > 1.0f) y = 1.0f;

        float theta = x * 3.1415926535f;
        float phi   = y * 6.2831853071f;

        float ct = __cosf(theta * 0.5f);
        float st = __sinf(theta * 0.5f);
        float cp = __cosf(phi);
        float sp = __sinf(phi);

        uint32_t stride = 1u << q;

        for (uint32_t i = 0; i < amp_count; i += 2 * stride) {
            for (uint32_t j = 0; j < stride; j++) {
                uint32_t idx0 = i + j;
                uint32_t idx1 = i + j + stride;

                float re0 = amps[2 * idx0];
                float im0 = amps[2 * idx0 + 1];
                float re1 = amps[2 * idx1];
                float im1 = amps[2 * idx1 + 1];

                amps[2 * idx0]     = ct * re0 - st * ( cp * re1 + sp * im1);
                amps[2 * idx0 + 1] = ct * im0 - st * ( cp * im1 - sp * re1);
                amps[2 * idx1]     = st * ( cp * re0 - sp * im0) + ct * re1;
                amps[2 * idx1 + 1] = st * ( cp * im0 + sp * re0) + ct * im1;
            }
        }
    }

    // Normalize
    float norm = 0.0f;
    for (uint32_t i = 0; i < amp_count * 2; i++) {
        norm += amps[i] * amps[i];
    }
    norm = __fsqrt_rn(norm);
    if (norm > 1e-12f) {
        float inv = 1.0f / norm;
        for (uint32_t i = 0; i < amp_count * 2; i++) {
            amps[i] *= inv;
        }
    }
}

// ── Launch wrappers (extern "C" for easy binding) ──────────────────

extern "C" void fll_resonance_matrix(
    const float* amplitudes,
    float* resonance_M,
    uint32_t seed_count,
    uint32_t amp_stride,
    cudaStream_t stream = nullptr)
{
    uint32_t threads = 256;
    uint32_t blocks = seed_count;
    resonance_matrix_kernel<<<blocks, threads, 0, stream>>>(
        amplitudes, resonance_M, seed_count, amp_stride
    );
}

extern "C" void fll_encode_seeds(
    const float* latent_vectors,
    float* amplitudes_out,
    uint32_t seed_count,
    uint32_t latent_dim,
    uint32_t qubit_count,
    float temperature,
    cudaStream_t stream = nullptr)
{
    uint32_t threads = 256;
    uint32_t blocks = (seed_count + threads - 1) / threads;
    encode_seeds_kernel<<<blocks, threads, 0, stream>>>(
        latent_vectors, amplitudes_out,
        seed_count, latent_dim, qubit_count, temperature
    );
}
