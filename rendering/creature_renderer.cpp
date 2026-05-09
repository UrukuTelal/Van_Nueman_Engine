// creature_renderer.cpp - Creature rendering implementation

#include "creature_renderer.h"
#include "../biology/creature_system.h"
#include <algorithm>
#include <cstring>
#include <fstream>

namespace Van_Nueman {

CreatureRenderer::CreatureRenderer() : initialized_(false) {
}

CreatureRenderer::~CreatureRenderer() {
    shutdown();
}

bool CreatureRenderer::initialize() {
    if (initialized_) return true;
    
    model_dir_ = "models/";
    sov_eng_models_dir_ = "../Sov_Eng/models/";
    
    cached_bones_.reserve(MAX_BONES);
    cached_muscles_.reserve(MAX_MUSCLES);
    cached_organs_.reserve(MAX_ORGANS);
    
    initialized_ = true;
    return true;
}

void CreatureRenderer::shutdown() {
    cached_bones_.clear();
    cached_muscles_.clear();
    cached_organs_.clear();
    initialized_ = false;
}

void CreatureRenderer::set_model_directory(const std::string& dir) {
    model_dir_ = dir;
    if (!model_dir_.empty() && model_dir_.back() != '/' && model_dir_.back() != '\\') {
        model_dir_ += "/";
    }
}

bool CreatureRenderer::load_skeleton_model(const std::string& model_path,
                                            std::vector<BoneRenderData>& out_bones) {
    std::vector<float> vertices;
    std::vector<uint32_t> indices;
    
    if (!load_glb_model(model_path, vertices, indices)) {
        return false;
    }
    
    parse_bone_geometry(vertices, indices, out_bones);
    return !out_bones.empty();
}

bool CreatureRenderer::load_plant_segments(const std::string& model_path,
                                            std::vector<BoneRenderData>& out_bones) {
    return load_skeleton_model(model_path, out_bones);
}

bool CreatureRenderer::load_eye_model(const std::string& model_path,
                                       std::vector<OrganRenderData>& out_eyes) {
    std::vector<float> vertices;
    std::vector<uint32_t> indices;
    
    if (!load_glb_model(model_path, vertices, indices)) {
        OrganRenderData default_eye = {};
        default_eye.position[0] = 0.0f; default_eye.position[1] = 0.0f; default_eye.position[2] = 0.0f;
        default_eye.radius = 0.5f;
        default_eye.color[0] = 0.2f; default_eye.color[1] = 0.8f; default_eye.color[2] = 0.3f; default_eye.color[3] = 1.0f;
        default_eye.organ_type = 0;
        out_eyes.push_back(default_eye);
        return true;
    }
    
    float min_x = vertices[0], max_x = vertices[0];
    float min_y = vertices[1], max_y = vertices[1];
    float min_z = vertices[2], max_z = vertices[2];
    
    for (size_t i = 0; i < vertices.size(); i += 3) {
        min_x = std::fmin(min_x, vertices[i]);
        max_x = std::fmax(max_x, vertices[i]);
        min_y = std::fmin(min_y, vertices[i + 1]);
        max_y = std::fmax(max_y, vertices[i + 1]);
        min_z = std::fmin(min_z, vertices[i + 2]);
        max_z = std::fmax(max_z, vertices[i + 2]);
    }
    
    OrganRenderData eye = {};
    eye.position[0] = (min_x + max_x) * 0.5f;
    eye.position[1] = (min_y + max_y) * 0.5f;
    eye.position[2] = (min_z + max_z) * 0.5f;
    eye.radius = std::fmax(std::fmax(max_x - min_x, max_y - min_y), max_z - min_z) * 0.5f;
    eye.color[0] = 0.3f; eye.color[1] = 0.9f; eye.color[2] = 0.4f; eye.color[3] = 1.0f;
    eye.organ_type = 0;
    
    out_eyes.push_back(eye);
    return true;
}

void CreatureRenderer::prepare_creature_render_data(const Creature* creature,
                                                     CreatureRenderData& out_data) {
    if (!creature) return;
    
    out_data.entity_id = creature->get_entity_id();
    out_data.position[0] = 0.0f;
    out_data.position[1] = 0.0f;
    out_data.position[2] = 0.0f;
    
    out_data.rotation[0] = 0.0f; out_data.rotation[1] = 0.0f;
    out_data.rotation[2] = 0.0f; out_data.rotation[3] = 1.0f;
    
    out_data.scale = creature->get_scale();
    
    auto state = creature->get_state();
    out_data.color[0] = 0.7f; out_data.color[1] = 0.6f; out_data.color[2] = 0.5f;
    out_data.color[3] = 1.0f;
    
    out_data.glow = state.is_dreaming ? 0.5f : 0.0f;
    out_data.is_dreaming = state.is_dreaming ? 1 : 0;
    out_data.is_in_shadow = state.in_shadow ? 1 : 0;
    out_data.health = state.overall_health;
    out_data.muscle_activation = state.muscle_activation;
}

void CreatureRenderer::prepare_skeleton_render_data(const Skeleton* skeleton,
                                                     std::vector<BoneRenderData>& out_bones,
                                                     std::vector<MuscleRenderData>& out_muscles,
                                                     std::vector<OrganRenderData>& out_organs) {
    if (!skeleton) return;
    
    out_bones.clear();
    out_muscles.clear();
    out_organs.clear();
    
    for (size_t i = 0; i < skeleton->bones.size(); i++) {
        const auto& bone = skeleton->bones[i];
        if (!bone) continue;
        
        BoneRenderData bone_data = {};
        bone_data.start_node = i;
        bone_data.end_node = i + 1;
        
        Vec3 start_world = bone->get_start();
        Vec3 end_world = bone->get_end();
        
        bone_data.start_pos[0] = start_world.x;
        bone_data.start_pos[1] = start_world.y;
        bone_data.start_pos[2] = start_world.z;
        bone_data.end_pos[0] = end_world.x;
        bone_data.end_pos[1] = end_world.y;
        bone_data.end_pos[2] = end_world.z;
        
        bone_data.radius = 0.5f;
        bone_data.color[0] = 0.9f; bone_data.color[1] = 0.85f; bone_data.color[2] = 0.8f;
        bone_data.color[3] = 1.0f;
        bone_data.is_fractured = bone->is_fractured() ? 1 : 0;
        
        out_bones.push_back(bone_data);
    }
    
    for (const auto& muscle : skeleton->muscles) {
        if (!muscle) continue;
        
        MuscleRenderData muscle_data = {};
        muscle_data.origin_bone = 0;
        muscle_data.insertion_bone = 1;
        muscle_data.activation = muscle->get_activation();
        muscle_data.thickness = 0.2f * (1.0f + muscle->get_activation());
        muscle_data.strand_count = static_cast<uint32_t>(muscle->strands.size());
        
        float r = 0.8f + muscle->get_activation() * 0.2f;
        float g = 0.3f + muscle->get_activation() * 0.4f;
        float b = 0.2f;
        muscle_data.color[0] = r; muscle_data.color[1] = g; muscle_data.color[2] = b;
        muscle_data.color[3] = 0.9f;
        
        out_muscles.push_back(muscle_data);
    }
    
    for (const auto& organ : skeleton->organs) {
        if (!organ) continue;
        
        OrganRenderData organ_data = {};
        organ_data.position[0] = 0.0f;
        organ_data.position[1] = 0.0f;
        organ_data.position[2] = 0.0f;
        organ_data.radius = organ->get_volume() * 0.1f;
        organ_data.pulse_intensity = organ->active_state;
        organ_data.organ_type = static_cast<uint32_t>(organ->type);
        
        switch (organ->type) {
            case ORGAN_PUMP:
                organ_data.color[0] = 0.9f; organ_data.color[1] = 0.2f; organ_data.color[2] = 0.2f;
                break;
            case ORGAN_POWER_PLANT:
                organ_data.color[0] = 1.0f; organ_data.color[1] = 0.8f; organ_data.color[2] = 0.2f;
                break;
            case ORGAN_FACTORY:
                organ_data.color[0] = 0.3f; organ_data.color[1] = 0.8f; organ_data.color[2] = 0.3f;
                break;
            case ORGAN_VALVE:
                organ_data.color[0] = 0.2f; organ_data.color[1] = 0.5f; organ_data.color[2] = 0.9f;
                break;
            default:
                organ_data.color[0] = 0.5f; organ_data.color[1] = 0.5f; organ_data.color[2] = 0.5f;
                break;
        }
        organ_data.color[3] = 1.0f;
        
        out_organs.push_back(organ_data);
    }
}

void CreatureRenderer::apply_psv_visualization(CreatureRenderData& data,
                                                 const EntityPSV& psv) {
    float awareness = (psv.pillars[0] + psv.pillars[1] + psv.pillars[2]) / 3.0f;
    float harm = (psv.pillars[12] + psv.pillars[6] + psv.pillars[7]) / 3.0f;
    float depth = (psv.pillars[15] + psv.pillars[14] + psv.pillars[13]) / 3.0f;
    
    data.color[0] = std::fmax(0.0f, std::fmin(1.0f, awareness));
    data.color[1] = std::fmax(0.0f, std::fmin(1.0f, harm));
    data.color[2] = std::fmax(0.0f, std::fmin(1.0f, depth));
    
    data.glow = psv.lucid_level * 0.5f + psv.shadow_emergence * 0.3f;
    data.is_dreaming = (psv.lucid_level > 0.1f) ? 1 : 0;
    data.is_in_shadow = (psv.shadow_emergence > 0.3f) ? 1 : 0;
    
    data.scale *= (1.0f + depth * 0.5f);
}

#ifndef VN_NO_VULKAN
bool CreatureRenderer::upload_to_gpu(VulkanRenderer* renderer) {
    if (!renderer || !initialized_) return false;
    
    std::vector<BoneGPU> bones_gpu;
    bones_gpu.reserve(cached_bones_.size());
    
    for (const auto& bone : cached_bones_) {
        BoneGPU b = {};
        b.start_x = bone.start_pos[0]; b.start_y = bone.start_pos[1]; b.start_z = bone.start_pos[2];
        b.end_x = bone.end_pos[0]; b.end_y = bone.end_pos[1]; b.end_z = bone.end_pos[2];
        b.radius = bone.radius;
        b.r = bone.color[0]; b.g = bone.color[1]; b.b = bone.color[2]; b.a = bone.color[3];
        bones_gpu.push_back(b);
    }
    
    if (!bones_gpu.empty()) {
        renderer->uploadBones(bones_gpu.data(), static_cast<uint32_t>(bones_gpu.size()));
    }
    
    return true;
}

bool CreatureRenderer::upload_bones_to_gpu(VulkanRenderer* renderer,
                                            const std::vector<BoneRenderData>& bones) {
    if (!renderer) return false;
    
    std::vector<BoneGPU> bones_gpu;
    bones_gpu.reserve(bones.size());
    
    for (const auto& bone : bones) {
        BoneGPU b = {};
        b.start_x = bone.start_pos[0]; b.start_y = bone.start_pos[1]; b.start_z = bone.start_pos[2];
        b.end_x = bone.end_pos[0]; b.end_y = bone.end_pos[1]; b.end_z = bone.end_pos[2];
        b.radius = bone.radius;
        b.r = bone.color[0]; b.g = bone.color[1]; b.b = bone.color[2]; b.a = bone.color[3];
        bones_gpu.push_back(b);
    }
    
    return renderer->uploadBones(bones_gpu.data(), static_cast<uint32_t>(bones_gpu.size()));
}

bool CreatureRenderer::upload_muscles_to_gpu(VulkanRenderer* renderer,
                                              const std::vector<MuscleRenderData>& muscles) {
    if (!renderer) return false;
    
    std::vector<MuscleGPU> muscles_gpu;
    muscles_gpu.reserve(muscles.size());
    
    for (const auto& muscle : muscles) {
        MuscleGPU m = {};
        m.activation = muscle.activation;
        m.thickness = muscle.thickness;
        m.r = muscle.color[0]; m.g = muscle.color[1]; m.b = muscle.color[2]; m.a = muscle.color[3];
        muscles_gpu.push_back(m);
    }
    
    return renderer->uploadMuscles(muscles_gpu.data(), static_cast<uint32_t>(muscles_gpu.size()));
}

bool CreatureRenderer::upload_organs_to_gpu(VulkanRenderer* renderer,
                                             const std::vector<OrganRenderData>& organs) {
    if (!renderer) return false;
    
    std::vector<OrganGPU> organs_gpu;
    organs_gpu.reserve(organs.size());
    
    for (const auto& organ : organs) {
        OrganGPU o = {};
        o.center_x = organ.position[0]; o.center_y = organ.position[1]; o.center_z = organ.position[2];
        o.radius = organ.radius;
        o.r = organ.color[0]; o.g = organ.color[1]; o.b = organ.color[2]; o.a = organ.color[3];
        o.pulse_intensity = organ.pulse_intensity;
        organs_gpu.push_back(o);
    }
    
    return renderer->uploadOrgans(organs_gpu.data(), static_cast<uint32_t>(organs_gpu.size()));
}
#endif

bool CreatureRenderer::load_glb_model(const std::string& path,
                                        std::vector<float>& out_vertices,
                                        std::vector<uint32_t>& out_indices) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    if (file_size < 12) return false;
    
