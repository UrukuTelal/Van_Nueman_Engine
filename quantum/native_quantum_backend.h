#pragma once

#include "quantum_interface.h"
#include "../include/Entity.h"
#include "../include/Transform.h"
#include "../include/BlochMath.h"
#include "../spatial/SpatialGrid.h"
#include <cstring>
#include <cmath>
#include <cstdio>

namespace vn {
namespace quantum {

#ifdef __CUDACC__

// ── CUDA kernels ──────────────────────────────────────────────

__global__ void native_wht_kernel(const float* input, float* output, int n) {
    int idx = threadIdx.x;
    if (idx >= n) return;

    __shared__ float s_data[32];
    s_data[idx] = input[idx];
    __syncthreads();

    for (int len = 1; len < n; len <<= 1) {
        int i = (idx / (len << 1)) * (len << 1) + (idx % len);
        float temp = s_data[i];
        s_data[i] = temp + s_data[i + len];
        s_data[i + len] = temp - s_data[i + len];
        __syncthreads();
    }

    float norm = 1.0f / sqrtf((float)n);
    output[idx] = s_data[idx] * norm;
}

__global__ void native_hopf_kernel(const float* thought, float* membrane, float* norms) {
    __shared__ float scratch[32];
    __shared__ float norm_thought[512];

    // Normalize input to unit S^511 before projection
    if (threadIdx.x == 0) {
        float sum_sq = 0.0f;
        for (int i = 0; i < 512; i++) sum_sq += thought[i] * thought[i];
        float inv_norm = (sum_sq > 1e-10f) ? rsqrtf(sum_sq) : 0.0f;
        for (int i = 0; i < 512; i++) norm_thought[i] = thought[i] * inv_norm;
    }
    __syncthreads();

    for (int f = 0; f < 32; f++) {
        float frame[16];
        for (int i = threadIdx.x; i < 16; i += blockDim.x)
            frame[i] = norm_thought[f * 16 + i];

        __syncthreads();

        if (threadIdx.x < 16)
            scratch[threadIdx.x] = frame[threadIdx.x];
        else if (threadIdx.x < 32)
            scratch[threadIdx.x] = 0.0f;

        __syncthreads();

        for (int len = 1; len < 32; len <<= 1) {
            int i = (threadIdx.x / (len << 1)) * (len << 1) + (threadIdx.x % len);
            if (i + len < 32) {
                float t = scratch[i];
                scratch[i] = t + scratch[i + len];
                scratch[i + len] = t - scratch[i + len];
            }
            __syncthreads();
        }

        float norm = 1.0f / sqrtf(32.0f);
        if (threadIdx.x < 32) {
            float phase = cosf(2.0f * 3.14159265f * (float)(f * threadIdx.x) / 32.0f);
            atomicAdd(&membrane[threadIdx.x], scratch[threadIdx.x] * norm * phase);
        }
        __syncthreads();
    }

    if (threadIdx.x == 0) {
        float inv = 1.0f / sqrtf(32.0f);
        for (int i = 0; i < 32; i++)
            membrane[i] *= inv;
    }
}

__global__ void native_bloch_evolve_kernel(const float* values, const float* deltas, float* result, int n) {
    int idx = threadIdx.x;
    if (idx >= n) return;
    float theta = acosf(2.0f * values[idx] - 1.0f);
    float new_theta = theta + deltas[idx];
    result[idx] = (cosf(new_theta) + 1.0f) * 0.5f;
}

#endif // __CUDACC__

class NativeQuantumBackend : public QuantumBackend {
public:
    QuantumBackendType type() const override { return QuantumBackendType::NATIVE_QUANTUM; }
    const char* name() const override { return "NativeQuantum"; }

    bool is_available() const override {
#ifdef __CUDACC__
        int device_count = 0;
        cudaError_t err = cudaGetDeviceCount(&device_count);
        return (err == cudaSuccess && device_count > 0);
#else
        return false;
#endif
    }

    bool supports_subroutine(QuantumSubroutine s) const override { return true; }

