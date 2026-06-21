#pragma once

#include "PillarArrays.h"
#include <vector>
#include <memory>
#include <cstdint>
#include <algorithm>

class AgentCognition;

namespace vn {
namespace simulation {

class AgentECS {
public:
    using Index = uint32_t;
    static const Index InvalidIndex = UINT32_MAX;

    AgentECS() = default;
    explicit AgentECS(size_t capacity) { reserve(capacity); }

    void reserve(size_t capacity) {
        pillars_.reserve(capacity);
        x_.reserve(capacity);
        y_.reserve(capacity);
        z_.reserve(capacity);
        vx_.reserve(capacity);
        vy_.reserve(capacity);
        vz_.reserve(capacity);
        active_.reserve(capacity);
        resources_.reserve(capacity);
        last_hash_x_.reserve(capacity);
        last_hash_y_.reserve(capacity);
        last_hash_z_.reserve(capacity);
        cognitions_.reserve(capacity);
    }

    Index add_agent(const PillarStateVector& initial_pillars,
                    float x, float y, float z,
                    float vx, float vy, float vz,
                    bool active,
                    int resources,
                    float last_hash_x, float last_hash_y, float last_hash_z,
                    std::unique_ptr<AgentCognition> cognition) {
        const Index idx = static_cast<Index>(size_);

        // Make sure each vector has at least idx+1 elements.
        // Since we reserved, we can assume capacity, but we need to resize if we didn't reserve enough.
        // We'll use a helper to ensure size.

        auto ensure_size = [&](std::vector<float>& vec) {
            if (vec.size() <= idx) vec.resize(idx + 1);
        };
        auto ensure_size_u8 = [&](std::vector<uint8_t>& vec) {
            if (vec.size() <= idx) vec.resize(idx + 1);
        };
        auto ensure_size_int = [&](std::vector<int>& vec) {
            if (vec.size() <= idx) vec.resize(idx + 1);
        };
        auto ensure_size_cog = [&](std::vector<std::unique_ptr<AgentCognition>>& vec) {
            if (vec.size() <= idx) vec.resize(idx + 1);
        };

        ensure_size(x_);
        ensure_size(y_);
        ensure_size(z_);
        ensure_size(vx_);
        ensure_size(vy_);
        ensure_size(vz_);
        ensure_size_u8(active_);
        ensure_size_int(resources_);
        ensure_size(last_hash_x_);
        ensure_size(last_hash_y_);
        ensure_size(last_hash_z_);
        ensure_size_cog(cognitions_);

        // Set the values
        x_[idx] = x;
        y_[idx] = y;
        z_[idx] = z;
        vx_[idx] = vx;
        vy_[idx] = vy;
        vz_[idx] = vz;
        active_[idx] = active;
        resources_[idx] = resources;
        last_hash_x_[idx] = last_hash_x;
        last_hash_y_[idx] = last_hash_y;
        last_hash_z_[idx] = last_hash_z;
        cognitions_[idx] = std::move(cognition);

        // Ensure PillarArrays is sized before writing (fix UB on add after reserve)
        pillars_.resize(idx + 1);
        // Set the initial pillars
        pillars_.set_pillars(idx, initial_pillars);

        size_++;
        return idx;
    }

    void remove_agent(Index idx) {
        if (!is_valid(idx)) return;
        if (size_ == 0) return;

        const Index last_idx = size_ - 1;
        if (idx != last_idx) {
            // Swap with last element for all arrays
            std::swap(x_[idx], x_[last_idx]);
            std::swap(y_[idx], y_[last_idx]);
            std::swap(z_[idx], z_[last_idx]);
            std::swap(vx_[idx], vx_[last_idx]);
            std::swap(vy_[idx], vy_[last_idx]);
            std::swap(vz_[idx], vz_[last_idx]);
            std::swap(active_[idx], active_[last_idx]);
            std::swap(resources_[idx], resources_[last_idx]);
            std::swap(last_hash_x_[idx], last_hash_x_[last_idx]);
            std::swap(last_hash_y_[idx], last_hash_y_[last_idx]);
            std::swap(last_hash_z_[idx], last_hash_z_[last_idx]);
            std::swap(cognitions_[idx], cognitions_[last_idx]);
            pillars_.swap_remove(idx, last_idx);
        }
        // Pop back all vectors to match logical size
        x_.pop_back();
        y_.pop_back();
        z_.pop_back();
        vx_.pop_back();
        vy_.pop_back();
        vz_.pop_back();
        active_.pop_back();
        resources_.pop_back();
        last_hash_x_.pop_back();
        last_hash_y_.pop_back();
        last_hash_z_.pop_back();
        cognitions_.pop_back();
        pillars_.resize(size_ - 1);
        size_--;
    }

