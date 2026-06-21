#ifndef VAN_NUEMAN_TRANSFORM_COMPUTE_H
#define VAN_NUEMAN_TRANSFORM_COMPUTE_H

#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>
#include <memory>
#include "../include/Entity.h"

namespace vn { namespace simulation { class AgentECS; } }

class TransformCompute {
public:
    TransformCompute();
    ~TransformCompute();

    bool init(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue computeQueue,
              uint32_t computeFamily, uint32_t maxEntities);
    void cleanup();

    // Upload agent data from ECS to GPU SSBOs
    bool upload(const vn::simulation::AgentECS& ecs);

    // Dispatch the transform compute shader
    bool dispatch(float dt, uint32_t mode, float hazardLevel, float resourceDensity);

    // Download results back to ECS
    bool download(vn::simulation::AgentECS& ecs);

    // Getters
    VkPipeline getPipeline() const { return pipeline_; }
    VkPipelineLayout getPipelineLayout() const { return pipelineLayout_; }
    bool isReady() const { return ready_; }
    VkCommandBuffer getCommandBuffer() const { return computeCmdBuffer_; }

private:
    bool createBuffers(uint32_t maxEntities);
    bool createDescriptorSet();
    bool createShaderModule();
    bool createPipeline();
    bool createComputeCommandResources();
    void destroyBuffers();

    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkQueue computeQueue_ = VK_NULL_HANDLE;
    uint32_t computeFamily_ = 0;
    uint32_t maxEntities_ = 0;
    bool ready_ = false;

    // Compute command resources
    VkCommandPool computeCmdPool_ = VK_NULL_HANDLE;
    VkCommandBuffer computeCmdBuffer_ = VK_NULL_HANDLE;
    VkFence computeFence_ = VK_NULL_HANDLE;

    // SSBOs for SoA layout (mirrors AgentECS + transform_compute.cpp)
    VkBuffer posXBuffer_ = VK_NULL_HANDLE;   VkDeviceMemory posXMemory_ = VK_NULL_HANDLE;
    VkBuffer posYBuffer_ = VK_NULL_HANDLE;   VkDeviceMemory posYMemory_ = VK_NULL_HANDLE;
    VkBuffer posZBuffer_ = VK_NULL_HANDLE;   VkDeviceMemory posZMemory_ = VK_NULL_HANDLE;
    VkBuffer velXBuffer_ = VK_NULL_HANDLE;   VkDeviceMemory velXMemory_ = VK_NULL_HANDLE;
    VkBuffer velYBuffer_ = VK_NULL_HANDLE;   VkDeviceMemory velYMemory_ = VK_NULL_HANDLE;
    VkBuffer velZBuffer_ = VK_NULL_HANDLE;   VkDeviceMemory velZMemory_ = VK_NULL_HANDLE;
    VkBuffer activeBuffer_ = VK_NULL_HANDLE; VkDeviceMemory activeMemory_ = VK_NULL_HANDLE;
    VkBuffer resourcesBuffer_ = VK_NULL_HANDLE; VkDeviceMemory resourcesMemory_ = VK_NULL_HANDLE;
    VkBuffer pillarBuffers_[NumPillars]; VkDeviceMemory pillarMemories_[NumPillars];

    VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkShaderModule shaderModule_ = VK_NULL_HANDLE;

    // Pre-allocated staging buffers (avoid per-frame allocation)
    std::vector<float> posStaging_;
    std::vector<float> velStaging_;
    std::vector<uint32_t> activeStaging_;
    std::vector<int32_t> resourceStaging_;
    std::vector<int64_t> pillarStaging_;
};

#endif