    QuantumResult execute_wht(const float input[32], float output[32]) override {
        QuantumResult r;
        r.success = true;
        r.fidelity_estimate = 1.0;

#ifdef __CUDACC__
        float *d_in, *d_out;
        if (cudaMalloc(&d_in, 32 * sizeof(float)) != cudaSuccess ||
            cudaMalloc(&d_out, 32 * sizeof(float)) != cudaSuccess) {
            std::memcpy(output, input, 32 * sizeof(float));
            fwht_cpu(output);
            r.amplitudes.assign(output, output + 32);
            return r;
        }

        cudaMemcpy(d_in, input, 32 * sizeof(float), cudaMemcpyHostToDevice);
        native_wht_kernel<<<1, 32>>>(d_in, d_out, 32);
        cudaMemcpy(output, d_out, 32 * sizeof(float), cudaMemcpyDeviceToHost);

        cudaFree(d_in);
        cudaFree(d_out);
#else
        std::memcpy(output, input, 32 * sizeof(float));
        fwht_cpu(output);
#endif

        r.amplitudes.assign(output, output + 32);
        return r;
    }

    QuantumResult execute_hopf_projection(const float thought[512], float membrane[32]) override {
        QuantumResult r;
        r.success = true;
        r.fidelity_estimate = 1.0;
        std::memset(membrane, 0, 32 * sizeof(float));

#ifdef __CUDACC__
        float *d_thought, *d_membrane, *d_norms = nullptr;
        if (cudaMalloc(&d_thought, 512 * sizeof(float)) != cudaSuccess ||
            cudaMalloc(&d_membrane, 32 * sizeof(float)) != cudaSuccess) {
            hopf_cpu(thought, membrane);
            r.amplitudes.assign(membrane, membrane + 32);
            return r;
        }

        cudaMemcpy(d_thought, thought, 512 * sizeof(float), cudaMemcpyHostToDevice);
        cudaMemset(d_membrane, 0, 32 * sizeof(float));
        native_hopf_kernel<<<1, 32>>>(d_thought, d_membrane, d_norms);
        cudaMemcpy(membrane, d_membrane, 32 * sizeof(float), cudaMemcpyDeviceToHost);

        cudaFree(d_thought);
        cudaFree(d_membrane);
#else
        hopf_cpu(thought, membrane);
#endif

        r.amplitudes.assign(membrane, membrane + 32);
        return r;
    }

    QuantumResult execute_spatial_search(const SpatialGrid<128>& grid,
                                          vn::fp20_t x, vn::fp20_t y, vn::fp20_t z,
                                          uint32_t range,
                                          std::vector<uint32_t>& out) override {
        QuantumResult r;
        r.success = true;
        r.fidelity_estimate = 1.0;

#ifdef __CUDACC__
        if (is_available()) {
            uint32_t* d_out;
            uint32_t* d_count;
            uint32_t h_count = 0;
            if (cudaMalloc(&d_out, 1024 * sizeof(uint32_t)) == cudaSuccess &&
                cudaMalloc(&d_count, sizeof(uint32_t)) == cudaSuccess) {
                cudaMemcpy(d_count, &h_count, sizeof(uint32_t), cudaMemcpyHostToDevice);

                dim3 block(256);
                dim3 grid_dim(1);
                native_spatial_search_kernel<<<grid_dim, block>>>(
                    (const SpatialGridGPU<128>*)&grid, x.to_float(), y.to_float(), z.to_float(),
                    range, d_out, d_count, 1024u);

                cudaMemcpy(&h_count, d_count, sizeof(uint32_t), cudaMemcpyDeviceToHost);
                out.resize(h_count);
                if (h_count > 0)
                    cudaMemcpy(out.data(), d_out, h_count * sizeof(uint32_t), cudaMemcpyDeviceToHost);

                cudaFree(d_out);
                cudaFree(d_count);
                r.measurements = out;
                return r;
            }
            if (d_out) cudaFree(d_out);
            if (d_count) cudaFree(d_count);
        }
#endif
        grid.query(x, y, z, range, out);
        r.measurements = out;
        return r;
    }

    QuantumResult execute_pillar_coupling(const PillarStateVector& actor,
                                           const PillarStateVector& subject,
                                           int operator_pillar,
                                           int target_pillar,
                                           PillarStateVector& result) override {
        QuantumResult r;
        r.success = true;
        r.fidelity_estimate = 1.0;
        result = subject;
        vn::fp20_t depth(0.5f);
        vn::fp20_t constraint(0.0f);
        vn::fp20_t alignment = bloch_dot_product(actor, result);
        transform_bloch(result, vn::fp20_t(actor[operator_pillar]),
                        vn::fp20_t(actor[Influence]),
                        target_pillar, depth, constraint, alignment);
        r.amplitudes.reserve(NumPillars);
        for (int i = 0; i < NumPillars; i++)
            r.amplitudes.push_back(result[i]);
        return r;
    }

