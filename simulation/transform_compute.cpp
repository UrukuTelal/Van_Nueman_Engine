#include "transform_compute.h"
#include "../include/simulation/AgentECS.h"
#include "../agents/cognition.h"
#include <fstream>
#include <vector>
#include <iostream>
#include <cstring>
#include <algorithm>

// ── SPIR-V loading helper ──

static std::vector<uint32_t> readSPIRV(const char* path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return {};
    size_t size = file.tellg();
    file.seekg(0);
    std::vector<uint32_t> code(size / sizeof(uint32_t));
    file.read(reinterpret_cast<char*>(code.data()), size);
    return code;
}

// ── Memory type helper ──

static uint32_t findMemoryType(VkPhysicalDevice physDev, uint32_t typeFilter,
                                VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physDev, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & props) == props)
            return i;
    }
    return 0;
}

// ── Buffer creation helper ──

static bool createBuffer(VkDevice device, VkPhysicalDevice physDev,
                         VkDeviceSize size, VkBufferUsageFlags usage,
                         VkMemoryPropertyFlags props,
                         VkBuffer& buffer, VkDeviceMemory& memory) {
    VkBufferCreateInfo bci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bci.size = size; bci.usage = usage;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device, &bci, nullptr, &buffer) != VK_SUCCESS)
        return false;

    VkMemoryRequirements mr;
    vkGetBufferMemoryRequirements(device, buffer, &mr);

    VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.allocationSize = mr.size;
    mai.memoryTypeIndex = findMemoryType(physDev, mr.memoryTypeBits, props);
    if (vkAllocateMemory(device, &mai, nullptr, &memory) != VK_SUCCESS)
        return false;

    vkBindBufferMemory(device, buffer, memory, 0);
    return true;
}

// ── Constructor / Destructor ──

TransformCompute::TransformCompute() {
    for (int i = 0; i < NumPillars; i++) {
        pillarBuffers_[i] = VK_NULL_HANDLE;
        pillarMemories_[i] = VK_NULL_HANDLE;
    }
}

TransformCompute::~TransformCompute() { cleanup(); }

// ── Init ──

bool TransformCompute::init(VkDevice device, VkPhysicalDevice physDev,
                             VkQueue computeQueue, uint32_t computeFamily,
                             uint32_t maxEntities) {
    device_ = device;
    physicalDevice_ = physDev;
    computeQueue_ = computeQueue;
    computeFamily_ = computeFamily;
    maxEntities_ = maxEntities;

    // Pre-allocate staging vectors to their maximum size
    posStaging_.resize(maxEntities);
    velStaging_.resize(maxEntities);
    activeStaging_.resize(maxEntities);
    resourceStaging_.resize(maxEntities);
    pillarStaging_.resize(maxEntities);

    if (!createBuffers(maxEntities)) return false;
    if (!createDescriptorSet()) return false;
    if (!createShaderModule()) return false;
    if (!createPipeline()) return false;
    if (!createComputeCommandResources()) return false;

    ready_ = true;
    std::cout << "TransformCompute initialized: " << maxEntities << " entities" << std::endl;
    return true;
}

// ── Create SSBO buffers for SoA layout ──

bool TransformCompute::createBuffers(uint32_t maxEntities) {
    VkDeviceSize floatSize = maxEntities * sizeof(float);
    VkDeviceSize intSize = maxEntities * sizeof(int32_t);
    VkDeviceSize uintSize = maxEntities * sizeof(uint32_t);
    VkDeviceSize fp20Size = maxEntities * sizeof(int64_t); // fp20_t is ScaledInt<int64_t,20>

    VkBufferUsageFlags ssbo = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                               VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                               VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VkMemoryPropertyFlags hostVis = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    bool ok = true;
    ok = ok && createBuffer(device_, physicalDevice_, floatSize, ssbo, hostVis,
                            posXBuffer_, posXMemory_);
    ok = ok && createBuffer(device_, physicalDevice_, floatSize, ssbo, hostVis,
                            posYBuffer_, posYMemory_);
    ok = ok && createBuffer(device_, physicalDevice_, floatSize, ssbo, hostVis,
                            posZBuffer_, posZMemory_);
    ok = ok && createBuffer(device_, physicalDevice_, floatSize, ssbo, hostVis,
                            velXBuffer_, velXMemory_);
    ok = ok && createBuffer(device_, physicalDevice_, floatSize, ssbo, hostVis,
                            velYBuffer_, velYMemory_);
    ok = ok && createBuffer(device_, physicalDevice_, floatSize, ssbo, hostVis,
                            velZBuffer_, velZMemory_);
    ok = ok && createBuffer(device_, physicalDevice_, uintSize, ssbo, hostVis,
                            activeBuffer_, activeMemory_);
    ok = ok && createBuffer(device_, physicalDevice_, intSize, ssbo, hostVis,
                            resourcesBuffer_, resourcesMemory_);
    for (int i = 0; i < NumPillars; i++) {
        ok = ok && createBuffer(device_, physicalDevice_, fp20Size, ssbo, hostVis,
                                pillarBuffers_[i], pillarMemories_[i]);
    }
    return ok;
}

