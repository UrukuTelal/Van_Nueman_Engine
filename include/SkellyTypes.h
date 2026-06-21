// SkellyTypes.h - Type definitions for SkellyAPI system
// Modular skeleton with bones, muscles, transports, and organs

#pragma once

#include <cstdint>
#include <cstring>
#include <algorithm>
#include <vector>
#include <string>
#include "Entity.h"

// Forward declarations
struct BoneSegment;
struct MuscleGroup;
struct TransportSystem;
struct Organ;

// 3D Vector (matching Taichi Vector.field)
struct Vec3 {
    float x, y, z;
    
    Vec3() : x(0), y(0), z(0) {}
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    
    Vec3 operator+(const Vec3& other) const {
        return Vec3(x + other.x, y + other.y, z + other.z);
    }
    
    Vec3 operator-(const Vec3& other) const {
        return Vec3(x - other.x, y - other.y, z - other.z);
    }
    
    Vec3 operator*(float scalar) const {
        return Vec3(x * scalar, y * scalar, z * scalar);
    }
    
    float norm() const {
        return sqrtf(x*x + y*y + z*z);
    }
};

// Skeleton node (joint)
struct SkeletonNode {
    uint32_t id;
    Vec3 local_pos;
    Vec3 global_pos_cache;
    SkeletonNode* parent;
    std::vector<SkeletonNode*> children;
    char name[64];
    bool is_fractured;
    
    // Constraints (pitch, yaw, roll)
    float pitch_min, pitch_max;
    float yaw_min, yaw_max;
    float roll_min, roll_max;
    
    SkeletonNode(uint32_t id_, const Vec3& pos, SkeletonNode* parent_ = nullptr, const char* name_ = "joint")
        : id(id_), local_pos(pos), parent(parent_), is_fractured(false)
    {
        pitch_min = -1.5f; pitch_max = 1.5f;
        yaw_min = -1.5f; yaw_max = 1.5f;
        roll_min = -0.5f; roll_max = 0.5f;
        strncpy(name, name_, 63);
        name[63] = '\0';
        if (parent) {
            parent->children.push_back(this);
        }
    }
    
    Vec3 get_global_position() {
        if (parent == nullptr || is_fractured) {
            global_pos_cache = local_pos;
            return local_pos;
        }
        global_pos_cache = parent->get_global_position() + local_pos;
        return global_pos_cache;
    }
};

// Bone segment (connects two nodes)
struct BoneSegment {
    uint32_t id;
    SkeletonNode* start;
    SkeletonNode* end;
    float flexibility;
    float break_threshold;
    bool is_fractured;
    float total_capacity;  // volume capacity
    std::vector<MuscleGroup*> attachments;
    std::vector<Organ*> organs;
    
    BoneSegment(uint32_t id_, SkeletonNode* start_, SkeletonNode* end_, float radius = 2.0f)
        : id(id_), start(start_), end(end_), flexibility(0.1f), 
          break_threshold(100.0f), is_fractured(false)
    {
        float len = (end->local_pos - start->local_pos).norm();
        total_capacity = 3.14159f * radius * radius * len;
    }
    
    float get_void_space(const std::vector<MuscleGroup*>& all_muscles);
};

// Muscle strand (part of a muscle group)
struct MuscleStrand {
    float origin_rot;
    float insertion_rot;
    float base_r;
    float current_r;
    
    MuscleStrand(float origin_rot_, float insertion_rot_, float base_r_ = 0.1f)
        : origin_rot(origin_rot_), insertion_rot(insertion_rot_), 
          base_r(base_r_), current_r(base_r_) {}
};

// Volumetric muscle group
struct MuscleGroup {
    uint32_t id;
    char name[64];
    SkeletonNode* origin;
    SkeletonNode* insertion;
    float activation;  // 0.0 to 1.0
    std::vector<MuscleStrand> strands;
    
    MuscleGroup(uint32_t id_, const char* name_, SkeletonNode* origin_, 
                SkeletonNode* insertion_, uint32_t strand_count = 8)
        : id(id_), origin(origin_), insertion(insertion_), activation(0.0f)
    {
        strncpy(name, name_, 63);
        name[63] = '\0';
        for (uint32_t i = 0; i < strand_count; i++) {
            float rot = (i / (float)strand_count) * 2 * 3.14159f;
            strands.emplace_back(rot, rot + 0.2f);
        }
    }
    
    void update_volume() {
        Vec3 curr_vec = insertion->get_global_position() - origin->get_global_position();
        float curr_len = curr_vec.norm();
        float rest_len = (insertion->local_pos - origin->local_pos).norm();
        float expansion = (rest_len > 0) ? sqrtf(rest_len / curr_len) : 1.0f;
        
        for (auto& strand : strands) {
            strand.current_r = strand.base_r * expansion * (1.0f + activation);
        }
    }
};

// Transport system base
struct TransportSystem {
    uint32_t id;
    char name[64];
    std::vector<SkeletonNode*> nodes;
    float elasticity;
    float flow_rate;
    float resistance;
    bool is_severed;
    
    TransportSystem(uint32_t id_, const char* name_, const std::vector<SkeletonNode*>& nodes_, 
                   float elasticity_ = 0.5f)
        : id(id_), nodes(nodes_), elasticity(elasticity_), 
          flow_rate(0.0f), resistance(1.0f), is_severed(false)
    {
        strncpy(name, name_, 63);
        name[63] = '\0';
    }
    
    virtual ~TransportSystem() = default;
};

// Interstitial fluid (specialized transport)
struct InterstitialFluid : TransportSystem {
    BoneSegment* segment;
    float pressure;
    