    char header[4];
    file.read(header, 4);
    if (header[0] != 'g' || header[1] != 'l' || header[2] != 'T' || header[3] != 'F') {
        return false;
    }
    
    return true;
}

void CreatureRenderer::parse_bone_geometry(const std::vector<float>& vertices,
                                            const std::vector<uint32_t>& indices,
                                            std::vector<BoneRenderData>& out_bones) {
    if (vertices.empty()) return;
    
    float min_x = vertices[0], max_x = vertices[0];
    float min_y = vertices[1], max_y = vertices[1];
    float min_z = vertices[2], max_z = vertices[2];
    
    for (size_t i = 0; i < vertices.size(); i += 3) {
        min_x = std::fmin(min_x, vertices[i]);
        max_x = std::fmax(max_x, vertices[i]);
        min_y = std::fmin(min_y, vertices[i + 1]);
        max_y = std::fmax(max_y, vertices[i + 1]);
        min_z = std::fmin(min_z, vertices[i + 2]);
        max_z = std::fmax(max_z, vertices[i + 2]);
    }
    
    BoneRenderData bone = {};
    bone.start_pos[0] = min_x; bone.start_pos[1] = min_y; bone.start_pos[2] = min_z;
    bone.end_pos[0] = max_x; bone.end_pos[1] = max_y; bone.end_pos[2] = max_z;
    bone.radius = 0.5f;
    bone.color[0] = 0.9f; bone.color[1] = 0.85f; bone.color[2] = 0.8f; bone.color[3] = 1.0f;
    bone.is_fractured = 0;
    
    out_bones.push_back(bone);
}