// ── Create descriptor set with SSBO bindings ──

bool TransformCompute::createDescriptorSet() {
    // 24 bindings matching transform_compute.cpp kernel parameter order:
    // 3 pos + 3 vel + 16 pillars + 1 active + 1 resources = 24
    const uint32_t bindingCount = 24;
    std::vector<VkDescriptorSetLayoutBinding> bindings(bindingCount);

    // Positions (0-2)
    bindings[0] = {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT};
    bindings[1] = {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT};
    bindings[2] = {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT};
    // Velocities (3-5)
    bindings[3] = {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT};
    bindings[4] = {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT};
    bindings[5] = {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT};
    // Pillars (6-21) in pillar index order
    for (int i = 0; i < NumPillars; i++) {
        bindings[6 + i] = {uint32_t(6 + i), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                           VK_SHADER_STAGE_COMPUTE_BIT};
    }
    // Active (22)
    bindings[22] = {22, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT};
    // Resources (23)
    bindings[23] = {23, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT};

    VkDescriptorSetLayoutCreateInfo dslci = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dslci.bindingCount = bindingCount; dslci.pBindings = bindings.data();
    if (vkCreateDescriptorSetLayout(device_, &dslci, nullptr, &descriptorSetLayout_) != VK_SUCCESS)
        return false;

    VkDescriptorPoolSize poolSize = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bindingCount};
    VkDescriptorPoolCreateInfo dpci = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    dpci.maxSets = 1; dpci.poolSizeCount = 1; dpci.pPoolSizes = &poolSize;
    if (vkCreateDescriptorPool(device_, &dpci, nullptr, &descriptorPool_) != VK_SUCCESS)
        return false;

    VkDescriptorSetAllocateInfo dsai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dsai.descriptorPool = descriptorPool_; dsai.descriptorSetCount = 1;
    dsai.pSetLayouts = &descriptorSetLayout_;
    if (vkAllocateDescriptorSets(device_, &dsai, &descriptorSet_) != VK_SUCCESS)
        return false;

    // Write descriptor set
    std::vector<VkDescriptorBufferInfo> bufInfos(bindingCount);
    std::vector<VkWriteDescriptorSet> writes(bindingCount);

    VkBuffer buffers[6 + NumPillars + 2] = {
        posXBuffer_, posYBuffer_, posZBuffer_,
        velXBuffer_, velYBuffer_, velZBuffer_,
        pillarBuffers_[0], pillarBuffers_[1], pillarBuffers_[2],
        pillarBuffers_[3], pillarBuffers_[4], pillarBuffers_[5],
        pillarBuffers_[6], pillarBuffers_[7], pillarBuffers_[8],
        pillarBuffers_[9], pillarBuffers_[10], pillarBuffers_[11],
        pillarBuffers_[12], pillarBuffers_[13], pillarBuffers_[14],
        pillarBuffers_[15],
        activeBuffer_, resourcesBuffer_
    };

    for (uint32_t i = 0; i < bindingCount; i++) {
        bufInfos[i].buffer = buffers[i];
        bufInfos[i].offset = 0; bufInfos[i].range = VK_WHOLE_SIZE;
        writes[i] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writes[i].dstSet = descriptorSet_; writes[i].dstBinding = i;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[i].pBufferInfo = &bufInfos[i];
    }
    vkUpdateDescriptorSets(device_, bindingCount, writes.data(), 0, nullptr);
    return true;
}

// ── Create shader module ──

