#pragma once

#include "../voxel/VoxelCell.h"
#include "../voxel/BCCIndex.h"
#include "../include/Entity.h"
#include "voxel_organism.h"
#include <vector>
#include <unordered_map>
#include <cmath>
#include <cstdint>
#include <cstdlib>

struct ResourcePacket {
    float warmth;
    float nutrients;
    float flux_charge;
    float harm;
    float information;

    ResourcePacket() : warmth(0), nutrients(0), flux_charge(0), harm(0), information(0) {}
};

struct CollectiveMemory {
    std::vector<uint64_t> successful_arrangements;
    uint32_t max_memories;

    CollectiveMemory() : max_memories(100) {}
};

struct SocietyNode {
    VoxelOrganism* organism;
    uint32_t node_id;
    float influence_radius;
    bool dominant;

    std::vector<uint32_t> connected_nodes;
    ResourcePacket local_resources;
    ResourcePacket shared_resources;

    SocietyNode() : organism(nullptr), node_id(0), influence_radius(10.0f), dominant(false) {}
};

class SocietyNetwork {
public:
    SocietyNetwork() : tick_count_(0) {}

    void add_organism(VoxelOrganism* org, uint32_t node_id, float influence = 10.0f) {
        SocietyNode node;
        node.organism = org;
        node.node_id = node_id;
        node.influence_radius = influence;
        node.dominant = false;
        nodes_.push_back(node);
        node_map_[node_id] = nodes_.size() - 1;
    }

    void remove_organism(uint32_t node_id) {
        auto it = node_map_.find(node_id);
        if (it == node_map_.end()) return;
        nodes_.erase(nodes_.begin() + it->second);
        node_map_.erase(it);
        rebuild_connections();
    }

    void connect(uint32_t a_id, uint32_t b_id) {
        auto it_a = node_map_.find(a_id);
        auto it_b = node_map_.find(b_id);
        if (it_a == node_map_.end() || it_b == node_map_.end()) return;
        SocietyNode& a = nodes_[it_a->second];
        SocietyNode& b = nodes_[it_b->second];
        for (auto nid : a.connected_nodes) if (nid == b_id) return;
        a.connected_nodes.push_back(b_id);
        b.connected_nodes.push_back(a_id);
    }

    void disconnect(uint32_t a_id, uint32_t b_id) {
        auto it_a = node_map_.find(a_id);
        auto it_b = node_map_.find(b_id);
        if (it_a == node_map_.end() || it_b == node_map_.end()) return;
        auto& a_conn = nodes_[it_a->second].connected_nodes;
        auto& b_conn = nodes_[it_b->second].connected_nodes;
        auto rm = [](std::vector<uint32_t>& vec, uint32_t val) {
            vec.erase(std::remove(vec.begin(), vec.end(), val), vec.end());
        };
        rm(a_conn, b_id);
        rm(b_conn, a_id);
    }

    void resource_sharing_tick() {
        if (nodes_.empty()) return;

        for (auto& node : nodes_) {
            if (!node.organism || !node.organism->is_alive()) continue;
            const auto& psv = node.organism->pillar_state();
            node.local_resources.warmth = psv[Warmth];
            node.local_resources.nutrients = psv[Flux] * 0.5f + psv[Cohesion] * 0.5f;
            node.local_resources.flux_charge = psv[Flux];
            node.local_resources.harm = psv[Harm];
            node.local_resources.information = psv[Memory];
        }

        for (auto& node : nodes_) {
            node.shared_resources = ResourcePacket();
            int connections = 0;
            for (uint32_t conn_id : node.connected_nodes) {
                auto it = node_map_.find(conn_id);
                if (it == node_map_.end()) continue;
                const auto& other = nodes_[it->second];
                node.shared_resources.warmth += other.local_resources.warmth;
                node.shared_resources.nutrients += other.local_resources.nutrients;
                node.shared_resources.flux_charge += other.local_resources.flux_charge;
                node.shared_resources.harm += other.local_resources.harm;
                node.shared_resources.information += other.local_resources.information;
                connections++;
            }
            if (connections > 0) {
                float inv = 1.0f / static_cast<float>(connections);
                node.shared_resources.warmth *= inv;
                node.shared_resources.nutrients *= inv;
                node.shared_resources.flux_charge *= inv;
                node.shared_resources.harm *= inv;
                node.shared_resources.information *= inv;
            }
        }

        for (auto& node : nodes_) {
            if (!node.organism || !node.organism->is_alive()) continue;
            float excess_warmth = node.local_resources.warmth - node.shared_resources.warmth;
            float excess_nutrients = node.local_resources.nutrients - node.shared_resources.nutrients;
            if (excess_warmth > 0.1f || excess_nutrients > 0.1f) {
                node.dominant = true;
            } else {
                node.dominant = false;
            }
        }

        tick_count_++;
    }

    void collective_decision_tick() {
        if (nodes_.empty()) return;

        float avg_information = 0.0f;
        float avg_harm = 0.0f;
        int alive_count = 0;
        for (const auto& node : nodes_) {
            if (!node.organism || !node.organism->is_alive()) continue;
            avg_information += node.local_resources.information;
            avg_harm += node.local_resources.harm;
            alive_count++;
        }
        if (alive_count == 0) return;
        avg_information /= alive_count;
        avg_harm /= alive_count;

        if (avg_harm > 0.7f) {
            uint32_t sacrifice = static_cast<uint32_t>(std::rand()) % nodes_.size();
            if (nodes_[sacrifice].organism && nodes_[sacrifice].organism->is_alive()) {
                nodes_[sacrifice].organism->tick(vn::fp20_t(1.0f), nullptr);
            }
        }

        if (avg_information > 0.8f) {
            for (auto& node : nodes_) {
                if (!node.organism || !node.organism->is_alive()) continue;
                if (node.local_resources.information > 0.5f) {
                    for (uint32_t conn_id : node.connected_nodes) {
                        auto it = node_map_.find(conn_id);
                        if (it == node_map_.end()) continue;
                        auto& other = nodes_[it->second];
                        other.local_resources.information =
                            (other.local_resources.information + node.local_resources.information) * 0.5f;
                    }
                }
            }
        }
    }

    void add_memory(uint64_t arrangement_hash) {
        memories_.successful_arrangements.push_back(arrangement_hash);
        if (memories_.successful_arrangements.size() > memories_.max_memories) {
            memories_.successful_arrangements.erase(memories_.successful_arrangements.begin());
        }
    }

    const std::vector<uint64_t>& get_memories() const {
        return memories_.successful_arrangements;
    }

    size_t node_count() const { return nodes_.size(); }
    size_t active_count() const {
        size_t count = 0;
        for (const auto& n : nodes_)
            if (n.organism && n.organism->is_alive()) count++;
        return count;
    }

    const SocietyNode& get_node(uint32_t index) const { return nodes_[index]; }
    uint32_t get_tick_count() const { return tick_count_; }

private:
    void rebuild_connections() {
        node_map_.clear();
        for (size_t i = 0; i < nodes_.size(); i++) {
            node_map_[nodes_[i].node_id] = i;
        }
    }

    std::vector<SocietyNode> nodes_;
    std::unordered_map<uint32_t, size_t> node_map_;
    CollectiveMemory memories_;
    uint32_t tick_count_;
};