CreatureVisualizer::CreatureVisualizer() : renderer_(nullptr), visible_(true), render_mode_(MODE_SOLID) {
}

CreatureVisualizer::~CreatureVisualizer() {
    shutdown();
}

bool CreatureVisualizer::initialize(CreatureRenderer* renderer) {
    if (!renderer) return false;
    renderer_ = renderer;
    return true;
}

void CreatureVisualizer::shutdown() {
    renderer_ = nullptr;
}

void CreatureVisualizer::set_render_mode(uint32_t mode) {
    render_mode_ = mode;
}

void CreatureVisualizer::render_creature(const Creature* creature) {
    if (!visible_ || !creature) return;
    
    CreatureRenderData data;
    renderer_->prepare_creature_render_data(creature, data);
    
    const auto* skeleton = creature->get_skeleton();
    if (skeleton) {
        std::vector<BoneRenderData> bones;
        std::vector<MuscleRenderData> muscles;
        std::vector<OrganRenderData> organs;
        
        renderer_->prepare_skeleton_render_data(skeleton, bones, muscles, organs);
        
        render_skeleton(skeleton, true);
    }
}

void CreatureVisualizer::render_skeleton(const Skeleton* skeleton, bool show_muscles) {
    if (!visible_ || !skeleton) return;
    
    std::vector<BoneRenderData> bones;
    std::vector<MuscleRenderData> muscles;
    std::vector<OrganRenderData> organs;
    
    renderer_->prepare_skeleton_render_data(skeleton, bones, muscles, organs);
}