bool TransformCompute::createShaderModule() {
    std::vector<std::string> paths = {
        "kernels/transform_compute.spv",
        "../kernels/transform_compute.spv",
        "transform_compute.spv"
    };
    std::vector<uint32_t> spirv;
    for (auto& p : paths) {
        spirv = readSPIRV(p.c_str());
        if (!spirv.empty()) { std::cout << "Loaded: " << p << std::endl; break; }
    }
    if (spirv.empty()) {
        std::cerr << "Failed to load transform_compute.spv" << std::endl;
        return false;
    }
    VkShaderModuleCreateInfo smci = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    smci.codeSize = spirv.size() * 4; smci.pCode = spirv.data();
    return vkCreateShaderModule(device_, &smci, nullptr, &shaderModule_) == VK_SUCCESS;
}

// ── Create pipeline ──

bool TransformCompute::createPipeline() {
    VkPushConstantRange pushRange = {};
    pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushRange.offset = 0; pushRange.size = sizeof(uint32_t) + sizeof(float) * 4;

    VkPipelineLayoutCreateInfo plci = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plci.setLayoutCount = 1; plci.pSetLayouts = &descriptorSetLayout_;
    plci.pushConstantRangeCount = 1; plci.pPushConstantRanges = &pushRange;
    if (vkCreatePipelineLayout(device_, &plci, nullptr, &pipelineLayout_) != VK_SUCCESS)
        return false;

    VkPipelineShaderStageCreateInfo ssci = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    ssci.stage = VK_SHADER_STAGE_COMPUTE_BIT; ssci.module = shaderModule_;
    ssci.pName = "transform_compute_main_soa";

    VkComputePipelineCreateInfo cpci = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    cpci.stage = ssci; cpci.layout = pipelineLayout_;
    VkResult r = vkCreateComputePipelines(device_, VK_NULL_HANDLE, 1, &cpci, nullptr, &pipeline_);
    if (r != VK_SUCCESS) return false;

    vkDestroyShaderModule(device_, shaderModule_, nullptr);
    shaderModule_ = VK_NULL_HANDLE;
    return true;
}

// ── Create compute command pool, buffer, and fence ──

bool TransformCompute::createComputeCommandResources() {
    VkCommandPoolCreateInfo cpci = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cpci.queueFamilyIndex = computeFamily_;
    if (vkCreateCommandPool(device_, &cpci, nullptr, &computeCmdPool_) != VK_SUCCESS)
        return false;

    VkCommandBufferAllocateInfo cbai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cbai.commandPool = computeCmdPool_;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(device_, &cbai, &computeCmdBuffer_) != VK_SUCCESS)
        return false;

    VkFenceCreateInfo fci = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    if (vkCreateFence(device_, &fci, nullptr, &computeFence_) != VK_SUCCESS)
        return false;

    return true;
}

// ── Upload ECS data to GPU ──

static void mapAndUpload(VkDevice device, VkDeviceMemory memory,
                         const void* src, VkDeviceSize size) {
    void* dst;
    vkMapMemory(device, memory, 0, size, 0, &dst);
    memcpy(dst, src, static_cast<size_t>(size));
    vkUnmapMemory(device, memory);
}

