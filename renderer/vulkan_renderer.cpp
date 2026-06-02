// Van Nueman Vulkan Renderer - Simplified Compute Version
#include "vulkan_renderer.h"
#include <iostream>
#include <vector>
#include <cstring>
#include <fstream>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define VK_CHECK(x) do { VkResult _r = x; if (_r) { std::cerr << "VK error " << _r << " at line " << __LINE__ << std::endl; return false; } } while(0)

static std::vector<uint32_t> readSPIRV(const char* filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) { std::cerr << "Failed to open SPIR-V: " << filename << std::endl; return {}; }
    size_t size = file.tellg(); file.seekg(0);
    std::vector<uint32_t> buffer((size + 3) / 4);
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    return buffer;
}

static VulkanRenderer* g_renderer = nullptr;

static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    if (g_renderer) g_renderer->onWindowResized(width, height);
}

VulkanRenderer::VulkanRenderer() : instance(VK_NULL_HANDLE), physicalDevice(VK_NULL_HANDLE),
    device(VK_NULL_HANDLE), graphicsQueue(VK_NULL_HANDLE), computeQueue(VK_NULL_HANDLE),
    surface(VK_NULL_HANDLE), swapchain(VK_NULL_HANDLE),
    renderPass(VK_NULL_HANDLE), pipelineLayout(VK_NULL_HANDLE), graphicsPipeline(VK_NULL_HANDLE),
    dsLayoutGraphics(VK_NULL_HANDLE),
    computePipelineLayout(VK_NULL_HANDLE), computePipeline(VK_NULL_HANDLE),
    descriptorSetLayout(VK_NULL_HANDLE), descriptorPool(VK_NULL_HANDLE),
    descriptorSet(VK_NULL_HANDLE),
    cameraBuffer(VK_NULL_HANDLE), cameraBufferMemory(VK_NULL_HANDLE),
    svoBuffer(VK_NULL_HANDLE), svoBufferMemory(VK_NULL_HANDLE),
    outputImage(VK_NULL_HANDLE), outputImageMemory(VK_NULL_HANDLE),
    outputImageView(VK_NULL_HANDLE), outputSampler(VK_NULL_HANDLE),
    commandPool(VK_NULL_HANDLE), window(nullptr),
    swapchainImages(nullptr), swapchainImageViews(nullptr), width(0), height(0),
    svoNodeCount(0), swapchainImageFormat(VK_FORMAT_B8G8R8A8_UNORM), currentFrame(0),
    graphicsFamily(-1), computeFamily(-1), presentFamily(-1), imageCount(0) {
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        imageAvailableSemaphores[i] = VK_NULL_HANDLE;
        renderFinishedSemaphores[i] = VK_NULL_HANDLE;
        inFlightFences[i] = VK_NULL_HANDLE;
    }
}

VulkanRenderer::~VulkanRenderer() { cleanup(); }

bool VulkanRenderer::init(uint32_t w, uint32_t h, const char* title) {
    width = w; height = h;
    g_renderer = this;
    std::cerr << "1. Init GLFW" << std::endl;
    if (!glfwInit()) { std::cerr << "GLFW init failed" << std::endl; return false; }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!window) { std::cerr << "Window creation failed" << std::endl; return false; }
    glfwSetFramebufferSizeCallback((GLFWwindow*)window, framebufferResizeCallback);

    if (!initVulkan()) return false;
    if (!createSwapchain()) return false;
    if (!createRenderPass()) return false;
    if (!createComputePipeline()) return false;
    if (!createOutputImage()) return false;
    if (!createDescriptorPoolAndSets()) return false;
    if (!createGraphicsPipeline()) return false;
    if (!createFramebuffers()) return false;
    if (!createBuffers()) return false;
    if (!createCommandBuffers()) return false;

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkSemaphoreCreateInfo sci = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VkFenceCreateInfo fci = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        VK_CHECK(vkCreateSemaphore(device, &sci, nullptr, &imageAvailableSemaphores[i]));
        VK_CHECK(vkCreateSemaphore(device, &sci, nullptr, &renderFinishedSemaphores[i]));
        VK_CHECK(vkCreateFence(device, &fci, nullptr, &inFlightFences[i]));
    }
    std::cerr << "Init complete!" << std::endl;
    return true;
}