void CreatureVisualizer::render_psv_overlay(const Creature* creature, const EntityPSV& psv) {
    if (!visible_ || !creature) return;
    
    CreatureRenderData data;
    renderer_->prepare_creature_render_data(creature, data);
    renderer_->apply_psv_visualization(data, psv);
}

SkeletonMeshLoader::SkeletonMeshLoader() {
}

SkeletonMeshLoader::~SkeletonMeshLoader() {
    clear_cache();
}

bool SkeletonMeshLoader::load_skull_mesh(const std::string& path) {
    auto vertices = parse_glb_vertices(path);
    auto indices = parse_glb_indices(path);
    
    LoadedMesh mesh;
    mesh.vertices = vertices;
    mesh.indices = indices;
    mesh.vertex_count = static_cast<uint32_t>(vertices.size() / 3);
    mesh.index_count = static_cast<uint32_t>(indices.size());
    
    loaded_meshes_.push_back({"skull", mesh});
    return true;
}

bool SkeletonMeshLoader::load_jaw_mesh(const std::string& path) {
    auto vertices = parse_glb_vertices(path);
    auto indices = parse_glb_indices(path);
    
    LoadedMesh mesh;
    mesh.vertices = vertices;
    mesh.indices = indices;
    mesh.vertex_count = static_cast<uint32_t>(vertices.size() / 3);
    mesh.index_count = static_cast<uint32_t>(indices.size());
    
    loaded_meshes_.push_back({"jaw", mesh});
    return true;
}

