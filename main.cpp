// Van Nueman Game - Main Entry Point
#include <iostream>
#include <chrono>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstring>

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
#include "simulation/tick_loop.h"
#include "SharedMemoryRingBuffer.h"
#include "FLLRenderer.h"
#include "ArenaAllocator.h"
#include "FLLShaders.h"
#include "audio_system.h"
#include "ThoughtGlyphBridge.h"
#include "cognitive_pipeline.h"
#include "rendering/psv_visualization.h"

// ── Audio System ─────────────────────────────────────────────────
static AudioSystem g_audio = nullptr;

// ── FLL Global State ──────────────────────────────────────────────
static vn::fll::ArenaAllocator g_fllArena;
static vn::fll::FLLRenderer g_fllRenderer;
static vn::fll::FractalNode* g_fllRoot = nullptr;

// ── Glyph Output Stage ───────────────────────────────────────────
// Bridges the cognitive pipeline (ThoughtEngine) to FLL glyph
// manifestation. The glyph evolves as the agent reasons, replacing
// textual LLM output with real-time geometric form.
static vn::fll::ThoughtGlyphBridge g_glyphBridge;
static vn::quantum::ThoughtEngine g_glyphEngine;
static vn::quantum::BCCThoughtEnvironment g_bccEnv;
static int g_glyphRebuildCounter = 0;
static const int GLYPH_REBUILD_INTERVAL = 60; // full tree rebuild every ~1s

// GLFW + Vulkan initialization (no ImGui)
bool initAll(VulkanRenderer& renderer, int width, int height) {
    std::cerr << "initAll() called - no ImGui" << std::endl;
    // Init renderer (creates window + Vulkan)
    if (!renderer.init(width, height, "Van Nueman - Pillar Simulation")) {
        std::cerr << "Failed to init Vulkan renderer" << std::endl;
        return false;
    }

    // ── Init FLL Engine ──────────────────────────────────────────
    std::cerr << "Initializing FLL engine..." << std::endl;
    if (!g_fllArena.init(1024, 64, 65536)) {
        std::cerr << "Failed to init FLL arena" << std::endl;
        return false;
    }
    if (!g_fllRenderer.init(&g_fllArena)) {
        std::cerr << "Failed to init FLL renderer" << std::endl;
        return false;
    }
    // Build initial glyph tree from neutral PSV (will be updated by simulation)
    {
        PillarStateVector defaultPsv;
        defaultPsv.fill(0.5f);
        g_glyphEngine.init_from_psv(defaultPsv);
        float thought[512];
        g_glyphEngine.get_thought(thought);
        g_fllRoot = g_glyphBridge.build_from_thought(thought, g_fllArena, 6);
        if (!g_fllRoot) {
            std::cerr << "Failed to build initial FLL glyph tree" << std::endl;
            return false;
        }
        g_glyphRebuildCounter = 0;
    }
    std::cerr << "FLL engine initialized. Root node: " << g_fllRoot->uid << ". Glyph output stage active." << std::endl;

    // ── Init Audio System ─────────────────────────────────────────
    g_audio = audio_init(1.0f);
    if (g_audio) {
        std::cerr << "Audio system initialized" << std::endl;
    } else {
        std::cerr << "Audio system init failed (non-fatal)" << std::endl;
    }

    return true;
}

