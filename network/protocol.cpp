// protocol.cpp - Implementation of VanNueman protocol

#include "protocol.h"
#include <cstring>
#include <cstdio>

namespace VanNueman {
namespace Protocol {

// Initialize a message header
void init_header(MessageHeader* h, MessageType type, uint32_t payload_size) {
    if (!h) return;
    h->magic = MAGIC;
    h->type = type;
    h->flags = 0;
    h->payload_size = payload_size;
    h->checksum = 0;  // Would calculate actual checksum
}

// Validate message header
bool validate_header(const MessageHeader* h) {
    if (!h) return false;
    if (h->magic != MAGIC) return false;
    return true;
}

// Render packet (server -> client)
void pack_render_packet(RenderPacket* pkt, const float* svo_data, uint32_t size) {
    if (!pkt || !svo_data) return;
    init_header(&pkt->header, MSG_RENDER_PACKET, sizeof(RenderPacket));
    // Copy SVO data (simplified)
    memcpy(pkt->svo_chunk, svo_data, (size < 1024) ? size : 1024);
}

// Unpack render packet
bool unpack_render_packet(const RenderPacket* pkt, float* out_svo_data, uint32_t* out_size) {
    if (!pkt || !validate_header(&pkt->header)) return false;
    if (pkt->header.type != MSG_RENDER_PACKET) return false;
    
    // Copy SVO data out
    if (out_svo_data) {
        memcpy(out_svo_data, pkt->svo_chunk, 1024);
    }
    if (out_size) {
        *out_size = 1024;  // Simplified
    }
    return true;
}

// Feedback message
void pack_feedback_msg(FeedbackMsg* msg, uint32_t entity_id, float pillars[NumPillars]) {
    if (!msg) return;
    init_header(&msg->header, MSG_FEEDBACK, sizeof(FeedbackMsg));
    msg->entity_id = entity_id;
    if (pillars) {
        memcpy(msg->pillar_delta, pillars, sizeof(float) * NumPillars);
    }
}

// Federation message
void pack_federation_msg(FederationMsg* msg, uint32_t src_server, uint32_t dst_server, 
                          float pillars[NumPillars]) {
    if (!msg) return;
    init_header(&msg->header, MSG_FEDERATION, sizeof(FederationMsg));
    msg->source_server = src_server;
    msg->target_server = dst_server;
    if (pillars) {
        memcpy(msg->pillar_delta, pillars, sizeof(float) * NumPillars);
    }
}

}  // namespace Protocol
}  // namespace VanNueman
