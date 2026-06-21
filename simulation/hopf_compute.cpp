#include "hopf_compute.h"
#include <fstream>
#include <vector>
#include <iostream>
#include <cstring>
#include <algorithm>

static std::vector<uint32_t> readSPIRV(const char* path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return {};
    size_t size = file.tellg();
    file.seekg(0);
    std::vector<uint32_t> code(size / sizeof(uint32_t));
    file.read(reinterpret_cast<char*>(code.data()), size);
    return code;
}

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

HopfCompute::HopfCompute() {}

HopfCompute::~HopfCompute() { cleanup(); }

bool HopfCompute::init(VkDevice device, VkPhysicalDevice physDev,
                        VkQueue computeQueue, uint32_t computeFamily,
                        uint32_t maxEntities) {
    device_ = device;
    physicalDevice_ = physDev;
    computeQueue_ = computeQueue;
    computeFamily_ = computeFamily;
    maxEntities_ = maxEntities;

    // Pre-allocate staging vectors to their maximum size
    thoughtStaging_.resize(maxEntities * 512);
    whtStaging_.resize(maxEntities * 1024);
    membraneStaging_.resize(maxEntities * HOPF_BASE_DIM);
    fiberStaging_.resize(maxEntities * HOPF_MAX_FIBERS);
    complexityStaging_.resize(maxEntities);
    depthStaging_.resize(maxEntities);
    constraintStaging_.resize(maxEntities);
    activeStaging_.resize(maxEntities);
    ruptureStaging_.resize(maxEntities);

    if (!createBuffers(maxEntities)) return false;
    if (!createDescriptorSet()) return false;
    if (!createShaderModule()) return false;
    if (!createPipeline()) return false;
    if (!createComputeCommandResources()) return false;

    ready_ = true;
    std::cout << "HopfCompute initialized: " << maxEntities << " entities" << std::endl;
    return true;
}

bool HopfCompute::createBuffers(uint32_t maxEntities) {
    VkDeviceSize thoughtStateSize  = maxEntities * 512 * sizeof(int64_t);
    VkDeviceSize whtCoeffsSize    = maxEntities * 1024 * sizeof(float);
    VkDeviceSize membraneSize     = maxEntities * 32 * sizeof(float);
    VkDeviceSize fiberCoherenceSize = maxEntities * 256 * sizeof(float);
    VkDeviceSize floatScalarSize  = maxEntities * sizeof(float);
    VkDeviceSize uintSize         = maxEntities * sizeof(uint32_t);

    VkBufferUsageFlags ssbo = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                               VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                               VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VkMemoryPropertyFlags hostVis = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    bool ok = true;
    ok = ok && createBuffer(device_, physicalDevice_, thoughtStateSize, ssbo, hostVis,
                            thoughtStateBuffer_, thoughtStateMemory_);
    ok = ok && createBuffer(device_, physicalDevice_, whtCoeffsSize, ssbo, hostVis,
                            whtCoeffsBuffer_, whtCoeffsMemory_);
    ok = ok && createBuffer(device_, physicalDevice_, membraneSize, ssbo, hostVis,
                            membraneBuffer_, membraneMemory_);
    ok = ok && createBuffer(device_, physicalDevice_, fiberCoherenceSize, ssbo, hostVis,
                            fiberCoherenceBuffer_, fiberCoherenceMemory_);
    ok = ok && createBuffer(device_, physicalDevice_, floatScalarSize, ssbo, hostVis,
                            relComplexityBuffer_, relComplexityMemory_);
    ok = ok && createBuffer(device_, physicalDevice_, floatScalarSize, ssbo, hostVis,
                            depthBufferBuffer_, depthBufferMemory_);
    ok = ok && createBuffer(device_, physicalDevice_, floatScalarSize, ssbo, hostVis,
                            constraintLevelBuffer_, constraintLevelMemory_);
    ok = ok && createBuffer(device_, physicalDevice_, uintSize, ssbo, hostVis,
                            activeBuffer_, activeMemory_);
    ok = ok && createBuffer(device_, physicalDevice_, uintSize, ssbo, hostVis,
                            inRuptureBuffer_, inRuptureMemory_);
    return ok;
}