int runGameLoop(VulkanRenderer& renderer) {
    std::cerr << "Entering game loop (SDF Skelly test + TRANSFORM GPU compute)" << std::endl;

    // Init simulation tick loop with GPU TRANSFORM compute
    SimulationTickLoop simulation;
    simulation.initialize(512);
    simulation.init_gpu(renderer.getVkDevice(),
                         renderer.getVkPhysicalDevice(),
                         renderer.getVkComputeQueue(),
                         renderer.getVkComputeFamily(),
                         1024);

    // Init native reasoning worker (Hopf-PID ThoughtEngine, no external LLM needed)
    simulation.init_native_reasoning();

    // Spawn a few agents
    for (int i = 0; i < 16; i++) {
        simulation.add_agent(
            (static_cast<float>(std::rand()) / RAND_MAX - 0.5f) * 20.0f,
            (static_cast<float>(std::rand()) / RAND_MAX - 0.5f) * 20.0f,
            (static_cast<float>(std::rand()) / RAND_MAX - 0.5f) * 20.0f
        );
    }
    std::cout << "Spawned " << simulation.get_agent_ecs().size() << " agents" << std::endl;

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

    // Shared memory ring buffer for low-latency IPC to AI framework
    vn::ipc::SharedMemoryRingBuffer shm_ring;
    shm_ring.init();

    auto last_time = std::chrono::high_resolution_clock::now();
    int frame_count = 0;
    float camera_angle = 0.0f;

    while (!renderer.shouldClose()) {
        auto current_time = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(current_time - last_time).count();
        last_time = current_time;

        glfwPollEvents();

        // Step simulation (pillar physics on GPU)
        simulation.tick(dt);

        // Update audio system (cleanup finished sounds)
        audio_update(g_audio, dt);

        // ── Glyph Output Stage ─────────────────────────────────────
        // Drive the FLL fractal glyph from the first active agent's
        // cognitive state. Full tree rebuild at ~1 Hz; root geometry
        // updated every frame for smooth animation.
        {
            auto& ecs = simulation.get_agent_ecs();
            uint32_t n = ecs.size();

            for (uint32_t i = 0; i < n; i++) {
                if (!ecs.active(i)) continue;

                PillarStateVector agentPsv = ecs.get_pillars(i);

                // Blend agent PSV with BCC spatial context
                g_bccEnv.store_at_agent(agentPsv);
                g_bccEnv.scatter(agentPsv, 1);
                PillarStateVector spatial = g_bccEnv.gather(2);
                for (int p = 0; p < NumPillars; p++) {
                    float blended = agentPsv[p] * 0.7f + spatial.pillars[p] * 0.3f;
                    agentPsv[p] = vn::fp20_t(blended);
                }

                // One tick of cognitive processing for glyph evolution
                g_glyphEngine.think(agentPsv, 1, dt, false, &g_bccEnv);

                // Extract the 512D thought vector
                float thought[512];
                g_glyphEngine.get_thought(thought);

                // Periodic full tree rebuild for fractal self-similarity
                g_glyphRebuildCounter++;
                if (g_glyphRebuildCounter >= GLYPH_REBUILD_INTERVAL) {
                    g_fllArena.reset();
                    g_fllRoot = g_glyphBridge.build_from_thought(thought, g_fllArena, 6);
                    g_glyphRebuildCounter = 0;
                    if (g_fllRoot) {
                        g_fllRenderer.set_active_depth(0);
                    }
                } else if (g_fllRoot) {
                    // Per-frame root geometry update for smooth animation
                    g_glyphBridge.update_root_from_thought(g_fllRoot, thought);
                }
                break; // use first active agent for glyph
            }
        }

        // Upload agent positions to renderer
        {
            auto& ecs = simulation.get_agent_ecs();
            uint32_t n = static_cast<uint32_t>(ecs.size());
            std::vector<float> pos(n * 3);
            std::vector<float> col(n * 3);
            std::vector<float> sizes(n);
            std::vector<float> alphas(n);
            uint32_t active_count = 0;
            for (uint32_t i = 0; i < n; i++) {
                if (!ecs.active(i)) continue;
                pos[active_count * 3 + 0] = ecs.x(i);
                pos[active_count * 3 + 1] = ecs.y(i);
                pos[active_count * 3 + 2] = ecs.z(i);
                auto psv = ecs.get_pillars(i);
                vn::float3 psv_color;
                float psv_size, psv_alpha;
                psv_to_vis(psv, psv_color, psv_size, psv_alpha);
                col[active_count * 3 + 0] = psv_color.x;
                col[active_count * 3 + 1] = psv_color.y;
                col[active_count * 3 + 2] = psv_color.z;
                sizes[active_count] = psv_size;
                alphas[active_count] = psv_alpha;
                active_count++;
            }
            if (active_count > 0) {
                renderer.uploadAgents(pos.data(), col.data(), active_count, sizes.data(), alphas.data());
            }
        }

        // Export PSV state to shared memory ring buffer for AI bridge (every 30 frames)
        if (frame_count % 30 == 0 && shm_ring.is_initialized()) {
            auto& ecs = simulation.get_agent_ecs();
            uint32_t n = static_cast<uint32_t>(ecs.size());
            uint64_t tick = simulation.get_world_state().tick_count;

            shm_ring.write_snapshot(tick, n, [&](uint32_t i, vn::ipc::AgentSnapshot& asnap) -> bool {
                if (!ecs.active(i)) return false;
                auto p = ecs.get_pillars(i);
                asnap.agent_id = i;
                asnap.pos_x = ecs.x(i);
                asnap.pos_y = ecs.y(i);
                asnap.pos_z = ecs.z(i);
                for (int pi = 0; pi < 16; pi++)
                    asnap.pillars[pi] = p[pi];
                return true;
            });

            // Log telemetry every 300 frames (every 10th SHM write)
            if (frame_count % 300 == 0) {
                const auto* hdr = shm_ring.get_header();
                if (hdr) {
                    auto& tm = hdr->telemetry;
                    std::cout << "[Telemetry] SHM writes: " << tm.c_write_count
                              << ", last: " << tm.c_write_last_ns << "ns"
                              << ", max: " << tm.c_write_max_ns << "ns" << std::endl;
                }
            }
        }

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

        // ── FLL Per-frame Update ──────────────────────────────────
        // Derive zoom from camera distance
        float orbit_dist = std::sqrt(cam_x * cam_x + cam_z * cam_z);
        float fll_zoom = std::max(1.0f, orbit_dist * 0.1f);
        g_fllRenderer.set_zoom_level(fll_zoom);
        g_fllRenderer.update(dt, 0.0f, 0.0f);

        // Pack FLL render list into FractalNodeGPU buffer and upload
        uint32_t rcount = g_fllRenderer.render_list_count();
        if (rcount > 0) {
            std::vector<vn::fll::FractalNodeGPU> gpuNodes;
            gpuNodes.reserve(rcount);
            for (uint32_t i = 0; i < rcount; i++) {
                vn::fll::FractalNode* node = g_fllRenderer.render_list()[i];
                vn::fll::FractalNodeGPU gpu{};
                gpu.uid = node->uid;
                gpu.depth = node->depth;
                gpu.child_count = node->child_count;
                std::memcpy(&gpu.geo, &node->geometry, sizeof(vn::fll::GeometricCoefficients));
                gpuNodes.push_back(gpu);
            }
            renderer.uploadFLLNodes(gpuNodes.data(), rcount, sizeof(vn::fll::FractalNodeGPU));
        }

        // Push FLL constants into renderer for dispatch during render()
        renderer.setFLLPushConstants(g_fllRenderer.push_constants());

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

    audio_shutdown(g_audio);
    g_audio = nullptr;
    skelly_api_cleanup(skelly);
    shm_ring.shutdown();
    std::cerr << "Exiting game loop (ran " << simulation.get_world_state().tick_count << " ticks)" << std::endl;
    return 0;
}

#endif

int main() {
#ifndef NDEBUG
    std::cerr << "DEBUG: Entering main()" << std::endl;
#endif

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