    bool is_valid(Index idx) const {
        return idx < size_;
    }

    Index size() const { return size_; }
    Index capacity() const {
        if (x_.empty()) return 0;
        return x_.capacity();
    }

    // Pillar access
    vn::fp20_t& pillar(Index idx, PillarIndex pillar) {
        return pillars_.pillar(pillar, idx);
    }
    const vn::fp20_t& pillar(Index idx, PillarIndex pillar) const {
        return pillars_.pillar(pillar, idx);
    }

    // Batch pillar access: returns a state vector for a single entity
    PillarStateVector get_pillars(Index idx) const {
        PillarStateVector psv;
        for (int i = 0; i < NumPillars; i++)
            psv[i] = pillars_.pillar(static_cast<PillarIndex>(i), idx);
        return psv;
    }

    // Const reference to all pillars for index-based iteration
    const PillarArrays& pillar_arrays() const { return pillars_; }
    PillarArrays& pillar_arrays() { return pillars_; }

    // Per-entity pillar array access (returns underlying pillar array for all entities)
    std::vector<vn::fp20_t>& pillar_array(PillarIndex p) { return pillars_.pillar_array(p); }
    const std::vector<vn::fp20_t>& pillar_array(PillarIndex p) const { return pillars_.pillar_array(p); }

    // Position
    float& x(Index idx) { return x_[idx]; }
    float& y(Index idx) { return y_[idx]; }
    float& z(Index idx) { return z_[idx]; }
    const float& x(Index idx) const { return x_[idx]; }
    const float& y(Index idx) const { return y_[idx]; }
    const float& z(Index idx) const { return z_[idx]; }

    // Velocity
    float& vx(Index idx) { return vx_[idx]; }
    float& vy(Index idx) { return vy_[idx]; }
    float& vz(Index idx) { return vz_[idx]; }
    const float& vx(Index idx) const { return vx_[idx]; }
    const float& vy(Index idx) const { return vy_[idx]; }
    const float& vz(Index idx) const { return vz_[idx]; }

    // Active
    uint8_t& active(Index idx) { return active_[idx]; }
    uint8_t active(Index idx) const { return active_[idx]; }

    // Resources
    int& resources(Index idx) { return resources_[idx]; }
    int resources(Index idx) const { return resources_[idx]; }

    // Last hash
    float& last_hash_x(Index idx) { return last_hash_x_[idx]; }
    float& last_hash_y(Index idx) { return last_hash_y_[idx]; }
    float& last_hash_z(Index idx) { return last_hash_z_[idx]; }
    const float& last_hash_x(Index idx) const { return last_hash_x_[idx]; }
    const float& last_hash_y(Index idx) const { return last_hash_y_[idx]; }
    const float& last_hash_z(Index idx) const { return last_hash_z_[idx]; }

    // Cognition
    AgentCognition* cognition(Index idx) { return cognitions_[idx].get(); }
    const AgentCognition* cognition(Index idx) const { return cognitions_[idx].get(); }

private:
    PillarArrays pillars_;
    std::vector<float> x_, y_, z_;
    std::vector<float> vx_, vy_, vz_;
    std::vector<uint8_t> active_;  // uint8_t instead of bool to avoid vector<bool> proxy issues
    std::vector<int> resources_;
    std::vector<float> last_hash_x_, last_hash_y_, last_hash_z_;
    std::vector<std::unique_ptr<AgentCognition>> cognitions_;
    size_t size_ = 0;

}; // class AgentECS

} // namespace simulation
} // namespace vn