bool VulkanRenderer::initVulkan() {
    VkApplicationInfo ai = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    ai.pApplicationName = "VanNueman"; ai.apiVersion = VK_API_VERSION_1_2;

    uint32_t extCount = 0;
    const char** exts = glfwGetRequiredInstanceExtensions(&extCount);
    std::vector<const char*> extensions(exts, exts + extCount);

    VkInstanceCreateInfo ii = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ii.pApplicationInfo = &ai; ii.enabledExtensionCount = extCount; ii.ppEnabledExtensionNames = exts;
    VK_CHECK(vkCreateInstance(&ii, nullptr, &instance));
    std::cerr << "Instance created" << std::endl;

    VK_CHECK(glfwCreateWindowSurface(instance, (GLFWwindow*)window, nullptr, &surface));
    std::cerr << "Surface created" << std::endl;

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (!deviceCount) return false;
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
    physicalDevice = devices[0];
    std::cerr << "Physical device found" << std::endl;

    uint32_t qfCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &qfCount, nullptr);
    std::vector<VkQueueFamilyProperties> qfamilies(qfCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &qfCount, qfamilies.data());

    for (int i = 0; i < (int)qfCount; i++) {
        if (qfamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) graphicsFamily = i;
        if (qfamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) computeFamily = i;
        VkBool32 present = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &present);
        if (present) presentFamily = i;
    }
    std::cerr << "Queue families: graphics=" << graphicsFamily << " compute=" << computeFamily << " present=" << presentFamily << std::endl;

    std::vector<VkDeviceQueueCreateInfo> qcis;
    float priority = 1.0f;
    auto addQueue = [&](int f) {
        if (f < 0) return;
        for (auto& q : qcis) if ((int)q.queueFamilyIndex == f) return;
        VkDeviceQueueCreateInfo q = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        q.queueFamilyIndex = f; q.queueCount = 1; q.pQueuePriorities = &priority;
        qcis.push_back(q);
    };
    addQueue(graphicsFamily); addQueue(computeFamily); addQueue(presentFamily);

    VkDeviceCreateInfo di = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    di.queueCreateInfoCount = (uint32_t)qcis.size(); di.pQueueCreateInfos = qcis.data();
    const char* ext = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    di.enabledExtensionCount = 1; di.ppEnabledExtensionNames = &ext;
    VK_CHECK(vkCreateDevice(physicalDevice, &di, nullptr, &device));
    std::cerr << "Device created" << std::endl;

    vkGetDeviceQueue(device, graphicsFamily, 0, &graphicsQueue);
    vkGetDeviceQueue(device, computeFamily >= 0 ? computeFamily : graphicsFamily, 0, &computeQueue);
    std::cerr << "Queues obtained" << std::endl;

    VkCommandPoolCreateInfo cpci = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cpci.queueFamilyIndex = graphicsFamily;
    VK_CHECK(vkCreateCommandPool(device, &cpci, nullptr, &commandPool));
    std::cerr << "Command pool created" << std::endl;
    return true;
}

bool VulkanRenderer::createSwapchain() {
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &caps);

    width = caps.currentExtent.width == 0xFFFFFFFF ? width : caps.currentExtent.width;
    height = caps.currentExtent.height == 0xFFFFFFFF ? height : caps.currentExtent.height;

    uint32_t pmCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &pmCount, nullptr);
    std::vector<VkPresentModeKHR> pms(pmCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &pmCount, pms.data());
    VkPresentModeKHR pm = VK_PRESENT_MODE_FIFO_KHR;
    for (auto m : pms) if (m == VK_PRESENT_MODE_MAILBOX_KHR) { pm = m; break; }

    VkSwapchainCreateInfoKHR sci = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    sci.surface = surface; sci.minImageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && sci.minImageCount > caps.maxImageCount)
        sci.minImageCount = caps.maxImageCount;
    sci.imageFormat = swapchainImageFormat; sci.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    sci.imageExtent = {width, height}; sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    sci.preTransform = caps.currentTransform; sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode = pm; sci.clipped = VK_TRUE;
    VK_CHECK(vkCreateSwapchainKHR(device, &sci, nullptr, &swapchain));
    std::cerr << "Swapchain created" << std::endl;

    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
    swapchainImages = new VkImage[imageCount];
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages);
    swapchainImageViews = new VkImageView[imageCount];
    for (uint32_t i = 0; i < imageCount; i++) {
        VkImageViewCreateInfo ivci = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        ivci.image = swapchainImages[i]; ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ivci.format = swapchainImageFormat; ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ivci.subresourceRange.levelCount = 1; ivci.subresourceRange.layerCount = 1;
        VK_CHECK(vkCreateImageView(device, &ivci, nullptr, &swapchainImageViews[i]));
    }
    std::cerr << "Swapchain images created: " << imageCount << std::endl;
    return true;
}

bool VulkanRenderer::createRenderPass() {
    VkAttachmentDescription color = {};
    color.format = swapchainImageFormat; color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkAttachmentReference ref = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass = {}; subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1; subpass.pColorAttachments = &ref;
    VkRenderPassCreateInfo rpci = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rpci.attachmentCount = 1; rpci.pAttachments = &color; rpci.subpassCount = 1; rpci.pSubpasses = &subpass;
    VK_CHECK(vkCreateRenderPass(device, &rpci, nullptr, &renderPass));
    std::cerr << "Render pass created" << std::endl;

    return true;
}

