// approval_system.h - Server registry and approval system

#pragma once

#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

// Server info for registry
typedef struct ServerInfo {
    uint32_t id;
    char name[128];
    char ip[64];
    int port;
    uint32_t player_count;
    uint32_t max_players;
    float pillars[NUM_PILLARS];  // Server's pillar state
    bool is_approved;
    bool is_public;
} ServerInfo;

// Approval system handle
typedef struct ApprovalSystemImpl* ApprovalSystem;

// Create approval system
ApprovalSystem approval_create(const char* registry_db_path);

// Destroy approval system
void approval_destroy(ApprovalSystem asys);

// Register server (requires approval)
int32_t approval_register_server(ApprovalSystem asys, const ServerInfo* info);

// Approve server (admin action)
int32_t approval_approve_server(ApprovalSystem asys, uint32_t server_id, bool approved);

// Get server list (filtered by public/approved)
int32_t approval_get_server_list(ApprovalSystem asys, ServerInfo* out_list, uint32_t max_count, bool only_approved);

// Update server heart-beat
int32_t approval_heartbeat(ApprovalSystem asys, uint32_t server_id);

// Remove server (timeout or admin action)
int32_t approval_remove_server(ApprovalSystem asys, uint32_t server_id);

// Validate server (check if approved and not banned)
bool approval_is_valid_server(ApprovalSystem asys, uint32_t server_id);

#ifdef __cplusplus
}
#endif
