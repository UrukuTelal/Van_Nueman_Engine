#ifndef VAN_NUEMAN_PROTOCOL_H
#define VAN_NUEMAN_PROTOCOL_H

#include <cstdint>
#include <cstddef>
#include "../include/Entity.h"

#if defined(_MSC_VER)
#define PACKED(...) __pragma(pack(push, 1)) __VA_ARGS__ __pragma(pack(pop))
#else
#define PACKED(...) __VA_ARGS__ __attribute__((packed))
#endif

namespace VanNueman::Protocol {

enum MessageType : uint8_t {
    MSG_FIRST = 0x01,
    MSG_CLIENT_INPUT = 0x01,
    MSG_SERVER_FEEDBACK = 0x02,
    MSG_RENDER_PACKET = 0x03,
    MSG_CHUNK_UPDATE = 0x04,
    MSG_FEDERATION_MSG = 0x05,
    MSG_PILLAR_UPDATE = 0x06,
    MSG_STAR_CLUSTER = 0x07,    // Star cluster state
    MSG_PLANET_DATA = 0x08,     // Planet information
    MSG_LIFEFORM_UPDATE = 0x09, // Lifeform state
    MSG_FIND_LIFE = 0x0A,       // Scan for life
    MSG_COLLECT_GENETIC = 0x0B, // Collect genetic material
    MSG_TERRAFORM = 0x0C,       // Terraform planet
    MSG_COMBINE_GENETIC = 0x0D, // Combine genetics
    MSG_LAST = 0x0D
};

PACKED(struct MessageHeader {
    uint32_t magic;
    MessageType type;
    uint16_t flags;
    uint32_t payload_size;
    uint32_t checksum;
});

PACKED(struct ClientInput {
    uint32_t player_id;
    float thrust;
    float pitch;
    float yaw;
    uint8_t actions;
});

PACKED(struct RenderPacket {
    MessageHeader header;
    uint32_t chunk_id;
    uint8_t lod_level;
    uint16_t changed_count;
    float svo_chunk[];
});

PACKED(struct FederationMsg {
    uint32_t source_server;
    uint32_t target_server;
    uint8_t hop_count;
    float force;
    uint8_t pillar;
});

PACKED(struct FeedbackMsg {
    MessageHeader header;
    uint32_t entity_id;
    float pillar_delta[NUM_PILLARS];
});

constexpr uint16_t VERSION = 1;
constexpr uint32_t MAGIC = 0x564E4D4E; // "VNMN"
constexpr uint32_t MAX_PAYLOAD_SIZE = 4096;

PACKED(struct StarClusterMsg {
    uint32_t star_count;
    uint32_t planet_count;
    uint32_t lifeform_count;
    float simulation_time;
});

PACKED(struct StarData {
    uint32_t id;
    float x, y, z;
    uint8_t star_type;  // StarType enum
    float mass;
    float temperature;
});

PACKED(struct PlanetData {
    uint32_t id;
    uint32_t star_id;
    float orbital_distance;
    float radius;
    float surface_temp;
    uint8_t habitable;  // bool
    uint32_t lifeform_count;
});

PACKED(struct LifeformData {
    uint32_t id;
    uint32_t planet_id;
    float x, y, z;
    float energy;
    float fitness;
    float pillars[NUM_PILLARS];  // 16 pillars
    uint8_t alive;  // bool
});

PACKED(struct FindLifeRequest {
    uint32_t player_id;
    float scan_x, scan_y, scan_z;
    float scan_radius;
});

PACKED(struct CollectGeneticRequest {
    uint32_t player_id;
    uint32_t lifeform_id;
});

PACKED(struct TerraformRequest {
    uint32_t player_id;
    uint32_t planet_id;
    float terraform_type; // 0 = atmosphere, 1 = temperature, etc.
    float intensity;
});

PACKED(struct CombineGeneticRequest {
    uint32_t player_id;
    uint32_t genetic_id1;
    uint32_t genetic_id2;
    float mutation_rate;
});

PACKED(struct GeneticMaterial {
    uint32_t id;
    float pillars[NUM_PILLARS];
    uint32_t source_lifeform_id;
});

PACKED(struct TerraformResult {
    uint32_t planet_id;
    float new_temperature;
    float new_atmosphere;
    uint8_t success;
});

PACKED(struct CombineResult {
    uint32_t new_genetic_id;
    float new_pillars[NUM_PILLARS];
    uint8_t viable;
});

} // namespace
#endif