bool VulkanRenderer::createComputePipeline() {
    std::vector<std::string> paths = {
        "kernels/render_vulkan.spv",
        "../kernels/render_vulkan.spv",
        "render_vulkan.spv"
    };
    std::vector<uint32_t> spirv;
    for (auto& path : paths) {
        spirv = readSPIRV(path.c_str());
        if (!spirv.empty()) { std::cerr << "Loaded shader: " << path << std::endl; break; }
    }
    if (spirv.empty()) { std::cerr << "Failed to load render_vulkan.spv" << std::endl; return false; }

    VkShaderModuleCreateInfo smci = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    smci.codeSize = spirv.size() * 4; smci.pCode = spirv.data();
    VkShaderModule shaderModule;
    VK_CHECK(vkCreateShaderModule(device, &smci, nullptr, &shaderModule));
    std::cerr << "Shader module created" << std::endl;

    VkDescriptorSetLayoutBinding bindings[6] = {};
    bindings[0] = {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT};
    bindings[1] = {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT};
    bindings[2] = {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT};
    bindings[3] = {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT};
    bindings[4] = {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT};
    bindings[5] = {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT};

    VkDescriptorSetLayoutCreateInfo dslci = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dslci.bindingCount = 6; dslci.pBindings = bindings;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &dslci, nullptr, &descriptorSetLayout));
    std::cerr << "Compute descriptor set layout created" << std::endl;

    VkPipelineLayoutCreateInfo plci = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plci.setLayoutCount = 1; plci.pSetLayouts = &descriptorSetLayout;
    VK_CHECK(vkCreatePipelineLayout(device, &plci, nullptr, &computePipelineLayout));
    std::cerr << "Compute pipeline layout created" << std::endl;

    VkPipelineShaderStageCreateInfo ssci = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    ssci.stage = VK_SHADER_STAGE_COMPUTE_BIT; ssci.module = shaderModule; ssci.pName = "main";

    VkComputePipelineCreateInfo cpci = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    cpci.stage = ssci; cpci.layout = computePipelineLayout;
    VkResult r = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &cpci, nullptr, &computePipeline);
    std::cerr << "Compute pipeline result: " << r << std::endl;
    if (r) return false;
    vkDestroyShaderModule(device, shaderModule, nullptr);
    std::cerr << "Compute pipeline created" << std::endl;
    return true;
}

bool VulkanRenderer::createOutputImage() {
    // Use RGBA8 format for compatibility with both compute write and fragment sample
    VkImageCreateInfo ici = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ici.imageType = VK_IMAGE_TYPE_2D; ici.format = VK_FORMAT_R8G8B8A8_UNORM;
    ici.extent = {width, height, 1}; ici.mipLevels = 1; ici.arrayLayers = 1;
    ici.samples = VK_SAMPLE_COUNT_1_BIT; ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VK_CHECK(vkCreateImage(device, &ici, nullptr, &outputImage));
    std::cerr << "Output image created" << std::endl;

    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(device, outputImage, &mr);
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
    uint32_t memTypeIndex = 0;
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if (mr.memoryTypeBits & (1 << i)) {
            if (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
                memTypeIndex = i; break;
            }
        }
    }
    VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.allocationSize = mr.size; mai.memoryTypeIndex = memTypeIndex;
    VK_CHECK(vkAllocateMemory(device, &mai, nullptr, &outputImageMemory));
    vkBindImageMemory(device, outputImage, outputImageMemory, 0);
    std::cerr << "Output image memory allocated" << std::endl;

    VkImageViewCreateInfo ivci = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    ivci.image = outputImage; ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ivci.format = VK_FORMAT_R8G8B8A8_UNORM; ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ivci.subresourceRange.levelCount = 1; ivci.subresourceRange.layerCount = 1;
    VK_CHECK(vkCreateImageView(device, &ivci, nullptr, &outputImageView));
    std::cerr << "Output image view created" << std::endl;

    VkSamplerCreateInfo sci = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sci.magFilter = VK_FILTER_LINEAR; sci.minFilter = VK_FILTER_LINEAR;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    VK_CHECK(vkCreateSampler(device, &sci, nullptr, &outputSampler));
    std::cerr << "Output sampler created" << std::endl;

    return true;
}

bool VulkanRenderer::createDescriptorPoolAndSets() {
    VkDescriptorPoolSize ps[4] = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}
    };
    VkDescriptorPoolCreateInfo dpci = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    dpci.maxSets = 2; dpci.poolSizeCount = 4; dpci.pPoolSizes = ps;
    VK_CHECK(vkCreateDescriptorPool(device, &dpci, nullptr, &descriptorPool));
    std::cerr << "Descriptor pool created" << std::endl;

    // Allocate compute descriptor set
    VkDescriptorSetAllocateInfo dsai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dsai.descriptorPool = descriptorPool; dsai.descriptorSetCount = 1;
    dsai.pSetLayouts = &descriptorSetLayout;
    VK_CHECK(vkAllocateDescriptorSets(device, &dsai, &descriptorSet));
    std::cerr << "Compute descriptor set allocated" << std::endl;

    // Only update descriptors that have valid handles (camera + output image)
    VkDescriptorBufferInfo cameraInfo = {cameraBuffer, 0, sizeof(CameraUBO)};
    VkDescriptorImageInfo imageInfo = {VK_NULL_HANDLE, outputImageView, VK_IMAGE_LAYOUT_GENERAL};

    VkWriteDescriptorSet writes[2] = {};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = descriptorSet; writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1; writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].pBufferInfo = &cameraInfo;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = descriptorSet; writes[1].dstBinding = 2;
    writes[1].descriptorCount = 1; writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[1].pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(device, 2, writes, 0, nullptr);
    std::cerr << "Compute descriptor sets updated (camera + output image only)" << std::endl;
    std::cerr << "Skelly buffers will be updated when uploaded" << std::endl;

    // Create graphics descriptor set layout (must match fragment shader binding 2)
    VkDescriptorSetLayoutBinding gBinding = {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT};
    VkDescriptorSetLayoutCreateInfo gdslci = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    gdslci.bindingCount = 1; gdslci.pBindings = &gBinding;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &gdslci, nullptr, &dsLayoutGraphics));
    std::cerr << "Graphics descriptor set layout created" << std::endl;

    // Allocate graphics descriptor set
    dsai.pSetLayouts = &dsLayoutGraphics;
    VK_CHECK(vkAllocateDescriptorSets(device, &dsai, &graphicsDescriptorSet));
    std::cerr << "Graphics descriptor set allocated" << std::endl;

    // Update graphics descriptor set with output image sampler
    VkDescriptorImageInfo outputInfo = {outputSampler, outputImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet gWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    gWrite.dstSet = graphicsDescriptorSet; gWrite.dstBinding = 2;  // Match compute shader binding 2
    gWrite.descriptorCount = 1; gWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    gWrite.pImageInfo = &outputInfo;
    vkUpdateDescriptorSets(device, 1, &gWrite, 0, nullptr);
    std::cerr << "Graphics descriptor set updated (binding 2)" << std::endl;

    return true;
}

