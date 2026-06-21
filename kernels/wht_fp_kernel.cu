#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <cstdint>

#define WHT_FP_SCALE 1048576LL

__device__ int64_t wht_fp_add(int64_t a, int64_t b) { return a + b; }
__device__ int64_t wht_fp_sub(int64_t a, int64_t b) { return a - b; }

__global__ void wht_transform_fp_kernel(int64_t* buffer, uint32_t log2n, uint32_t stride) {
    uint32_t tid = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t n = 1 << log2n;
    uint32_t batch_idx = tid / n;
    uint32_t elem_idx = tid % n;

    bool valid = batch_idx < stride;

    __syncthreads();

    if (!valid) return;

    int64_t* batch = buffer + batch_idx * n;

    for (uint32_t len = 1; len < n; len <<= 1) {
        uint32_t pair_idx = elem_idx & (len - 1);
        uint32_t base = elem_idx - pair_idx;
        uint32_t offset = elem_idx + len;

        if (pair_idx == 0 && offset < n) {
            int64_t temp = batch[elem_idx];
            batch[elem_idx] = wht_fp_add(temp, batch[offset]);
            batch[offset] = wht_fp_sub(temp, batch[offset]);
        }
        __syncthreads();
    }
}

__global__ void wht_inverse_fp_kernel(int64_t* buffer, uint32_t log2n, uint32_t stride) {
    uint32_t tid = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t n = 1 << log2n;
    uint32_t batch_idx = tid / n;
    uint32_t elem_idx = tid % n;

    bool valid = batch_idx < stride;

    __syncthreads();

    if (!valid) return;

    int64_t* batch = buffer + batch_idx * n;

    // Same WHT butterfly as forward (WHT is self-inverse up to scale)
    for (uint32_t len = 1; len < n; len <<= 1) {
        uint32_t pair_idx = elem_idx & (len - 1);
        uint32_t base = elem_idx - pair_idx;
        uint32_t offset = elem_idx + len;

        if (pair_idx == 0 && offset < n) {
            int64_t temp = batch[elem_idx];
            batch[elem_idx] = wht_fp_add(temp, batch[offset]);
            batch[offset] = wht_fp_sub(temp, batch[offset]);
        }
        __syncthreads();
    }

    // Scale by 1/N for inverse transform (undoes forward amplification)
    if (elem_idx < n) {
        batch[elem_idx] /= (int64_t)n;
    }
}