bool HopfCompute::createDescriptorSet() {
    const uint32_t bindingCount = 9;
    std::vector<VkDescriptorSetLayoutBinding> bindings(bindingCount);

    bindings[0] = {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT};
    bindings[1] = {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT};
    bindings[2] = {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT};
    bindings[3] = {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT};
    bindings[4] = {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT};
    bindings[5] = {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT};
    bindings[6] = {6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT};
    bindings[7] = {7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT};
    bindings[8] = {8, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT};

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

    VkBuffer buffers[bindingCount] = {
        thoughtStateBuffer_, whtCoeffsBuffer_, membraneBuffer_,
        fiberCoherenceBuffer_, relComplexityBuffer_, depthBufferBuffer_,
        constraintLevelBuffer_, activeBuffer_, inRuptureBuffer_
    };

    std::vector<VkDescriptorBufferInfo> bufInfos(bindingCount);
    std::vector<VkWriteDescriptorSet> writes(bindingCount);

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

bool HopfCompute::createShaderModule() {
    std::vector<std::string> paths = {
        "kernels/hopf_pid_compute.spv",
        "../kernels/hopf_pid_compute.spv",
        "hopf_pid_compute.spv"
    };
    std::vector<uint32_t> spirv;
    for (auto& p : paths) {
        spirv = readSPIRV(p.c_str());
        if (!spirv.empty()) { std::cout << "Loaded: " << p << std::endl; break; }
    }
    if (spirv.empty()) {
        std::cerr << "Failed to load hopf_pid_compute.spv" << std::endl;
        return false;
    }
    VkShaderModuleCreateInfo smci = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    smci.codeSize = spirv.size() * 4; smci.pCode = spirv.data();
    return vkCreateShaderModule(device_, &smci, nullptr, &shaderModule_) == VK_SUCCESS;
}

bool HopfCompute::createPipeline() {
    VkPushConstantRange pushRange = {};
    pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushRange.offset = 0; pushRange.size = sizeof(float) * 3 + sizeof(uint32_t) * 2;

    VkPipelineLayoutCreateInfo plci = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plci.setLayoutCount = 1; plci.pSetLayouts = &descriptorSetLayout_;
    plci.pushConstantRangeCount = 1; plci.pPushConstantRanges = &pushRange;
    if (vkCreatePipelineLayout(device_, &plci, nullptr, &pipelineLayout_) != VK_SUCCESS)
        return false;

    VkPipelineShaderStageCreateInfo ssci = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    ssci.stage = VK_SHADER_STAGE_COMPUTE_BIT; ssci.module = shaderModule_;
    ssci.pName = "hopf_pid_compute_main";

    VkComputePipelineCreateInfo cpci = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    cpci.stage = ssci; cpci.layout = pipelineLayout_;
    VkResult r = vkCreateComputePipelines(device_, VK_NULL_HANDLE, 1, &cpci, nullptr, &pipeline_);
    if (r != VK_SUCCESS) return false;

    vkDestroyShaderModule(device_, shaderModule_, nullptr);
    shaderModule_ = VK_NULL_HANDLE;
    return true;
}

bool HopfCompute::createComputeCommandResources() {
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

static void mapAndUpload(VkDevice device, VkDeviceMemory memory,
                         const void* src, VkDeviceSize size) {
    void* dst;
    vkMapMemory(device, memory, 0, size, 0, &dst);
    memcpy(dst, src, static_cast<size_t>(size));
    vkUnmapMemory(device, memory);
}

bool HopfCompute::upload(const std::vector<HopfPIDState>& hopfStates) {
    if (!ready_) return false;

    uint32_t count = std::min(static_cast<uint32_t>(hopfStates.size()), maxEntities_);
    if (count == 0) return true;

    // Upload thought state (512 fxp20 per entity -> int64_t flat array)
    {
        for (uint32_t i = 0; i < count; i++) {
            for (int f = 0; f < HOPF_FRAME_COUNT; f++) {
                for (int p = 0; p < NumPillars; p++) {
                    thoughtStaging_[i * 512 + f * NumPillars + p] =
                        vn::fp20_t(hopfStates[i].frames[f][p]).raw();
                }
            }
        }
        mapAndUpload(device_, thoughtStateMemory_, thoughtStaging_.data(),
                     count * 512 * sizeof(int64_t));
    }

    // Upload WHT coefficients (1024 floats per entity)
    {
        for (uint32_t i = 0; i < count; i++) {
            for (int f = 0; f < HOPF_FRAME_COUNT; f++) {
                for (int c = 0; c < WHT_N; c++) {
                    whtStaging_[i * 1024 + f * WHT_N + c] = hopfStates[i].frame_wht[f][c];
                }
            }
        }
        mapAndUpload(device_, whtCoeffsMemory_, whtStaging_.data(),
                     count * 1024 * sizeof(float));
    }

    // Upload membrane (32 floats per entity)
    {
        for (uint32_t i = 0; i < count; i++) {
            for (int j = 0; j < HOPF_BASE_DIM; j++) {
                membraneStaging_[i * HOPF_BASE_DIM + j] = hopfStates[i].membrane[j];
            }
        }
        mapAndUpload(device_, membraneMemory_, membraneStaging_.data(),
                     count * HOPF_BASE_DIM * sizeof(float));
    }

    // Upload fiber coherence (256 floats per entity)
    {
        for (uint32_t i = 0; i < count; i++) {
            for (int j = 0; j < HOPF_MAX_FIBERS; j++) {
                fiberStaging_[i * HOPF_MAX_FIBERS + j] =
                    hopfStates[i].fibers[j].coherence;
            }
        }
        mapAndUpload(device_, fiberCoherenceMemory_, fiberStaging_.data(),
                     count * HOPF_MAX_FIBERS * sizeof(float));
    }

    // Upload scalar metadata
    {
        for (uint32_t i = 0; i < count; i++) {
            complexityStaging_[i] = hopfStates[i].relational_complexity;
            depthStaging_[i] = hopfStates[i].depth_buffer;
            constraintStaging_[i] = hopfStates[i].constraint_level;
            activeStaging_[i] = 1u;
            ruptureStaging_[i] = hopfStates[i].in_rupture ? 1u : 0u;
        }
        mapAndUpload(device_, relComplexityMemory_, complexityStaging_.data(),
                     count * sizeof(float));
        mapAndUpload(device_, depthBufferMemory_, depthStaging_.data(),
                     count * sizeof(float));
        mapAndUpload(device_, constraintLevelMemory_, constraintStaging_.data(),
                     count * sizeof(float));
        mapAndUpload(device_, activeMemory_, activeStaging_.data(),
                     count * sizeof(uint32_t));
        mapAndUpload(device_, inRuptureMemory_, ruptureStaging_.data(),
                     count * sizeof(uint32_t));
    }

    return true;
}

bool HopfCompute::dispatch(float dt, uint32_t mode,
                            float planckThetaMin, float complexityCap) {
    if (!ready_) return false;

    VkCommandBufferBeginInfo cbbi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    cbbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(computeCmdBuffer_, &cbbi) != VK_SUCCESS)
        return false;

    vkCmdBindPipeline(computeCmdBuffer_, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_);
    vkCmdBindDescriptorSets(computeCmdBuffer_, VK_PIPELINE_BIND_POINT_COMPUTE,
                            pipelineLayout_, 0, 1, &descriptorSet_, 0, nullptr);

    // Push constants: entity_count, dt, mode, planck_theta_min, complexity_cap
    struct PushData {
        uint32_t entityCount; float dt; uint32_t mode;
        float planckThetaMin; float complexityCap;
    };
    PushData pd = {maxEntities_, dt, mode, planckThetaMin, complexityCap};
    vkCmdPushConstants(computeCmdBuffer_, pipelineLayout_, VK_SHADER_STAGE_COMPUTE_BIT,
                       0, sizeof(PushData), &pd);

    uint32_t groupCount = (maxEntities_ + 255) / 256;
    vkCmdDispatch(computeCmdBuffer_, groupCount, 1, 1);

    if (vkEndCommandBuffer(computeCmdBuffer_) != VK_SUCCESS)
        return false;

    VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers = &computeCmdBuffer_;
    if (vkQueueSubmit(computeQueue_, 1, &si, computeFence_) != VK_SUCCESS)
        return false;

    vkWaitForFences(device_, 1, &computeFence_, VK_TRUE, UINT64_MAX);
    vkResetFences(device_, 1, &computeFence_);

    return true;
}

bool HopfCompute::download(std::vector<HopfPIDState>& hopfStates) {
    if (!ready_) return false;

    uint32_t count = std::min(static_cast<uint32_t>(hopfStates.size()), maxEntities_);
    if (count == 0) return true;

    auto mapAndRead = [&](VkDeviceMemory memory, void* dst, VkDeviceSize size) {
        void* src;
        vkMapMemory(device_, memory, 0, size, 0, &src);
        memcpy(dst, src, static_cast<size_t>(size));
        vkUnmapMemory(device_, memory);
    };

    // Download thought state
    {
        mapAndRead(thoughtStateMemory_, thoughtStaging_.data(), count * 512 * sizeof(int64_t));
        for (uint32_t i = 0; i < count; i++) {
            for (int f = 0; f < HOPF_FRAME_COUNT; f++) {
                for (int p = 0; p < NumPillars; p++) {
                    int64_t raw = thoughtStaging_[i * 512 + f * NumPillars + p];
                    hopfStates[i].frames[f][p] = vn::fp20_t::from_raw(raw);
                }
            }
        }
    }

    // Download WHT coefficients
    {
        mapAndRead(whtCoeffsMemory_, whtStaging_.data(), count * 1024 * sizeof(float));
        for (uint32_t i = 0; i < count; i++) {
            for (int f = 0; f < HOPF_FRAME_COUNT; f++) {
                for (int c = 0; c < WHT_N; c++) {
                    hopfStates[i].frame_wht[f][c] = whtStaging_[i * 1024 + f * WHT_N + c];
                }
            }
        }
    }

    // Download membrane
    {
        mapAndRead(membraneMemory_, membraneStaging_.data(), count * HOPF_BASE_DIM * sizeof(float));
        for (uint32_t i = 0; i < count; i++) {
            for (int j = 0; j < HOPF_BASE_DIM; j++) {
                hopfStates[i].membrane[j] = membraneStaging_[i * HOPF_BASE_DIM + j];
            }
        }
    }

    // Download fiber coherence
    {
        mapAndRead(fiberCoherenceMemory_, fiberStaging_.data(),
                   count * HOPF_MAX_FIBERS * sizeof(float));
        for (uint32_t i = 0; i < count; i++) {
            for (int j = 0; j < HOPF_MAX_FIBERS; j++) {
                hopfStates[i].fibers[j].coherence = fiberStaging_[i * HOPF_MAX_FIBERS + j];
            }
        }
    }

    // Download scalar metadata
    {
        mapAndRead(relComplexityMemory_, complexityStaging_.data(), count * sizeof(float));
        mapAndRead(depthBufferMemory_, depthStaging_.data(), count * sizeof(float));
        mapAndRead(constraintLevelMemory_, constraintStaging_.data(), count * sizeof(float));
        mapAndRead(activeMemory_, activeStaging_.data(), count * sizeof(uint32_t));
        mapAndRead(inRuptureMemory_, ruptureStaging_.data(), count * sizeof(uint32_t));

        for (uint32_t i = 0; i < count; i++) {
            hopfStates[i].relational_complexity = complexityStaging_[i];
            hopfStates[i].depth_buffer = depthStaging_[i];
            hopfStates[i].constraint_level = constraintStaging_[i];
            hopfStates[i].in_rupture = ruptureStaging_[i] != 0;
        }
    }

    return true;
}

void HopfCompute::cleanup() {
    if (device_ == VK_NULL_HANDLE) return;

    auto destroy = [&](VkBuffer& buf, VkDeviceMemory& mem) {
        if (buf) { vkDestroyBuffer(device_, buf, nullptr); buf = VK_NULL_HANDLE; }
        if (mem) { vkFreeMemory(device_, mem, nullptr); mem = VK_NULL_HANDLE; }
    };

    destroy(thoughtStateBuffer_, thoughtStateMemory_);
    destroy(whtCoeffsBuffer_, whtCoeffsMemory_);
    destroy(membraneBuffer_, membraneMemory_);
    destroy(fiberCoherenceBuffer_, fiberCoherenceMemory_);
    destroy(relComplexityBuffer_, relComplexityMemory_);
    destroy(depthBufferBuffer_, depthBufferMemory_);
    destroy(constraintLevelBuffer_, constraintLevelMemory_);
    destroy(activeBuffer_, activeMemory_);
    destroy(inRuptureBuffer_, inRuptureMemory_);

    if (descriptorSetLayout_) {
        vkDestroyDescriptorSetLayout(device_, descriptorSetLayout_, nullptr);
        descriptorSetLayout_ = VK_NULL_HANDLE;
    }
    if (descriptorPool_) {
        vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
        descriptorPool_ = VK_NULL_HANDLE;
    }
    if (pipelineLayout_) {
        vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
        pipelineLayout_ = VK_NULL_HANDLE;
    }
    if (pipeline_) {
        vkDestroyPipeline(device_, pipeline_, nullptr);
        pipeline_ = VK_NULL_HANDLE;
    }
    if (shaderModule_) {
        vkDestroyShaderModule(device_, shaderModule_, nullptr);
        shaderModule_ = VK_NULL_HANDLE;
    }

    if (computeCmdPool_) {
        if (computeCmdBuffer_) {
            vkFreeCommandBuffers(device_, computeCmdPool_, 1, &computeCmdBuffer_);
            computeCmdBuffer_ = VK_NULL_HANDLE;
        }
        vkDestroyCommandPool(device_, computeCmdPool_, nullptr);
        computeCmdPool_ = VK_NULL_HANDLE;
    }
    if (computeFence_) {
        vkDestroyFence(device_, computeFence_, nullptr);
        computeFence_ = VK_NULL_HANDLE;
    }

    ready_ = false;
}