bool VulkanRenderer::createGraphicsPipeline() {
    // Load compiled shaders - try multiple paths
    std::vector<std::string> vert_paths = {
        "fullscreen.vert.spv",
        "kernels/fullscreen.vert.spv",
        "../kernels/fullscreen.vert.spv"
    };
    
    std::vector<std::string> frag_paths = {
        "display.frag.spv",
        "kernels/display.frag.spv",
        "../kernels/display.frag.spv"
    };
    
    std::vector<uint32_t> vertSpirv, fragSpirv;
    for (auto& path : vert_paths) {
        vertSpirv = readSPIRV(path.c_str());
        if (!vertSpirv.empty()) { std::cerr << "Loaded vertex shader: " << path << std::endl; break; }
    }
    
    for (auto& path : frag_paths) {
        fragSpirv = readSPIRV(path.c_str());
        if (!fragSpirv.empty()) { std::cerr << "Loaded fragment shader: " << path << std::endl; break; }
    }

    if (vertSpirv.empty() || fragSpirv.empty()) {
        std::cerr << "Failed to load graphics shaders" << std::endl;
        return false;
    }

    // Create shader modules
    VkShaderModule vertModule, fragModule;
    VkShaderModuleCreateInfo smci = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};

    smci.codeSize = vertSpirv.size() * 4; smci.pCode = vertSpirv.data();
    VK_CHECK(vkCreateShaderModule(device, &smci, nullptr, &vertModule));

    smci.codeSize = fragSpirv.size() * 4; smci.pCode = fragSpirv.data();
    VK_CHECK(vkCreateShaderModule(device, &smci, nullptr, &fragModule));

    // Pipeline layout uses graphics descriptor set layout
    VkPipelineLayoutCreateInfo plci = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plci.setLayoutCount = 1; plci.pSetLayouts = &dsLayoutGraphics;
    VK_CHECK(vkCreatePipelineLayout(device, &plci, nullptr, &pipelineLayout));
    std::cerr << "Graphics pipeline layout created" << std::endl;

    // Shader stages
    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT; stages[0].module = vertModule;
    stages[0].pName = "main";

    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module = fragModule;
    stages[1].pName = "main";

    // Vertex input - none (fullscreen triangle trick)
    VkPipelineVertexInputStateCreateInfo vertexInput = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

    // Input assembly - triangle list
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // Viewport/scissor
    VkViewport viewport = {0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f};
    VkRect2D scissor = {{0, 0}, {width, height}};
    VkPipelineViewportStateCreateInfo viewportState = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportState.viewportCount = 1; viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1; viewportState.pScissors = &scissor;

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL; rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE; rasterizer.lineWidth = 1.0f;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Color blend
    VkPipelineColorBlendAttachmentState blendAttach = {};
    blendAttach.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo colorBlend = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    colorBlend.attachmentCount = 1; colorBlend.pAttachments = &blendAttach;

    // Create graphics pipeline
    VkGraphicsPipelineCreateInfo pci = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pci.stageCount = 2; pci.pStages = stages;
    pci.pVertexInputState = &vertexInput; pci.pInputAssemblyState = &inputAssembly;
    pci.pViewportState = &viewportState; pci.pRasterizationState = &rasterizer;
    pci.pMultisampleState = &multisampling; pci.pColorBlendState = &colorBlend;
    pci.layout = pipelineLayout; pci.renderPass = renderPass; pci.subpass = 0;

    VkResult r = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pci, nullptr, &graphicsPipeline);
    vkDestroyShaderModule(device, vertModule, nullptr);
    vkDestroyShaderModule(device, fragModule, nullptr);

    std::cerr << "Graphics pipeline result: " << r << std::endl;
    if (r) return false;
    std::cerr << "Graphics pipeline created" << std::endl;
    return true;
}

bool VulkanRenderer::createFramebuffers() {
    swapchainFramebuffers.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; i++) {
        VkFramebufferCreateInfo fbci = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fbci.renderPass = renderPass; fbci.attachmentCount = 1; fbci.pAttachments = &swapchainImageViews[i];
        fbci.width = width; fbci.height = height; fbci.layers = 1;
        VK_CHECK(vkCreateFramebuffer(device, &fbci, nullptr, &swapchainFramebuffers[i]));
    }
    std::cerr << "Framebuffers created" << std::endl;
    return true;
}