bool TransformCompute::upload(const vn::simulation::AgentECS& ecs) {
    if (!ready_) return false;

    uint32_t count = std::min(static_cast<uint32_t>(ecs.size()), maxEntities_);
    if (count == 0) return true;

    VkDeviceSize floatSize = count * sizeof(float);
    VkDeviceSize intSize = count * sizeof(int32_t);
    VkDeviceSize fp20Size = count * sizeof(int64_t);

    // Upload positions
    {
        for (uint32_t i = 0; i < count; i++) posStaging_[i] = ecs.x(i);
        mapAndUpload(device_, posXMemory_, posStaging_.data(), floatSize);
        for (uint32_t i = 0; i < count; i++) posStaging_[i] = ecs.y(i);
        mapAndUpload(device_, posYMemory_, posStaging_.data(), floatSize);
        for (uint32_t i = 0; i < count; i++) posStaging_[i] = ecs.z(i);
        mapAndUpload(device_, posZMemory_, posStaging_.data(), floatSize);
    }

    // Upload velocities
    {
        for (uint32_t i = 0; i < count; i++) velStaging_[i] = ecs.vx(i);
        mapAndUpload(device_, velXMemory_, velStaging_.data(), floatSize);
        for (uint32_t i = 0; i < count; i++) velStaging_[i] = ecs.vy(i);
        mapAndUpload(device_, velYMemory_, velStaging_.data(), floatSize);
        for (uint32_t i = 0; i < count; i++) velStaging_[i] = ecs.vz(i);
        mapAndUpload(device_, velZMemory_, velStaging_.data(), floatSize);
    }

    // Upload active mask (bool -> uint32_t)
    {
        for (uint32_t i = 0; i < count; i++) activeStaging_[i] = ecs.active(i) ? 1u : 0u;
        mapAndUpload(device_, activeMemory_, activeStaging_.data(), count * sizeof(uint32_t));
    }

    // Upload resources
    {
        for (uint32_t i = 0; i < count; i++) resourceStaging_[i] = static_cast<int32_t>(ecs.resources(i));
        mapAndUpload(device_, resourcesMemory_, resourceStaging_.data(), intSize);
    }

    // Upload pillars (each pillar array: fp20_t = ScaledInt<int64_t,20>, GPU expects int64_t)
    for (int p = 0; p < NumPillars; p++) {
        for (uint32_t i = 0; i < count; i++) {
            pillarStaging_[i] = ecs.pillar(i, static_cast<PillarIndex>(p)).raw();
        }
        mapAndUpload(device_, pillarMemories_[p], pillarStaging_.data(), fp20Size);
    }

    return true;
}

// ── Dispatch ──

bool TransformCompute::dispatch(float dt, uint32_t mode,
                                 float hazardLevel, float resourceDensity) {
    if (!ready_) return false;

    // Begin command buffer
    VkCommandBufferBeginInfo cbbi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    cbbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(computeCmdBuffer_, &cbbi) != VK_SUCCESS)
        return false;

    vkCmdBindPipeline(computeCmdBuffer_, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_);
    vkCmdBindDescriptorSets(computeCmdBuffer_, VK_PIPELINE_BIND_POINT_COMPUTE,
                            pipelineLayout_, 0, 1, &descriptorSet_, 0, nullptr);

    // Push constants: entity_count, dt, mode, hazard_level, resource_density
    // Must match transform_compute_main() kernel parameter order after buffer pointers
    struct PushData {
        uint32_t entityCount; float dt; uint32_t mode; float hazard; float resource;
    };
    PushData pd = {maxEntities_, dt, mode, hazardLevel, resourceDensity};
    vkCmdPushConstants(computeCmdBuffer_, pipelineLayout_, VK_SHADER_STAGE_COMPUTE_BIT,
                       0, sizeof(PushData), &pd);

    // Dispatch groups (256 threads per group via workgroup size in shader)
    uint32_t groupCount = (maxEntities_ + 255) / 256;
    vkCmdDispatch(computeCmdBuffer_, groupCount, 1, 1);

    if (vkEndCommandBuffer(computeCmdBuffer_) != VK_SUCCESS)
        return false;

    // Submit to compute queue
    VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers = &computeCmdBuffer_;
    if (vkQueueSubmit(computeQueue_, 1, &si, computeFence_) != VK_SUCCESS)
        return false;

    // Wait for completion
    vkWaitForFences(device_, 1, &computeFence_, VK_TRUE, UINT64_MAX);
    vkResetFences(device_, 1, &computeFence_);

    return true;
}

// ── Download results back to ECS ──

