#ifndef VAN_NUEMAN_CUDA_PILLAR_COMPUTE_H
#define VAN_NUEMAN_CUDA_PILLAR_COMPUTE_H

#include <cstdint>
#include <memory>

namespace vn { namespace simulation { class AgentECS; } }

class CUDAPillarCompute {
public:
    CUDAPillarCompute();
    ~CUDAPillarCompute();

    bool init(uint32_t maxEntities);
    void cleanup();

    bool upload(const vn::simulation::AgentECS& ecs);
    bool dispatch(float dt, float hazardLevel, float resourceDensity);
    bool download(vn::simulation::AgentECS& ecs);

    bool isReady() const { return ready_; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool ready_ = false;
};

#endif