    InterstitialFluid(uint32_t id_, const char* name_, BoneSegment* segment_)
        : TransportSystem(id_, name_, {segment_->start, segment_->end}), 
          segment(segment_), pressure(1.0f)
    {}
    
    float calculate_turgor(const std::vector<MuscleGroup*>& muscles);
};

// Organ types
enum OrganType : uint32_t {
    ORGAN_PUMP = 0,
    ORGAN_VALVE = 1,
    ORGAN_POWER_PLANT = 2,
    ORGAN_FACTORY = 3
};

// Organellar node
struct Organ {
    uint32_t id;
    char name[64];
    OrganType type;
    float volume;
    float active_state;  // Pulse intensity or metabolic rate
    float energy_output;
    BoneSegment* attached_segment;
    
    Organ(uint32_t id_, const char* name_, OrganType type_, 
          BoneSegment* segment_, float vol = 2.0f)
        : id(id_), type(type_), volume(vol), active_state(0.0f), 
          energy_output(0.0f), attached_segment(segment_)
    {
        strncpy(name, name_, 63);
        name[63] = '\0';
    }
    
    void operate(TransportSystem* transport = nullptr) {
        switch (type) {
            case ORGAN_PUMP:
                if (transport) {
                    transport->flow_rate += (volume * active_state) / transport->resistance;
                }
                break;
            case ORGAN_VALVE:
                if (transport) {
                    transport->resistance = 1.0f + (active_state * 10.0f);
                }
                break;
            case ORGAN_POWER_PLANT:
                energy_output = volume * active_state * 5.0f;
                break;
            case ORGAN_FACTORY:
                energy_output = volume * active_state * 1.2f;
                break;
        }
    }
};

// Modular skeleton (master container)
struct ModularSkeleton {
    SkeletonNode* root;
    std::vector<BoneSegment*> segments;
    std::vector<MuscleGroup*> muscles;
    std::vector<TransportSystem*> transports;
    std::vector<Organ*> organs;
    
    ModularSkeleton() : root(nullptr) {}
    ModularSkeleton(const ModularSkeleton&) = delete;
    ModularSkeleton& operator=(const ModularSkeleton&) = delete;
    
    ~ModularSkeleton() {
        // Delete transport systems (InterstitialFluid instances)
        for (auto* transport : transports) {
            delete transport;
        }
        
        // Delete all muscle groups
        for (auto* muscle : muscles) {
            delete muscle;
        }
        
        // Delete all organs (stored in ModularSkeleton::organs)
        // Note: BoneSegment::organs contains the same pointers, but they're non-owning
        for (auto* organ : organs) {
            delete organ;
        }
        
        // Delete all bone segments
        for (auto* segment : segments) {
            delete segment;
        }
        
        // Recursively delete all skeleton nodes starting from root
        if (root) {
            deleteSkeletonTree(root);
            root = nullptr;
        }
    }
    
    std::pair<SkeletonNode*, BoneSegment*> add_bone(SkeletonNode* parent, 
                                                    const Vec3& local_pos, 
                                                    const char* name = "bone") {
        SkeletonNode* node = new SkeletonNode(next_node_id++, local_pos, parent, name);
        BoneSegment* seg = new BoneSegment(next_segment_id++, parent, node);
        segments.push_back(seg);
        transports.push_back(new InterstitialFluid(next_transport_id++, 
                                                   (std::string(name) + "_fluid").c_str(), 
                                                   seg));
        return {node, seg};
    }
    
    Organ* add_organ(const char* name, OrganType type, BoneSegment* segment, float vol = 5.0f) {
        Organ* organ = new Organ(next_organ_id++, name, type, segment, vol);
        organs.push_back(organ);
        segment->organs.push_back(organ);
        
        // Structural deformation if volume exceeds void space
        float void_space = segment->get_void_space(muscles);
        if (vol > void_space) {
            Vec3 new_local = segment->end->local_pos * (1.0f + (vol - void_space) / 15.0f);
            segment->end->local_pos = new_local;
        }
        return organ;
    }
    
private:
    static void deleteSkeletonTree(SkeletonNode* node) {
        if (!node) return;
        for (auto* child : node->children) {
            deleteSkeletonTree(child);
        }
        delete node;
    }
    
    uint32_t next_node_id = 0;
    uint32_t next_segment_id = 0;
    uint32_t next_transport_id = 0;
    uint32_t next_organ_id = 0;
    uint32_t next_muscle_id = 0;
};

// Calculate void space for a bone segment
inline float BoneSegment::get_void_space(const std::vector<MuscleGroup*>& all_muscles) {
    float occupied = 0.0f;
    for (auto* mg : all_muscles) {
        if (mg->origin == start && mg->insertion == end) {
            float curr_len = (mg->insertion->get_global_position() -
                            mg->origin->get_global_position()).norm();
            for (auto& strand : mg->strands) {
                occupied += 3.14159f * strand.current_r * strand.current_r * curr_len;
            }
        }
    }
    return std::max(0.0f, total_capacity - occupied);
}

// Calculate turgor pressure
inline float InterstitialFluid::calculate_turgor(const std::vector<MuscleGroup*>& muscles) {
    float occ = 0.0f;
    for (auto* mg : muscles) {
        if (mg->origin == segment->start) {
            for (auto& strand : mg->strands) {
                occ += strand.current_r;
            }
        }
    }
    pressure = 1.0f + (occ * 1.5f);
    segment->flexibility = std::max(0.01f, 0.1f / pressure);
    return pressure;
}


