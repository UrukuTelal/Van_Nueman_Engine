// Van Nueman Vulkan Renderer Header
#ifndef VULKAN_RENDERER_H
#define VULKAN_RENDERER_H

#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>

struct SkellyPushConstants {
    uint32_t bone_count;
    uint32_t muscle_count;
    uint32_t organ_count;
};
#include "../include/SkellyGPU.h"
#include "../fll/include/FLLShaders.h"

#define MAX_FRAMES_IN_FLIGHT 2

struct CameraUBO {
    float position[3];
    float fov;
    float forward[3];
    float aspect;
    float right[3];
    float _pad1;
    float up[3];
    float _pad2;
    uint32_t width;
    uint32_t height;
    uint32_t _pad3[2];
};

struct SVO_Node_Render {
    uint32_t children[8];
    uint8_t valid_mask;
    uint8_t material;
    uint8_t r, g, b, a;
};

class VulkanRenderer {
public:
    VulkanRenderer();
    ~VulkanRenderer();

    bool init(uint32_t width, uint32_t height, const char* title);
    void cleanup();

    bool beginFrame();
    bool endFrame();
    bool render();
    bool shouldClose() const;

    void onWindowResized(uint32_t w, uint32_t h);

    void setDebug(bool enabled) { debugMode = enabled; }
    bool isDebug() const { return debugMode; }
    uint32_t getWidth() const { return width; }
    uint32_t getHeight() const { return height; }

    bool uploadSVO(SVO_Node_Render* nodes, uint32_t node_count);
    bool uploadBones(BoneGPU* bones, uint32_t count);
    bool uploadMuscles(MuscleGPU* muscles, uint32_t count);
    bool uploadOrgans(OrganGPU* organs, uint32_t count);
    bool uploadAgents(const float* positions, const float* colors, uint32_t count,
                      const float* sizes = nullptr, const float* alphas = nullptr);
    void updateCamera(const CameraUBO& cam);
    void* getWindow() const { return window; }

    // ── FLL Glyph Rendering ─────────────────────────────────────
    bool createFLLPipeline();
    bool createSkellyPipeline();
    bool uploadFLLNodes(const void* node_data, uint32_t node_count, size_t node_stride);
    void dispatchFLL(VkCommandBuffer cmd, const vn::fll::FLLPushConstants& pc);
    void setFLLPushConstants(const vn::fll::FLLPushConstants& pc)
        { fllPushConstants_ = pc; hasFLLPushConstants_ = true; }
    
    // Vulkan handle getters for ImGui integration
    VkInstance getVkInstance() const { return instance; }
    VkPhysicalDevice getVkPhysicalDevice() const { return physicalDevice; }
    VkDevice getVkDevice() const { return device; }
    VkQueue getVkQueue() const { return graphicsQueue; }
    VkQueue getVkComputeQueue() const { return computeQueue; }
    uint32_t getVkQueueFamily() const { return graphicsFamily; }
    uint32_t getVkComputeFamily() const { return computeFamily; }
    VkRenderPass getVkRenderPass() const { return renderPass; }
    VkCommandBuffer getVkCommandBuffer() const { return commandBuffers ? commandBuffers[currentFrame] : VK_NULL_HANDLE; }
    VkDescriptorPool getVkDescriptorPool() const { return descriptorPool; }

private:
    bool initVulkan();
    bool createSwapchain();
    bool createRenderPass();
    bool createComputePipeline();
    bool createOutputImage();
    bool createDescriptorPoolAndSets();
    bool createGraphicsPipeline();
    bool createFramebuffers();
    bool createBuffers();
    bool createCommandBuffers();
    void cleanupSwapchain();

    // Vulkan core
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue computeQueue = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    int graphicsFamily = -1, computeFamily = -1, presentFamily = -1;

    // Swapchain
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    uint32_t imageCount = 0;
    VkImage* swapchainImages = nullptr;
    VkImageView* swapchainImageViews = nullptr;
    VkFormat swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;
    uint32_t width = 0, height = 0;

    // Render pass & pipeline
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline = VK_NULL_HANDLE;
    VkDescriptorSetLayout dsLayoutGraphics = VK_NULL_HANDLE;

    // Compute pipeline
    VkPipelineLayout computePipelineLayout = VK_NULL_HANDLE;
    VkPipeline computePipeline = VK_NULL_HANDLE;

    // Descriptors
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VkDescriptorSet graphicsDescriptorSet = VK_NULL_HANDLE;

    // Buffers
    VkBuffer cameraBuffer = VK_NULL_HANDLE;
    VkDeviceMemory cameraBufferMemory = VK_NULL_HANDLE;
    VkBuffer svoBuffer = VK_NULL_HANDLE;
    VkDeviceMemory svoBufferMemory = VK_NULL_HANDLE;
    uint32_t svoNodeCount = 0;

    // Skelly GPU buffers
    VkBuffer boneBuffer = VK_NULL_HANDLE;
    VkDeviceMemory boneBufferMemory = VK_NULL_HANDLE;
    uint32_t boneCount = 0;
    VkBuffer muscleBuffer = VK_NULL_HANDLE;
    VkDeviceMemory muscleBufferMemory = VK_NULL_HANDLE;
    uint32_t muscleCount = 0;
    VkBuffer organBuffer = VK_NULL_HANDLE;
    VkDeviceMemory organBufferMemory = VK_NULL_HANDLE;
    uint32_t organCount = 0;

    // Agent marker buffer
    VkBuffer agentBuffer = VK_NULL_HANDLE;
    VkDeviceMemory agentBufferMemory = VK_NULL_HANDLE;
    uint32_t agentBufferCapacity = 0;

    // Output image (compute shader writes here)
    VkImage outputImage = VK_NULL_HANDLE;
    VkDeviceMemory outputImageMemory = VK_NULL_HANDLE;
    VkImageView outputImageView = VK_NULL_HANDLE;
    VkSampler outputSampler = VK_NULL_HANDLE;

    // ── FLL Rendering State ─────────────────────────────────────
    VkPipelineLayout fllPipelineLayout = VK_NULL_HANDLE;
    VkPipeline fllComputePipeline = VK_NULL_HANDLE;
    VkDescriptorSetLayout fllDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet fllDescriptorSet = VK_NULL_HANDLE;
    VkBuffer fllNodeBuffer = VK_NULL_HANDLE;
    VkDeviceMemory fllNodeBufferMemory = VK_NULL_HANDLE;
    uint32_t fllNodeCapacity = 0;
    vn::fll::FLLPushConstants fllPushConstants_{};
    bool hasFLLPushConstants_ = false;

    // ── Skelly SDF Rendering State ──────────────────────────────
    VkPipelineLayout skellyPipelineLayout = VK_NULL_HANDLE;
    VkPipeline skellyPipeline = VK_NULL_HANDLE;
    VkDescriptorSetLayout skellyDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet skellyDescriptorSet = VK_NULL_HANDLE;

    // Framebuffers
    std::vector<VkFramebuffer> swapchainFramebuffers;

    // Command & sync
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandBuffer* commandBuffers = nullptr;
    VkSemaphore imageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT] = {};
    VkSemaphore renderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT] = {};
    VkFence inFlightFences[MAX_FRAMES_IN_FLIGHT] = {};
    uint32_t currentFrame = 0;

    // Window
    void* window;
    bool debugMode = true;  // Debug output enabled by default
    bool framebufferResized = false;
};

#endif // VULKAN_RENDERER_H
