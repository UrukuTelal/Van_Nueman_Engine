// Van Nueman Minimal Vulkan Renderer - Just clears screen to blue
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <cstring>

#define VK_CHECK(x) do { VkResult _r = x; if (_r) { std::cerr << "VK error " << _r << " at line " << __LINE__ << std::endl; return false; } } while(0)

VkInstance instance = VK_NULL_HANDLE;
VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
VkDevice device = VK_NULL_HANDLE;
VkQueue graphicsQueue = VK_NULL_HANDLE;
VkSurfaceKHR surface = VK_NULL_HANDLE;
VkSwapchainKHR swapchain = VK_NULL_HANDLE;
VkRenderPass renderPass = VK_NULL_HANDLE;
VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
VkPipeline graphicsPipeline = VK_NULL_HANDLE;
VkCommandPool commandPool = VK_NULL_HANDLE;
VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
VkSemaphore imageAvailable = VK_NULL_HANDLE;
VkSemaphore renderFinished = VK_NULL_HANDLE;
VkFence inFlightFence = VK_NULL_HANDLE;

std::vector<VkImage> swapchainImages;
std::vector<VkImageView> swapchainImageViews;
std::vector<VkFramebuffer> swapchainFramebuffers;

uint32_t width = 1920, height = 1080;
GLFWwindow* window = nullptr;
int graphicsFamily = -1, presentFamily = -1;

bool initVulkan() {
    // Instance
    VkApplicationInfo ai = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    ai.pApplicationName = "VanNueman"; ai.apiVersion = VK_API_VERSION_1_2;
    uint32_t extCount = 0;
    const char** exts = glfwGetRequiredInstanceExtensions(&extCount);
    VkInstanceCreateInfo ii = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ii.pApplicationInfo = &ai; ii.enabledExtensionCount = extCount; ii.ppEnabledExtensionNames = exts;
    VK_CHECK(vkCreateInstance(&ii, nullptr, &instance));
    VK_CHECK(glfwCreateWindowSurface(instance, window, nullptr, &surface));

    // Physical device
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
    physicalDevice = devices[0];

    // Queue families
    uint32_t qfCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &qfCount, nullptr);
    std::vector<VkQueueFamilyProperties> qfamilies(qfCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &qfCount, qfamilies.data());
    for (int i = 0; i < (int)qfCount; i++) {
        if (qfamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) graphicsFamily = i;
        VkBool32 present = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &present);
        if (present) presentFamily = i;
    }

    // Device
    float priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> qcis;
    auto addQ = [&](int f) {
        for (auto& q : qcis) if ((int)q.queueFamilyIndex == f) return;
        VkDeviceQueueCreateInfo q = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        q.queueFamilyIndex = f; q.queueCount = 1; q.pQueuePriorities = &priority;
        qcis.push_back(q);
    };
    addQ(graphicsFamily); addQ(presentFamily);
    const char* ext = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    VkDeviceCreateInfo di = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    di.queueCreateInfoCount = qcis.size(); di.pQueueCreateInfos = qcis.data();
    di.enabledExtensionCount = 1; di.ppEnabledExtensionNames = &ext;
    VK_CHECK(vkCreateDevice(physicalDevice, &di, nullptr, &device));
    vkGetDeviceQueue(device, graphicsFamily, 0, &graphicsQueue);

    // Command pool
    VkCommandPoolCreateInfo cpci = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cpci.queueFamilyIndex = graphicsFamily;
    VK_CHECK(vkCreateCommandPool(device, &cpci, nullptr, &commandPool));

    return true;
}

bool createSwapchain() {
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &caps);
    VkSwapchainCreateInfoKHR sci = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    sci.surface = surface; sci.minImageCount = caps.minImageCount;
    sci.imageFormat = VK_FORMAT_B8G8R8A8_UNORM; sci.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    sci.imageExtent = {width, height}; sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    sci.preTransform = caps.currentTransform; sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode = VK_PRESENT_MODE_FIFO_KHR; sci.clipped = VK_TRUE;
    VK_CHECK(vkCreateSwapchainKHR(device, &sci, nullptr, &swapchain));

    uint32_t count = 0;
    vkGetSwapchainImagesKHR(device, swapchain, &count, nullptr);
    swapchainImages.resize(count);
    vkGetSwapchainImagesKHR(device, swapchain, &count, swapchainImages.data());

    swapchainImageViews.resize(count);
    for (uint32_t i = 0; i < count; i++) {
        VkImageViewCreateInfo ivci = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        ivci.image = swapchainImages[i]; ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ivci.format = VK_FORMAT_B8G8R8A8_UNORM; ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ivci.subresourceRange.levelCount = 1; ivci.subresourceRange.layerCount = 1;
        VK_CHECK(vkCreateImageView(device, &ivci, nullptr, &swapchainImageViews[i]));
    }
    return true;
}

bool createRenderPass() {
    VkAttachmentDescription color = {};
    color.format = VK_FORMAT_B8G8R8A8_UNORM; color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkAttachmentReference ref = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass = {}; subpass.colorAttachmentCount = 1; subpass.pColorAttachments = &ref;
    VkRenderPassCreateInfo rpci = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rpci.attachmentCount = 1; rpci.pAttachments = &color; rpci.subpassCount = 1; rpci.pSubpasses = &subpass;
    VK_CHECK(vkCreateRenderPass(device, &rpci, nullptr, &renderPass));
    return true;
}

