#ifndef FEDERATION_H
#define FEDERATION_H

#include <cstdint>
#include <cstring>

// Server-to-server log feedback federation
// No central server - P2P with logarithmic decay

struct FederationPeer {
    uint32_t peer_id;
    char address[64];
    float last_update;
    bool active;
};

struct FederationMessage {
    uint32_t from_peer;
    uint32_t to_peer;
    float pillar_state[16];  // 14 pillars at federation scale
    uint32_t hop_count;
    float timestamp;
};

// Logarithmic decay: decay = 1.0 / log(2 + hop_count)
inline float federation_decay(uint32_t hop_count) {
    return 1.0f / (1.0f + (float)hop_count * 0.5f);
}

// Send to peer with decay
inline void federation_send(FederationPeer& peer, const float pillars[16], FederationMessage& msg) {
    memcpy(msg.pillar_state, pillars, sizeof(float) * 16);
    msg.hop_count = 0;
    // peer address would be used for actual network send
}

// Apply received message with logarithmic feedback
inline void federation_apply(float local[16], const FederationMessage& msg) {
    float decay = federation_decay(msg.hop_count);
    
    for (int i = 0; i < 16; i++) {
        // Blend: (1-decay)*local + decay*received
        local[i] = local[i] * (1.0f - decay) + msg.pillar_state[i] * decay;
    }
}

#endif // FEDERATION_H