bool VulkanRenderer::createBuffers() {
    VkBufferCreateInfo bci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bci.size = sizeof(CameraUBO); bci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    VK_CHECK(vkCreateBuffer(device, &bci, nullptr, &cameraBuffer));
    VkMemoryRequirements mr;
    vkGetBufferMemoryRequirements(device, cameraBuffer, &mr);
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
    uint32_t memTypeIndex = 0;
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if (mr.memoryTypeBits & (1 << i)) {
            if (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
                memTypeIndex = i; break;
            }
        }
    }
    VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.allocationSize = mr.size; mai.memoryTypeIndex = memTypeIndex;
    VK_CHECK(vkAllocateMemory(device, &mai, nullptr, &cameraBufferMemory));
    vkBindBufferMemory(device, cameraBuffer, cameraBufferMemory, 0);
    std::cerr << "Camera buffer created" << std::endl;
    return true;
}

bool VulkanRenderer::createCommandBuffers() {
    commandBuffers = new VkCommandBuffer[MAX_FRAMES_IN_FLIGHT];
    VkCommandBufferAllocateInfo cbai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cbai.commandPool = commandPool; cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
    VK_CHECK(vkAllocateCommandBuffers(device, &cbai, commandBuffers));
    std::cerr << "Command buffers allocated" << std::endl;
    return true;
}

bool VulkanRenderer::uploadSVO(SVO_Node_Render* nodes, uint32_t node_count) {
    std::cerr << "uploadSVO: creating buffer..." << std::endl;
    svoNodeCount = node_count ? node_count : 1;
    VkBufferCreateInfo bci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bci.size = sizeof(SVO_Node_Render) * svoNodeCount; bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    VK_CHECK(vkCreateBuffer(device, &bci, nullptr, &svoBuffer));
    std::cerr << "uploadSVO: getting memory requirements..." << std::endl;
    VkMemoryRequirements mr;
    vkGetBufferMemoryRequirements(device, svoBuffer, &mr);
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
    uint32_t memTypeIndex = 0;
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if (mr.memoryTypeBits & (1 << i)) {
            if (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
                memTypeIndex = i; break;
            }
        }
    }
    std::cerr << "uploadSVO: allocating memory..." << std::endl;
    VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.allocationSize = mr.size; mai.memoryTypeIndex = memTypeIndex;
    VK_CHECK(vkAllocateMemory(device, &mai, nullptr, &svoBufferMemory));
    vkBindBufferMemory(device, svoBuffer, svoBufferMemory, 0);
    std::cerr << "uploadSVO: mapping memory..." << std::endl;
    void* data; VkResult mapResult = vkMapMemory(device, svoBufferMemory, 0, bci.size, 0, &data);
    if (mapResult != VK_SUCCESS) { std::cerr << "uploadSVO: vkMapMemory failed with " << mapResult << std::endl; return false; }
    std::cerr << "uploadSVO: copying data..." << std::endl;
    memcpy(data, nodes, bci.size); vkUnmapMemory(device, svoBufferMemory);
    std::cerr << "uploadSVO: updating descriptor set..." << std::endl;
    VkDescriptorBufferInfo svoInfo = {svoBuffer, 0, bci.size};
    VkWriteDescriptorSet wds = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    wds.dstSet = descriptorSet; wds.dstBinding = 1; wds.descriptorCount = 1;
    wds.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; wds.pBufferInfo = &svoInfo;
    vkUpdateDescriptorSets(device, 1, &wds, 0, nullptr);
    std::cerr << "uploadSVO: done" << std::endl;
    return true;
}

bool VulkanRenderer::uploadBones(BoneGPU* bones, uint32_t count) {
    std::cerr << "uploadBones: creating buffer..." << std::endl;
    boneCount = count;
    
    // Clean up old buffer if exists
    if (boneBuffer) {
        vkDestroyBuffer(device, boneBuffer, nullptr);
        vkFreeMemory(device, boneBufferMemory, nullptr);
    }
    
    VkBufferCreateInfo bci = {};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = sizeof(BoneGPU) * count;
    bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    std::cerr << "uploadBones: calling vkCreateBuffer..." << std::endl;
    std::cerr.flush();
    VkResult bufResult = vkCreateBuffer(device, &bci, nullptr, &boneBuffer);
    std::cerr << "uploadBones: vkCreateBuffer result=" << bufResult << " boneBuffer=" << boneBuffer << std::endl;
    std::cerr.flush();
    if (bufResult != VK_SUCCESS) return false;
    
    std::cerr << "uploadBones: getting memory requirements..." << std::endl;
    std::cerr.flush();
    VkMemoryRequirements mr;
    vkGetBufferMemoryRequirements(device, boneBuffer, &mr);
    std::cerr << "uploadBones: memory requirements: size=" << mr.size << " alignment=" << mr.alignment << " typeBits=" << mr.memoryTypeBits << std::endl;
    std::cerr.flush();
    
    std::cerr << "uploadBones: searching for memory type..." << std::endl;
    std::cerr.flush();
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
    uint32_t memTypeIndex = 0;
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if (mr.memoryTypeBits & (1 << i)) {
            if (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
                memTypeIndex = i; break;
            }
        }
    }
    VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.allocationSize = mr.size; mai.memoryTypeIndex = memTypeIndex;
    VK_CHECK(vkAllocateMemory(device, &mai, nullptr, &boneBufferMemory));
    vkBindBufferMemory(device, boneBuffer, boneBufferMemory, 0);

    void* data;
    vkMapMemory(device, boneBufferMemory, 0, bci.size, 0, &data);
    memcpy(data, bones, bci.size);
    vkUnmapMemory(device, boneBufferMemory);

    // Update descriptor set with new buffer
    VkDescriptorBufferInfo boneInfo = {boneBuffer, 0, sizeof(BoneGPU) * boneCount};
    VkWriteDescriptorSet wds = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    wds.dstSet = descriptorSet; wds.dstBinding = 3;
    wds.descriptorCount = 1; wds.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    wds.pBufferInfo = &boneInfo;
    vkUpdateDescriptorSets(device, 1, &wds, 0, nullptr);

    std::cerr << "uploadBones: done (" << count << " bones)" << std::endl;
    return true;
}