    QuantumResult execute_bloch_evolution(const float pillar_values[16],
                                           const float delta_thetas[16],
                                           float result[16]) override {
        QuantumResult r;
        r.success = true;
        r.fidelity_estimate = 1.0;

#ifdef __CUDACC__
        float *d_vals, *d_deltas, *d_result;
        if (cudaMalloc(&d_vals, 16 * sizeof(float)) == cudaSuccess &&
            cudaMalloc(&d_deltas, 16 * sizeof(float)) == cudaSuccess &&
            cudaMalloc(&d_result, 16 * sizeof(float)) == cudaSuccess) {
            cudaMemcpy(d_vals, pillar_values, 16 * sizeof(float), cudaMemcpyHostToDevice);
            cudaMemcpy(d_deltas, delta_thetas, 16 * sizeof(float), cudaMemcpyHostToDevice);
            native_bloch_evolve_kernel<<<1, 16>>>(d_vals, d_deltas, d_result, 16);
            cudaMemcpy(result, d_result, 16 * sizeof(float), cudaMemcpyDeviceToHost);
            cudaFree(d_vals);
            cudaFree(d_deltas);
            cudaFree(d_result);
            r.amplitudes.assign(result, result + 16);
            return r;
        }
        if (d_vals) cudaFree(d_vals);
        if (d_deltas) cudaFree(d_deltas);
        if (d_result) cudaFree(d_result);
#endif
        for (int i = 0; i < 16; i++) {
            vn::fp20_t val(pillar_values[i]);
            vn::fp20_t dt(delta_thetas[i]);
            vn::fp20_t rotated = bloch_rotate(val, dt);
            result[i] = rotated.to_float();
        }
        r.amplitudes.assign(result, result + 16);
        return r;
    }

    double estimate_noise_level() const override {
#ifdef __CUDACC__
        cudaDeviceProp prop;
        if (cudaGetDeviceProperties(&prop, 0) == cudaSuccess) {
            return 0.001 * (1.0 - prop.multiProcessorCount / 128.0);
        }
#endif
        return 0.001;
    }

    size_t available_qubits() const override {
#ifdef __CUDACC__
        cudaDeviceProp prop;
        if (cudaGetDeviceProperties(&prop, 0) == cudaSuccess) {
            size_t free_bytes, total_bytes;
            if (cudaMemGetInfo(&free_bytes, &total_bytes) == cudaSuccess) {
                size_t qubits = 0;
                size_t needed = 16;
                while (needed <= free_bytes / 8) {
                    qubits++;
                    needed *= 2;
                }
                return qubits > 20 ? 20 : qubits;
            }
        }
#endif
        return 10;
    }

private:
    static void fwht_cpu(float* x) {
        float temp;
        for (int len = 1; len < 32; len <<= 1) {
            for (int i = 0; i < 32; i += (len << 1)) {
                for (int j = 0; j < len; j++) {
                    temp = x[i + j];
                    x[i + j] = temp + x[i + j + len];
                    x[i + j + len] = temp - x[i + j + len];
                }
            }
        }
        float norm = 1.0f / std::sqrt(32.0f);
        for (int i = 0; i < 32; i++) x[i] *= norm;
    }

    static void hopf_cpu(const float thought[512], float membrane[32]) {
        float norm_thought[512];
        float sum_sq = 0.0f;
        for (int i = 0; i < 512; i++) sum_sq += thought[i] * thought[i];
        float norm = std::sqrt(sum_sq);
        float inv_norm = (norm > 1e-10f) ? (1.0f / norm) : 0.0f;
        for (int i = 0; i < 512; i++) norm_thought[i] = thought[i] * inv_norm;

        float frame_wht[32][32];
        for (int f = 0; f < 32; f++) {
            float frame[32] = {};
            for (int i = 0; i < 16; i++) frame[i] = norm_thought[f * 16 + i];
            fwht_cpu(frame);
            std::memcpy(frame_wht[f], frame, 32 * sizeof(float));
        }
        for (int i = 0; i < 32; i++) {
            float sum = 0.0f;
            for (int f = 0; f < 32; f++) {
                float phase = std::cos(2.0f * 3.14159265f * static_cast<float>(f * i) / 32.0f);
                sum += frame_wht[f][i] * phase;
            }
            membrane[i] = sum * (1.0f / std::sqrt(32.0f));
        }
    }
};

} // namespace quantum
} // namespace vn
