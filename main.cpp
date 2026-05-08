// Van Nueman Game - Main Entry Point
#include <iostream>
#include <chrono>

#ifdef HAVE_GLFW
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include "renderer/vulkan_renderer.h"
#include "gui/splash_screen.h"
#include "gui/main_menu.h"
#include "gui/loading_screen.h"
#include "gui/settings.h"
#include "include/SkellyGPU.h"
#include "api/skelly_api.h"
#include "../include/Entity.h"

// GLFW + Vulkan initialization (no ImGui)
bool initAll(VulkanRenderer& renderer, int width, int height) {
    std::cerr << "initAll() called - no ImGui" << std::endl;
    // Init renderer (creates window + Vulkan)
    if (!renderer.init(width, height, "Van Nueman - Pillar Simulation")) {
        std::cerr << "Failed to init Vulkan renderer" << std::endl;
        return false;
    }

    return true;
}

int runGameLoop(VulkanRenderer& renderer) {
    std::cerr << "Entering game loop (SDF Skelly test)" << std::endl;

    // Init Skelly API
    SkellyAPIContext skelly = skelly_api_init();
    int32_t creature = skelly_api_create_creature(skelly, -1, 0, 0, 0, "TestCreature");

    // Create test Skelly data (simple creature: 2 bones, 1 muscle, 1 organ)
    BoneGPU bones[2] = {};
    bones[0].start_x = 0.0f; bones[0].start_y = 0.0f; bones[0].start_z = 0.0f;
    bones[0].end_x = 0.0f; bones[0].end_y = 0.0f; bones[0].end_z = 10.0f;
    bones[0].radius = 0.5f;

    bones[1].start_x = 0.0f; bones[1].start_y = 0.0f; bones[1].start_z = 10.0f;
    bones[1].end_x = 0.0f; bones[1].end_y = 5.0f; bones[1].end_z = 15.0f;
    bones[1].radius = 0.3f;

    MuscleGPU muscles[1] = {};
    muscles[0].start_x = 0.0f; muscles[0].start_y = 0.0f; muscles[0].start_z = 2.0f;
    muscles[0].end_x = 0.0f; muscles[0].end_y = 0.0f; muscles[0].end_z = 8.0f;
    muscles[0].radius = 0.2f;
    muscles[0].activation = 0.5f;

    OrganGPU organs[1] = {};
    organs[0].center_x = 0.0f; organs[0].center_y = 0.0f; organs[0].center_z = 5.0f;
    organs[0].radius = 1.5f;
    organs[0].type = 2;  // POWER_PLANT (green)

    // Upload to GPU
    renderer.uploadBones(bones, 2);
    renderer.uploadMuscles(muscles, 1);
    renderer.uploadOrgans(organs, 1);

    auto last_time = std::chrono::high_resolution_clock::now();
    int frame_count = 0;
    float camera_angle = 0.0f;

    while (!renderer.shouldClose()) {
        auto current_time = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(current_time - last_time).count();
        last_time = current_time;

        glfwPollEvents();

        // Step Skelly simulation
        skelly_api_step(skelly, dt);

        // Update camera (rotate around creature)
        camera_angle += dt * 0.5f;
        float cam_x = 20.0f * sinf(camera_angle);
        float cam_z = 20.0f * cosf(camera_angle);
        CameraUBO cam = {};
        cam.position[0] = cam_x; cam.position[1] = 15.0f; cam.position[2] = cam_z;
        cam.forward[0] = -cam_x; cam.forward[1] = -15.0f; cam.forward[2] = -cam_z;
        cam.up[0] = 0.0f; cam.up[1] = 1.0f; cam.up[2] = 0.0f;
        cam.right[0] = cosf(camera_angle); cam.right[1] = 0.0f; cam.right[2] = -sinf(camera_angle);
        cam.fov = 60.0f;
        uint32_t w = renderer.getWidth();
        uint32_t h = renderer.getHeight();
        cam.aspect = (float)w / (float)h;
        cam.width = w; cam.height = h;
        renderer.updateCamera(cam);

        // Vulkan render
        renderer.beginFrame();
        std::cerr << "About to call render()" << std::endl;
        renderer.render();
        std::cerr << "render() returned" << std::endl;
        renderer.endFrame();

        frame_count++;
        if (frame_count % 1000 == 0) {
            std::cerr << "Frame " << frame_count << std::endl;
        }
    }

    skelly_api_cleanup(skelly);
    std::cerr << "Exiting game loop" << std::endl;
    return 0;
}

#endif

int main() {
    std::cerr << "DEBUG: Entering main()" << std::endl;

#ifdef HAVE_GLFW
    VulkanRenderer renderer;
    renderer.setDebug(false);

    if (!initAll(renderer, 1920, 1080)) {
        std::cerr << "Failed to initialize" << std::endl;
        return -1;
    }

    int result = runGameLoop(renderer);

    // Cleanup
    vkDeviceWaitIdle(renderer.getVkDevice());
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();

    return result;
#else
    std::cerr << "Van Nueman Engine - Headless Mode (no GLFW)" << std::endl;
    std::cerr << "Running in headless mode. Press Enter to exit." << std::endl;
    std::cin.get();
    return 0;
#endif
}