bool TransformCompute::download(vn::simulation::AgentECS& ecs) {
    if (!ready_) return false;

    uint32_t count = std::min(static_cast<uint32_t>(ecs.size()), maxEntities_);
    if (count == 0) return true;

    auto mapAndRead = [&](VkDeviceMemory memory, void* dst, VkDeviceSize size) {
        void* src;
        vkMapMemory(device_, memory, 0, size, 0, &src);
        memcpy(dst, src, static_cast<size_t>(size));
        vkUnmapMemory(device_, memory);
    };

    VkDeviceSize floatSize = count * sizeof(float);
    VkDeviceSize intSize = count * sizeof(int32_t);
    VkDeviceSize fp20Size = count * sizeof(int64_t);

    // Download positions
    {
        mapAndRead(posXMemory_, posStaging_.data(), floatSize);
        for (uint32_t i = 0; i < count; i++) ecs.x(i) = posStaging_[i];
        mapAndRead(posYMemory_, posStaging_.data(), floatSize);
        for (uint32_t i = 0; i < count; i++) ecs.y(i) = posStaging_[i];
        mapAndRead(posZMemory_, posStaging_.data(), floatSize);
        for (uint32_t i = 0; i < count; i++) ecs.z(i) = posStaging_[i];
    }

    // Download velocities
    {
        mapAndRead(velXMemory_, velStaging_.data(), floatSize);
        for (uint32_t i = 0; i < count; i++) ecs.vx(i) = velStaging_[i];
        mapAndRead(velYMemory_, velStaging_.data(), floatSize);
        for (uint32_t i = 0; i < count; i++) ecs.vy(i) = velStaging_[i];
        mapAndRead(velZMemory_, velStaging_.data(), floatSize);
        for (uint32_t i = 0; i < count; i++) ecs.vz(i) = velStaging_[i];
    }

    // Download active mask (uint32_t -> bool)
    {
        mapAndRead(activeMemory_, activeStaging_.data(), count * sizeof(uint32_t));
        for (uint32_t i = 0; i < count; i++) ecs.active(i) = activeStaging_[i] != 0;
    }

    // Download resources
    {
        mapAndRead(resourcesMemory_, resourceStaging_.data(), intSize);
        for (uint32_t i = 0; i < count; i++) ecs.resources(i) = resourceStaging_[i];
    }

    // Download pillars (fp20_t = ScaledInt<int64_t,20>, same representation on GPU and CPU)
    for (int p = 0; p < NumPillars; p++) {
        mapAndRead(pillarMemories_[p], pillarStaging_.data(), fp20Size);
        for (uint32_t i = 0; i < count; i++) {
            ecs.pillar(i, static_cast<PillarIndex>(p)) = vn::fp20_t::from_raw(pillarStaging_[i]);
        }
    }

    return true;
}

// ── Cleanup ──

void TransformCompute::cleanup() {
    if (device_ == VK_NULL_HANDLE) return;

    auto destroy = [&](VkBuffer& buf, VkDeviceMemory& mem) {
        if (buf) { vkDestroyBuffer(device_, buf, nullptr); buf = VK_NULL_HANDLE; }
        if (mem) { vkFreeMemory(device_, mem, nullptr); mem = VK_NULL_HANDLE; }
    };

    destroy(posXBuffer_, posXMemory_);
    destroy(posYBuffer_, posYMemory_);
    destroy(posZBuffer_, posZMemory_);
    destroy(velXBuffer_, velXMemory_);
    destroy(velYBuffer_, velYMemory_);
    destroy(velZBuffer_, velZMemory_);
    destroy(activeBuffer_, activeMemory_);
    destroy(resourcesBuffer_, resourcesMemory_);
    for (int i = 0; i < NumPillars; i++) destroy(pillarBuffers_[i], pillarMemories_[i]);

    if (descriptorSetLayout_) { vkDestroyDescriptorSetLayout(device_, descriptorSetLayout_, nullptr); descriptorSetLayout_ = VK_NULL_HANDLE; }
    if (descriptorPool_) { vkDestroyDescriptorPool(device_, descriptorPool_, nullptr); descriptorPool_ = VK_NULL_HANDLE; }
    if (pipelineLayout_) { vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr); pipelineLayout_ = VK_NULL_HANDLE; }
    if (pipeline_) { vkDestroyPipeline(device_, pipeline_, nullptr); pipeline_ = VK_NULL_HANDLE; }
    if (shaderModule_) { vkDestroyShaderModule(device_, shaderModule_, nullptr); shaderModule_ = VK_NULL_HANDLE; }

    if (computeCmdPool_) {
        if (computeCmdBuffer_) {
            vkFreeCommandBuffers(device_, computeCmdPool_, 1, &computeCmdBuffer_);
            computeCmdBuffer_ = VK_NULL_HANDLE;
        }
        vkDestroyCommandPool(device_, computeCmdPool_, nullptr);
        computeCmdPool_ = VK_NULL_HANDLE;
    }
    if (computeFence_) { vkDestroyFence(device_, computeFence_, nullptr); computeFence_ = VK_NULL_HANDLE; }

    ready_ = false;
}