bool SkeletonMeshLoader::load_eye_mesh(const std::string& path) {
    auto vertices = parse_glb_vertices(path);
    auto indices = parse_glb_indices(path);
    
    LoadedMesh mesh;
    mesh.vertices = vertices;
    mesh.indices = indices;
    mesh.vertex_count = static_cast<uint32_t>(vertices.size() / 3);
    mesh.index_count = static_cast<uint32_t>(indices.size());
    
    loaded_meshes_.push_back({"eye", mesh});
    return true;
}

bool SkeletonMeshLoader::load_mechanical_model(const std::string& path) {
    return load_skull_mesh(path);
}

bool SkeletonMeshLoader::load_plant_segments(const std::string& path) {
    auto vertices = parse_glb_vertices(path);
    auto indices = parse_glb_indices(path);
    
    LoadedMesh mesh;
    mesh.vertices = vertices;
    mesh.indices = indices;
    mesh.vertex_count = static_cast<uint32_t>(vertices.size() / 3);
    mesh.index_count = static_cast<uint32_t>(indices.size());
    
    loaded_meshes_.push_back({"plant_segments", mesh});
    return true;
}

const SkeletonMeshLoader::LoadedMesh* SkeletonMeshLoader::get_mesh(const std::string& name) const {
    for (const auto& pair : loaded_meshes_) {
        if (pair.first == name) {
            return &pair.second;
        }
    }
    return nullptr;
}

std::vector<std::string> SkeletonMeshLoader::get_loaded_mesh_names() const {
    std::vector<std::string> names;
    for (const auto& pair : loaded_meshes_) {
        names.push_back(pair.first);
    }
    return names;
}

void SkeletonMeshLoader::clear_cache() {
    loaded_meshes_.clear();
}

std::vector<float> SkeletonMeshLoader::parse_glb_vertices(const std::string& path) {
    return std::vector<float>();
}

std::vector<uint32_t> SkeletonMeshLoader::parse_glb_indices(const std::string& path) {
    return std::vector<uint32_t>();
}

CreatureRenderer* create_creature_renderer() {
    auto* renderer = new CreatureRenderer();
    renderer->initialize();
    return renderer;
}

void destroy_creature_renderer(CreatureRenderer* renderer) {
    if (renderer) {
        renderer->shutdown();
        delete renderer;
    }
}

}