#include "cuda_pillar_compute.h"

#ifdef VN_USE_CUDA

#include <cuda_runtime.h>
#include "../include/simulation/AgentECS.h"
#include "../include/Entity.h"
#include "../agents/cognition.h"

// CUDA kernels expect `PillarStateVector*` but we pass raw float arrays.
// Both are NumPillars*sizeof(float) per entity, so reinterpret_cast is safe.

extern __global__ void pillars_update_kernel(void* pillar_vectors,
                                              const float* external_forces,
                                              int num_entities, float dt);
extern __global__ void pillars_harm_kernel(void* pillar_vectors,
                                            const float* damage,
                                            int num_entities, float dt);

struct CUDAPillarCompute::Impl {
    float* d_pillars = nullptr;
    float* d_forces = nullptr;
    float* d_damage = nullptr;
    uint32_t capacity = 0;
    int device_id = 0;

    ~Impl() { cleanup(); }

    bool alloc(uint32_t maxEntities) {
        if (maxEntities == 0 && capacity > 0) return true;
        if (maxEntities <= capacity && d_pillars) return true;
        cleanup();
        cudaError_t e;
        size_t pitch = static_cast<size_t>(NumPillars) * sizeof(float);
        size_t total = maxEntities * pitch;
        e = cudaMalloc(&d_pillars, total);
        if (e != cudaSuccess) { cleanup(); return false; }
        e = cudaMemset(d_pillars, 0, total);
        if (e != cudaSuccess) { cleanup(); return false; }
        e = cudaMalloc(&d_forces, maxEntities * sizeof(float));
        if (e != cudaSuccess) { cleanup(); return false; }
        e = cudaMalloc(&d_damage, maxEntities * sizeof(float));
        if (e != cudaSuccess) { cleanup(); return false; }
        capacity = maxEntities;
        return true;
    }

    void cleanup() {
        if (d_pillars) { cudaFree(d_pillars); d_pillars = nullptr; }
        if (d_forces)  { cudaFree(d_forces);  d_forces = nullptr; }
        if (d_damage)  { cudaFree(d_damage);  d_damage = nullptr; }
        capacity = 0;
    }
};

CUDAPillarCompute::CUDAPillarCompute() : impl_(std::make_unique<Impl>()) {}
CUDAPillarCompute::~CUDAPillarCompute() = default;

bool CUDAPillarCompute::init(uint32_t maxEntities) {
    ready_ = impl_->alloc(maxEntities);
    return ready_;
}

void CUDAPillarCompute::cleanup() {
    impl_->cleanup();
    ready_ = false;
}

bool CUDAPillarCompute::upload(const vn::simulation::AgentECS& ecs) {
    if (!ready_) return false;
    size_t count = ecs.size();
    if (count == 0) return true;
    if (count > impl_->capacity) {
        if (!impl_->alloc(static_cast<uint32_t>(count))) return false;
    }

    std::vector<float> host_pillars(count * NumPillars, 0.0f);
    std::vector<float> host_forces(count, 0.0f);
    std::vector<float> host_damage(count, 0.0f);

    for (size_t i = 0; i < count; i++) {
        if (!ecs.active(static_cast<vn::simulation::AgentECS::Index>(i))) continue;
        auto psv = ecs.get_pillars(static_cast<vn::simulation::AgentECS::Index>(i));
        for (int p = 0; p < NumPillars; p++) {
            host_pillars[i * NumPillars + p] = psv[p];
        }
    }

    cudaError_t e;
    size_t pitch = static_cast<size_t>(NumPillars) * sizeof(float);
    e = cudaMemcpy(impl_->d_pillars, host_pillars.data(),
                   count * pitch, cudaMemcpyHostToDevice);
    if (e != cudaSuccess) return false;
    e = cudaMemcpy(impl_->d_forces, host_forces.data(),
                   count * sizeof(float), cudaMemcpyHostToDevice);
    if (e != cudaSuccess) return false;
    e = cudaMemcpy(impl_->d_damage, host_damage.data(),
                   count * sizeof(float), cudaMemcpyHostToDevice);
    if (e != cudaSuccess) return false;

    return true;
}

bool CUDAPillarCompute::dispatch(float dt, float hazardLevel, float resourceDensity) {
    if (!ready_) return false;
    size_t count = impl_->capacity;
    if (count == 0) return true;

    uint32_t block = 256;
    uint32_t grid = (static_cast<uint32_t>(count) + block - 1) / block;

    pillars_update_kernel<<<grid, block>>>(impl_->d_pillars, impl_->d_forces,
                                            static_cast<int>(count), dt);

    float harm_scale = hazardLevel * 0.1f;
    std::vector<float> host_damage(count, harm_scale);
    cudaMemcpy(impl_->d_damage, host_damage.data(),
               count * sizeof(float), cudaMemcpyHostToDevice);

    pillars_harm_kernel<<<grid, block>>>(impl_->d_pillars, impl_->d_damage,
                                          static_cast<int>(count), dt);

    cudaError_t e = cudaDeviceSynchronize();
    return e == cudaSuccess;
}

bool CUDAPillarCompute::download(vn::simulation::AgentECS& ecs) {
    if (!ready_) return false;
    size_t count = ecs.size();
    if (count == 0 || count > impl_->capacity) return false;

    std::vector<float> host_pillars(count * NumPillars);
    size_t pitch = static_cast<size_t>(NumPillars) * sizeof(float);
    cudaError_t e = cudaMemcpy(host_pillars.data(), impl_->d_pillars,
                                count * pitch, cudaMemcpyDeviceToHost);
    if (e != cudaSuccess) return false;

    for (size_t i = 0; i < count; i++) {
        if (!ecs.active(static_cast<vn::simulation::AgentECS::Index>(i))) continue;
        PillarStateVector psv;
        for (int p = 0; p < NumPillars; p++) {
            psv[p] = host_pillars[i * NumPillars + p];
        }
        for (int p = 0; p < NumPillars; p++) {
            ecs.pillar(static_cast<vn::simulation::AgentECS::Index>(i),
                       static_cast<PillarIndex>(p)) = vn::fp20_t(psv[p]);
        }
    }

    return true;
}

#else

// Stub: when VN_USE_CUDA is not defined, disable all operations
struct CUDAPillarCompute::Impl {};
CUDAPillarCompute::CUDAPillarCompute() = default;
CUDAPillarCompute::~CUDAPillarCompute() = default;
bool CUDAPillarCompute::init(uint32_t) { return false; }
void CUDAPillarCompute::cleanup() { ready_ = false; }
bool CUDAPillarCompute::upload(const vn::simulation::AgentECS&) { return false; }
bool CUDAPillarCompute::dispatch(float, float, float) { return false; }
bool CUDAPillarCompute::download(vn::simulation::AgentECS&) { return false; }

#endif
