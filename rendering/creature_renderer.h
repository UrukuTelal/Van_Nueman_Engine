// creature_renderer.h - Creature rendering integration with VulkanRenderer
// Adapted from Sov_Eng/renderer/integrated_creature.py patterns

#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <string>
#include "../include/SkellyTypes.h"
#include "../include/SkellyGPU.h"
#include "psv_visualization.h"

#ifndef VN_NO_VULKAN
#include "../renderer/vulkan_renderer.h"
#endif

namespace Van_Nueman {

struct CreatureRenderData {
    uint32_t entity_id;
    float position[3];
    float rotation[4];
    float scale;
    float color[4];
    float glow;
    uint32_t is_dreaming;
    uint32_t is_in_shadow;
    float health;
    float muscle_activation;
};

struct BoneRenderData {
    uint32_t start_node;
    uint32_t end_node;
    float start_pos[3];
    float end_pos[3];
    float radius;
    float color[4];
    uint32_t is_fractured;
};

struct MuscleRenderData {
    uint32_t origin_bone;
    uint32_t insertion_bone;
    float activation;
    float color[4];
    float thickness;
    uint32_t strand_count;
};

struct OrganRenderData {
    float position[3];
    float radius;
    float color[4];
    float pulse_intensity;
    uint32_t organ_type;
};

class CreatureRenderer {
public:
    CreatureRenderer();
    ~CreatureRenderer();
    
    bool initialize();
    void shutdown();
    
    void set_model_directory(const std::string& dir);
    const std::string& get_model_directory() const { return model_dir_; }
    
    bool load_skeleton_model(const std::string& model_path, 
                             std::vector<BoneRenderData>& out_bones);
    bool load_plant_segments(const std::string& model_path,
                            std::vector<BoneRenderData>& out_bones);
    bool load_eye_model(const std::string& model_path,
                       std::vector<OrganRenderData>& out_eyes);
    
    void prepare_creature_render_data(const class Creature* creature,
                                       CreatureRenderData& out_data);
    void prepare_skeleton_render_data(const class Skeleton* skeleton,
                                       std::vector<BoneRenderData>& out_bones,
                                       std::vector<MuscleRenderData>& out_muscles,
                                       std::vector<OrganRenderData>& out_organs);
    
    void apply_psv_visualization(CreatureRenderData& data, 
                                  const EntityPSV& psv);
    
#ifndef VN_NO_VULKAN
    bool upload_to_gpu(VulkanRenderer* renderer);
    bool upload_bones_to_gpu(VulkanRenderer* renderer, 
                             const std::vector<BoneRenderData>& bones);
    bool upload_muscles_to_gpu(VulkanRenderer* renderer,
                               const std::vector<MuscleRenderData>& muscles);
    bool upload_organs_to_gpu(VulkanRenderer* renderer,
                              const std::vector<OrganRenderData>& organs);
#endif

    static constexpr size_t MAX_BONES = 256;
    static constexpr size_t MAX_MUSCLES = 128;
    static constexpr size_t MAX_ORGANS = 64;

private:
    bool load_glb_model(const std::string& path, 
                        std::vector<float>& out_vertices,
                        std::vector<uint32_t>& out_indices);
    
    void parse_bone_geometry(const std::vector<float>& vertices,
                             const std::vector<uint32_t>& indices,
                             std::vector<BoneRenderData>& out_bones);
    
    std::string model_dir_;
    std::string sov_eng_models_dir_;
    
    std::vector<BoneRenderData> cached_bones_;
    std::vector<MuscleRenderData> cached_muscles_;
    std::vector<OrganRenderData> cached_organs_;
    
    bool initialized_ = false;
};

class CreatureVisualizer {
public:
    CreatureVisualizer();
    ~CreatureVisualizer();
    
    bool initialize(CreatureRenderer* renderer);
    void shutdown();
    
    void set_visible(bool visible) { visible_ = visible; }
    bool is_visible() const { return visible_; }
    
    void set_render_mode(uint32_t mode);
    uint32_t get_render_mode() const { return render_mode_; }
    
    void render_creature(const Creature* creature);
    void render_skeleton(const Skeleton* skeleton, bool show_muscles = true);
    void render_psv_overlay(const Creature* creature, const EntityPSV& psv);
    
    enum RenderMode : uint32_t {
        MODE_SOLID = 0,
        MODE_WIREFRAME = 1,
        MODE_TRANSPARENT = 2,
        MODE_DREAM_STATE = 3,
        MODE_SHADOW_STATE = 4
    };

private:
    CreatureRenderer* renderer_ = nullptr;
    bool visible_ = true;
    uint32_t render_mode_ = MODE_SOLID;
    
    float dream_glow_color_[4] = {0.5f, 0.2f, 1.0f, 0.8f};
    float shadow_color_[4] = {0.1f, 0.1f, 0.2f, 0.6f};
};

class SkeletonMeshLoader {
public:
    SkeletonMeshLoader();
    ~SkeletonMeshLoader();
    
    bool load_skull_mesh(const std::string& path);
    bool load_jaw_mesh(const std::string& path);
    bool load_eye_mesh(const std::string& path);
    bool load_mechanical_model(const std::string& path);
    bool load_plant_segments(const std::string& path);
    
    struct LoadedMesh {
        std::vector<float> vertices;
        std::vector<float> normals;
        std::vector<uint32_t> indices;
        uint32_t vertex_count;
        uint32_t index_count;
    };
    
    const LoadedMesh* get_mesh(const std::string& name) const;
    std::vector<std::string> get_loaded_mesh_names() const;
    
    void clear_cache();

private:
    std::vector<float> parse_glb_vertices(const std::string& path);
    std::vector<uint32_t> parse_glb_indices(const std::string& path);
    
    std::vector<std::pair<std::string, LoadedMesh>> loaded_meshes_;
};

CreatureRenderer* create_creature_renderer();
void destroy_creature_renderer(CreatureRenderer* renderer);

}