bool VulkanRenderer::uploadMuscles(MuscleGPU* muscles, uint32_t count) {
    std::cerr << "uploadMuscles: creating buffer..." << std::endl;
    muscleCount = count;
    
    // Clean up old buffer if exists
    if (muscleBuffer) {
        vkDestroyBuffer(device, muscleBuffer, nullptr);
        vkFreeMemory(device, muscleBufferMemory, nullptr);
    }
    
    VkBufferCreateInfo bci = {};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = sizeof(MuscleGPU) * count;
    bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VK_CHECK(vkCreateBuffer(device, &bci, nullptr, &muscleBuffer));
    
    VkMemoryRequirements mr;
    vkGetBufferMemoryRequirements(device, muscleBuffer, &mr);
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
    uint32_t memTypeIndex = 0;
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if (mr.memoryTypeBits & (1 << i)) {
            if (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
                memTypeIndex = i; break;
            }
        }
    }
    VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.allocationSize = mr.size; mai.memoryTypeIndex = memTypeIndex;
    VK_CHECK(vkAllocateMemory(device, &mai, nullptr, &muscleBufferMemory));
    vkBindBufferMemory(device, muscleBuffer, muscleBufferMemory, 0);

    void* data;
    vkMapMemory(device, muscleBufferMemory, 0, bci.size, 0, &data);
    memcpy(data, muscles, bci.size);
    vkUnmapMemory(device, muscleBufferMemory);

    // Update descriptor set with new buffer
    VkDescriptorBufferInfo muscleInfo = {muscleBuffer, 0, sizeof(MuscleGPU) * muscleCount};
    VkWriteDescriptorSet wds = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    wds.dstSet = descriptorSet; wds.dstBinding = 4;
    wds.descriptorCount = 1; wds.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    wds.pBufferInfo = &muscleInfo;
    vkUpdateDescriptorSets(device, 1, &wds, 0, nullptr);

    std::cerr << "uploadMuscles: done (" << count << " muscles)" << std::endl;
    return true;
}

bool VulkanRenderer::uploadOrgans(OrganGPU* organs, uint32_t count) {
    std::cerr << "uploadOrgans: creating buffer..." << std::endl;
    organCount = count;
    if (organBuffer) {
        vkDestroyBuffer(device, organBuffer, nullptr);
        vkFreeMemory(device, organBufferMemory, nullptr);
    }
    VkBufferCreateInfo bci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bci.size = sizeof(OrganGPU) * count; bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    VK_CHECK(vkCreateBuffer(device, &bci, nullptr, &organBuffer));

    VkMemoryRequirements mr;
    vkGetBufferMemoryRequirements(device, organBuffer, &mr);
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
    uint32_t memTypeIndex = 0;
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if (mr.memoryTypeBits & (1 << i)) {
            if (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
                memTypeIndex = i; break;
            }
        }
    }
    VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.allocationSize = mr.size; mai.memoryTypeIndex = memTypeIndex;
    VK_CHECK(vkAllocateMemory(device, &mai, nullptr, &organBufferMemory));
    vkBindBufferMemory(device, organBuffer, organBufferMemory, 0);

    void* data;
    vkMapMemory(device, organBufferMemory, 0, bci.size, 0, &data);
    memcpy(data, organs, bci.size);
    vkUnmapMemory(device, organBufferMemory);

    // Update descriptor set with new buffer
    VkDescriptorBufferInfo organInfo = {organBuffer, 0, sizeof(OrganGPU) * organCount};
    VkWriteDescriptorSet wds = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    wds.dstSet = descriptorSet; wds.dstBinding = 5;
    wds.descriptorCount = 1; wds.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    wds.pBufferInfo = &organInfo;
    vkUpdateDescriptorSets(device, 1, &wds, 0, nullptr);

    std::cerr << "uploadOrgans: done (" << count << " organs)" << std::endl;
    return true;
}