bool createGraphicsPipeline() {
    // Simple triangle shader - just to test rendering
    // Vertex shader
    const char* vShaderCode = R"(
        #version 450
        void main() { gl_Position = vec4(0.0, 0.0, 0.0, 1.0); }
    )";
    // Fragment shader - outputs blue
    const char* fShaderCode = R"(
        #version 450
        layout(location = 0) out vec4 outColor;
        void main() { outColor = vec4(0.1, 0.2, 0.3, 1.0); }
    )";

    // For now, just create a dummy pipeline layout
    VkPipelineLayoutCreateInfo plci = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    VK_CHECK(vkCreatePipelineLayout(device, &plci, nullptr, &pipelineLayout));

    // Skip actual graphics pipeline for now - we'll just clear
    return true;
}

bool createFramebuffers() {
    swapchainFramebuffers.resize(swapchainImageViews.size());
    for (size_t i = 0; i < swapchainImageViews.size(); i++) {
        VkFramebufferCreateInfo fbci = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fbci.renderPass = renderPass; fbci.attachmentCount = 1; fbci.pAttachments = &swapchainImageViews[i];
        fbci.width = width; fbci.height = height; fbci.layers = 1;
        VK_CHECK(vkCreateFramebuffer(device, &fbci, nullptr, &swapchainFramebuffers[i]));
    }
    return true;
}

bool createCommandBuffer() {
    VkCommandBufferAllocateInfo cbai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cbai.commandPool = commandPool; cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cbai.commandBufferCount = 1;
    VK_CHECK(vkAllocateCommandBuffers(device, &cbai, &commandBuffer));
    return true;
}

bool createSyncObjects() {
    VkSemaphoreCreateInfo sci = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fci = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO}; fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    VK_CHECK(vkCreateSemaphore(device, &sci, nullptr, &imageAvailable));
    VK_CHECK(vkCreateSemaphore(device, &sci, nullptr, &renderFinished));
    VK_CHECK(vkCreateFence(device, &fci, nullptr, &inFlightFence));
    return true;
}

bool render() {
    vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &inFlightFence);

    uint32_t imageIndex;
    vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailable, VK_NULL_HANDLE, &imageIndex);

    vkResetCommandBuffer(commandBuffer, 0);
    VkCommandBufferBeginInfo cbbi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(commandBuffer, &cbbi);

    VkRenderPassBeginInfo rpbi = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpbi.renderPass = renderPass; rpbi.framebuffer = swapchainFramebuffers[imageIndex];
    rpbi.renderArea = {{0, 0}, {width, height}};
    VkClearValue clear = {{{0.1f, 0.2f, 0.3f, 1.0f}}};
    rpbi.clearValueCount = 1; rpbi.pClearValues = &clear;
    vkCmdBeginRenderPass(commandBuffer, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdEndRenderPass(commandBuffer);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    VkPipelineStageFlags waitStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    si.waitSemaphoreCount = 1; si.pWaitSemaphores = &imageAvailable;
    si.pWaitDstStageMask = &waitStages;
    si.commandBufferCount = 1; si.pCommandBuffers = &commandBuffer;
    si.signalSemaphoreCount = 1; si.pSignalSemaphores = &renderFinished;
    VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &si, inFlightFence));

    VkPresentInfoKHR pi = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    pi.waitSemaphoreCount = 1; pi.pWaitSemaphores = &renderFinished;
    pi.swapchainCount = 1; pi.pSwapchains = &swapchain; pi.pImageIndices = &imageIndex;
    vkQueuePresentKHR(graphicsQueue, &pi);

    return true;
}

void cleanup() {
    if (!device) return;
    vkDeviceWaitIdle(device);
    if (inFlightFence) vkDestroyFence(device, inFlightFence, nullptr);
    if (renderFinished) vkDestroySemaphore(device, renderFinished, nullptr);
    if (imageAvailable) vkDestroySemaphore(device, imageAvailable, nullptr);
    if (commandBuffer) vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    for (auto fb : swapchainFramebuffers) if (fb) vkDestroyFramebuffer(device, fb, nullptr);
    if (pipelineLayout) vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    if (renderPass) vkDestroyRenderPass(device, renderPass, nullptr);
    for (auto iv : swapchainImageViews) if (iv) vkDestroyImageView(device, iv, nullptr);
    if (swapchain) vkDestroySwapchainKHR(device, swapchain, nullptr);
    if (commandPool) vkDestroyCommandPool(device, commandPool, nullptr);
    if (device) vkDestroyDevice(device, nullptr);
    if (surface) vkDestroySurfaceKHR(instance, surface, nullptr);
    if (instance) vkDestroyInstance(instance, nullptr);
}

int main() {
    if (!glfwInit()) { std::cerr << "GLFW init failed" << std::endl; return -1; }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window = glfwCreateWindow(width, height, "Van Nueman - Test", nullptr, nullptr);
    if (!window) { std::cerr << "Window creation failed" << std::endl; return -1; }

    if (!initVulkan()) { std::cerr << "Vulkan init failed" << std::endl; return -1; }
    if (!createSwapchain()) return -1;
    if (!createRenderPass()) return -1;
    if (!createGraphicsPipeline()) return -1;
    if (!createFramebuffers()) return -1;
    if (!createCommandBuffer()) return -1;
    if (!createSyncObjects()) return -1;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (!render()) break;
    }

    cleanup();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
