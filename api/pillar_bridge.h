// pillar_bridge.h - C++ Bridge for PillarAI Integration
// Provides bidirectional sync between Engine entities and PillarAI

#pragma once

#include <vector>
#include <string>
#include <memory>
#include <functional>
#include "../include/Entity.h"

namespace Van_Nueman {

// PSV: 16 pillars [Awareness, Willpower, Force, Influence, Resistance, Integrity,
//               Cohesion, Relation, Presence, Warmth, Memory, Attraction,
//               Harm, Distortion, Flux, Depth]
struct PillarState {
    std::array<float, 16> values;
    float timestamp;

    PillarState() : values({0.5f}), timestamp(0.0f) {
        values.fill(0.5f);  // Default neutral
    }
};

class PillarBridge {
public:
    static PillarBridge& get();

    // Initialize connection to PillarAI service
    bool initialize(const std::string& host = "localhost", int port = 8888);

    // Shutdown bridge
    void shutdown();

    // Push entity PSV to PillarAI
    void push_entity_psv(uint32_t entity_id, const PillarState& psv);

    // Pull entity PSV from PillarAI
    bool pull_entity_psv(uint32_t entity_id, PillarState& out_psv);

    // Broadcast PSV update to all connected services
    void broadcast_update();

    // Register callback for incoming PSV changes
    using PSVCallback = std::function<void(uint32_t, const PillarState&)>;
    void register_callback(PSVCallback callback);

    // Check if bridge is connected
    bool is_connected() const { return connected_; }

private:
    PillarBridge() = default;
    ~PillarBridge() = default;
    PillarBridge(const PillarBridge&) = delete;
    PillarBridge& operator=(const PillarBridge&) = delete;

    bool connected_ = false;
    std::string host_;
    int port_ = 8888;
    PSVCallback callback_;

    std::vector<uint32_t> pending_updates_;
};

// Helper functions for PSV manipulation
namespace PillarMath {
    // Calculate distance between two PSV states (Euclidean in 16D)
    float psv_distance(const PillarState& a, const PillarState& b);

    // Interpolate between two PSV states
    PillarState psv_lerp(const PillarState& a, const PillarState& b, float t);

    // Apply pillar coupling rules
    PillarState apply_coupling(const PillarState& ps, float dt);

    // Check if PSV is stable (no rapid changes)
    bool is_stable(const PillarState& ps, float threshold = 0.1f);

    // Generate identity hash from PSV
    uint64_t psv_hash(const PillarState& ps);
}

} // namespace Van_Nueman