bool VulkanRenderer::uploadAgents(const float* positions, const float* colors, uint32_t count) {
    uint32_t stride = 8 * sizeof(float);
    uint32_t total_size = sizeof(uint32_t) + count * stride;
    uint32_t needed_capacity = count + 1;

    if (!agentBuffer || needed_capacity > agentBufferCapacity) {
        if (agentBuffer) {
            vkDestroyBuffer(device, agentBuffer, nullptr);
            vkFreeMemory(device, agentBufferMemory, nullptr);
        }
        agentBufferCapacity = needed_capacity + 64;

        VkBufferCreateInfo bci = {};
        bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bci.size = sizeof(uint32_t) + agentBufferCapacity * stride;
        bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VK_CHECK(vkCreateBuffer(device, &bci, nullptr, &agentBuffer));

        VkMemoryRequirements mr;
        vkGetBufferMemoryRequirements(device, agentBuffer, &mr);
        VkPhysicalDeviceMemoryProperties memProps;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
        uint32_t memTypeIndex = 0;
        for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
            if (mr.memoryTypeBits & (1 << i)) {
                if (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
                    memTypeIndex = i; break;
                }
            }
        }
        VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        mai.allocationSize = mr.size; mai.memoryTypeIndex = memTypeIndex;
        VK_CHECK(vkAllocateMemory(device, &mai, nullptr, &agentBufferMemory));
        vkBindBufferMemory(device, agentBuffer, agentBufferMemory, 0);

        VkDescriptorBufferInfo agentInfo = {agentBuffer, 0, bci.size};
        VkWriteDescriptorSet wds = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        wds.dstSet = descriptorSet; wds.dstBinding = 3;
        wds.descriptorCount = 1; wds.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        wds.pBufferInfo = &agentInfo;
        vkUpdateDescriptorSets(device, 1, &wds, 0, nullptr);
    }

    void* data;
    VK_CHECK(vkMapMemory(device, agentBufferMemory, 0, total_size, 0, &data));
    uint32_t* header = static_cast<uint32_t*>(data);
    header[0] = count;
    float* agents = reinterpret_cast<float*>(header + 1);
    for (uint32_t i = 0; i < count; i++) {
        agents[i * 8 + 0] = positions[i * 3 + 0];
        agents[i * 8 + 1] = positions[i * 3 + 1];
        agents[i * 8 + 2] = positions[i * 3 + 2];
        agents[i * 8 + 3] = 0.3f;
        agents[i * 8 + 4] = colors[i * 3 + 0];
        agents[i * 8 + 5] = colors[i * 3 + 1];
        agents[i * 8 + 6] = colors[i * 3 + 2];
        agents[i * 8 + 7] = 1.0f;
    }
    vkUnmapMemory(device, agentBufferMemory);
    return true;
}

void VulkanRenderer::updateCamera(const CameraUBO& cam) {
    if (!cameraBufferMemory) { std::cerr << "updateCamera: cameraBufferMemory is null!" << std::endl; return; }
    void* data; VkResult mapResult = vkMapMemory(device, cameraBufferMemory, 0, sizeof(CameraUBO), 0, &data);
    if (mapResult != VK_SUCCESS) { std::cerr << "updateCamera: vkMapMemory failed with " << mapResult << std::endl; return; }
    memcpy(data, &cam, sizeof(CameraUBO)); vkUnmapMemory(device, cameraBufferMemory);
    VkDescriptorBufferInfo cameraInfo = {cameraBuffer, 0, sizeof(CameraUBO)};
    VkWriteDescriptorSet wds = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    wds.dstSet = descriptorSet; wds.dstBinding = 0; wds.descriptorCount = 1;
    wds.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; wds.pBufferInfo = &cameraInfo;
    vkUpdateDescriptorSets(device, 1, &wds, 0, nullptr);
}

bool VulkanRenderer::render() {
    std::cerr << "render() called" << std::endl;
    std::cerr.flush();
    vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &inFlightFences[currentFrame]);

    uint32_t imageIndex;
    vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphores[currentFrame],
                             VK_NULL_HANDLE, &imageIndex);

    vkResetCommandBuffer(commandBuffers[currentFrame], 0);

    VkCommandBufferBeginInfo cbbi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(commandBuffers[currentFrame], &cbbi);

    // Transition output image to GENERAL for compute write
    VkImageMemoryBarrier imb = {};
    imb.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imb.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imb.image = outputImage; imb.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imb.subresourceRange.levelCount = 1; imb.subresourceRange.layerCount = 1;
    imb.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; imb.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    imb.srcAccessMask = 0; imb.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(commandBuffers[currentFrame], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imb);

    // Dispatch compute (SDF raymarching)
    vkCmdBindPipeline(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
    vkCmdBindDescriptorSets(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_COMPUTE,
                            computePipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    vkCmdDispatch(commandBuffers[currentFrame], (width + 7) / 8, (height + 7) / 8, 1);

    // Transition output for sampling
    imb.oldLayout = VK_IMAGE_LAYOUT_GENERAL; imb.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT; imb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(commandBuffers[currentFrame], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imb);

    std::cerr << "Compute dispatched, transitioning image" << std::endl;

    // Begin render pass
    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    VkRenderPassBeginInfo rpbi = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpbi.renderPass = renderPass; rpbi.framebuffer = swapchainFramebuffers[imageIndex];
    rpbi.renderArea = {{0, 0}, {width, height}};
    rpbi.clearValueCount = 1; rpbi.pClearValues = &clearColor;
    vkCmdBeginRenderPass(commandBuffers[currentFrame], &rpbi, VK_SUBPASS_CONTENTS_INLINE);

    // Bind graphics pipeline and descriptor set
    vkCmdBindPipeline(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
    vkCmdBindDescriptorSets(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineLayout, 0, 1, &graphicsDescriptorSet, 0, nullptr);
    vkCmdDraw(commandBuffers[currentFrame], 3, 1, 0, 0);  // Fullscreen triangle

    vkCmdEndRenderPass(commandBuffers[currentFrame]);

    VK_CHECK(vkEndCommandBuffer(commandBuffers[currentFrame]));

    VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    VkPipelineStageFlags waitStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    si.waitSemaphoreCount = 1; si.pWaitSemaphores = &imageAvailableSemaphores[currentFrame];
    si.pWaitDstStageMask = &waitStages;
    si.commandBufferCount = 1; si.pCommandBuffers = &commandBuffers[currentFrame];
    si.signalSemaphoreCount = 1; si.pSignalSemaphores = &renderFinishedSemaphores[currentFrame];
    VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &si, inFlightFences[currentFrame]));

    VkPresentInfoKHR pi = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    pi.waitSemaphoreCount = 1; pi.pWaitSemaphores = &renderFinishedSemaphores[currentFrame];
    pi.swapchainCount = 1; pi.pSwapchains = &swapchain; pi.pImageIndices = &imageIndex;
    vkQueuePresentKHR(graphicsQueue, &pi);

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    return true;
}

