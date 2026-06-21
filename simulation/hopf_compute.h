#ifndef VAN_NUEMAN_HOPF_COMPUTE_H
#define VAN_NUEMAN_HOPF_COMPUTE_H

#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>
#include <memory>
#include "../include/HopfPID.h"

class HopfCompute {
public:
    HopfCompute();
    ~HopfCompute();

    bool init(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue computeQueue,
              uint32_t computeFamily, uint32_t maxEntities);
    void cleanup();

    bool upload(const std::vector<HopfPIDState>& hopfStates);
    bool dispatch(float dt, uint32_t mode, float planckThetaMin, float complexityCap);
    bool download(std::vector<HopfPIDState>& hopfStates);

    VkPipeline getPipeline() const { return pipeline_; }
    VkPipelineLayout getPipelineLayout() const { return pipelineLayout_; }
    bool isReady() const { return ready_; }
    uint32_t maxEntities() const { return maxEntities_; }

private:
    bool createBuffers(uint32_t maxEntities);
    bool createDescriptorSet();
    bool createShaderModule();
    bool createPipeline();
    bool createComputeCommandResources();

    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkQueue computeQueue_ = VK_NULL_HANDLE;
    uint32_t computeFamily_ = 0;
    uint32_t maxEntities_ = 0;
    bool ready_ = false;

    VkCommandPool computeCmdPool_ = VK_NULL_HANDLE;
    VkCommandBuffer computeCmdBuffer_ = VK_NULL_HANDLE;
    VkFence computeFence_ = VK_NULL_HANDLE;

    // SSBOs for HopfPID state (SoA layout matching hopf_pid_compute.cpp entry point)
    VkBuffer thoughtStateBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory thoughtStateMemory_ = VK_NULL_HANDLE;
    VkBuffer whtCoeffsBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory whtCoeffsMemory_ = VK_NULL_HANDLE;
    VkBuffer membraneBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory membraneMemory_ = VK_NULL_HANDLE;
    VkBuffer fiberCoherenceBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory fiberCoherenceMemory_ = VK_NULL_HANDLE;
    VkBuffer relComplexityBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory relComplexityMemory_ = VK_NULL_HANDLE;
    VkBuffer depthBufferBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory depthBufferMemory_ = VK_NULL_HANDLE;
    VkBuffer constraintLevelBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory constraintLevelMemory_ = VK_NULL_HANDLE;
    VkBuffer activeBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory activeMemory_ = VK_NULL_HANDLE;
    VkBuffer inRuptureBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory inRuptureMemory_ = VK_NULL_HANDLE;

    VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkShaderModule shaderModule_ = VK_NULL_HANDLE;

    // Pre-allocated staging buffers (avoid per-frame allocation)
    std::vector<int64_t> thoughtStaging_;
    std::vector<float> whtStaging_;
    std::vector<float> membraneStaging_;
    std::vector<float> fiberStaging_;
    std::vector<float> complexityStaging_;
    std::vector<float> depthStaging_;
    std::vector<float> constraintStaging_;
    std::vector<uint32_t> activeStaging_;
    std::vector<uint32_t> ruptureStaging_;
};

#endif