bool VulkanRenderer::beginFrame() { return true; }

void VulkanRenderer::onWindowResized(uint32_t w, uint32_t h) {
    framebufferResized = true;
}

bool VulkanRenderer::endFrame() {
    glfwPollEvents();
    if (framebufferResized) {
        framebufferResized = false;
        vkDeviceWaitIdle(device);
        cleanupSwapchain();
        createSwapchain();
        createFramebuffers();
    }
    return !glfwWindowShouldClose((GLFWwindow*)window);
}

bool VulkanRenderer::shouldClose() const {
    return glfwWindowShouldClose((GLFWwindow*)window);
}

void VulkanRenderer::cleanup() {
    if (!device) return;
    vkDeviceWaitIdle(device);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (imageAvailableSemaphores[i]) vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
        if (renderFinishedSemaphores[i]) vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
        if (inFlightFences[i]) vkDestroyFence(device, inFlightFences[i], nullptr);
    }
    if (commandBuffers) {
        vkFreeCommandBuffers(device, commandPool, MAX_FRAMES_IN_FLIGHT, commandBuffers);
        delete[] commandBuffers;
    }
    for (auto fb : swapchainFramebuffers) if (fb) vkDestroyFramebuffer(device, fb, nullptr);
    if (graphicsPipeline) vkDestroyPipeline(device, graphicsPipeline, nullptr);
    if (pipelineLayout) vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    if (renderPass) vkDestroyRenderPass(device, renderPass, nullptr);
    if (agentBuffer) vkDestroyBuffer(device, agentBuffer, nullptr);
    if (agentBufferMemory) vkFreeMemory(device, agentBufferMemory, nullptr);
    if (svoBuffer) vkDestroyBuffer(device, svoBuffer, nullptr);
    if (svoBufferMemory) vkFreeMemory(device, svoBufferMemory, nullptr);
    if (cameraBuffer) vkDestroyBuffer(device, cameraBuffer, nullptr);
    if (cameraBufferMemory) vkFreeMemory(device, cameraBufferMemory, nullptr);
    if (outputImage) vkDestroyImage(device, outputImage, nullptr);
    if (outputImageMemory) vkFreeMemory(device, outputImageMemory, nullptr);
    if (outputImageView) vkDestroyImageView(device, outputImageView, nullptr);
    if (outputSampler) vkDestroySampler(device, outputSampler, nullptr);
    if (descriptorPool) vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    if (computePipeline) vkDestroyPipeline(device, computePipeline, nullptr);
    if (computePipelineLayout) vkDestroyPipelineLayout(device, computePipelineLayout, nullptr);
    if (descriptorSetLayout) vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
    if (dsLayoutGraphics) vkDestroyDescriptorSetLayout(device, dsLayoutGraphics, nullptr);
    if (commandPool) vkDestroyCommandPool(device, commandPool, nullptr);
    cleanupSwapchain();
    if (device) vkDestroyDevice(device, nullptr);
    if (surface) vkDestroySurfaceKHR(instance, surface, nullptr);
    if (instance) vkDestroyInstance(instance, nullptr);
    if (window) { glfwDestroyWindow((GLFWwindow*)window); glfwTerminate(); }
    delete[] swapchainImages; delete[] swapchainImageViews;
}

void VulkanRenderer::cleanupSwapchain() {
    for (uint32_t i = 0; i < imageCount; i++)
        if (swapchainImageViews[i]) vkDestroyImageView(device, swapchainImageViews[i], nullptr);
    delete[] swapchainImages; swapchainImages = nullptr;
    delete[] swapchainImageViews; swapchainImageViews = nullptr;
    if (swapchain) vkDestroySwapchainKHR(device, swapchain, nullptr);
}
