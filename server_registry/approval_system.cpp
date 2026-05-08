// approval_system.cpp - Server registry and approval implementation

#include "approval_system.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

struct ServerInfoImpl {
    uint32_t id;
    char name[128];
    char ip[64];
    int port;
    uint32_t player_count;
    uint32_t max_players;
    float pillars[NUM_PILLARS];
    bool is_approved;
    bool is_public;
    time_t last_heartbeat;
};

struct ApprovalSystemImpl {
    char registry_db_path[256];
    ServerInfoImpl* servers;
    uint32_t server_capacity;
    uint32_t server_count;
};

ApprovalSystem approval_create(const char* registry_db_path) {
    ApprovalSystemImpl* asys = (ApprovalSystemImpl*)malloc(sizeof(ApprovalSystemImpl));
    memset(asys, 0, sizeof(ApprovalSystemImpl));
    
    strncpy(asys->registry_db_path, registry_db_path, 255);
    asys->registry_db_path[255] = '\0';
    
    asys->server_capacity = 1024;
    asys->servers = (ServerInfoImpl*)malloc(sizeof(ServerInfoImpl) * asys->server_capacity);
    memset(asys->servers, 0, sizeof(ServerInfoImpl) * asys->server_capacity);
    asys->server_count = 0;
    
    return asys;
}

void approval_destroy(ApprovalSystem asys) {
    if (!asys) return;
    ApprovalSystemImpl* impl = (ApprovalSystemImpl*)asys;
    
    free(impl->servers);
    free(impl);
}

int32_t approval_register_server(ApprovalSystem asys, const ServerInfo* info) {
    if (!asys || !info) return -1;
    ApprovalSystemImpl* impl = (ApprovalSystemImpl*)asys;
    
    // Check if already exists
    for (uint32_t i = 0; i < impl->server_count; i++) {
        if (impl->servers[i].id == info->id) {
            // Update existing
            memcpy(&impl->servers[i], info, sizeof(ServerInfo));
            impl->servers[i].last_heartbeat = time(nullptr);
            return 0;
        }
    }
    
    // Check capacity
    if (impl->server_count >= impl->server_capacity) {
        impl->server_capacity *= 2;
        impl->servers = (ServerInfoImpl*)realloc(impl->servers, 
                                                  sizeof(ServerInfoImpl) * impl->server_capacity);
    }
    
    // Add new
    ServerInfoImpl* server = &impl->servers[impl->server_count++];
    memset(server, 0, sizeof(ServerInfoImpl));
    server->id = info->id;
    strncpy(server->name, info->name, 127);
    strncpy(server->ip, info->ip, 63);
    server->port = info->port;
    server->player_count = info->player_count;
    server->max_players = info->max_players;
    memcpy(server->pillars, info->pillars, sizeof(float) * NUM_PILLARS);
    server->is_approved = false;  // Require approval by default
    server->is_public = info->is_public;
    server->last_heartbeat = time(nullptr);
    
    return 0;
}

int32_t approval_approve_server(ApprovalSystem asys, uint32_t server_id, bool approved) {
    if (!asys) return -1;
    ApprovalSystemImpl* impl = (ApprovalSystemImpl*)asys;
    
    for (uint32_t i = 0; i < impl->server_count; i++) {
        if (impl->servers[i].id == server_id) {
            impl->servers[i].is_approved = approved;
            return 0;
        }
    }
    
    return -1;  // Not found
}

int32_t approval_get_server_list(ApprovalSystem asys, ServerInfo* out_list, 
                               uint32_t max_count, bool only_approved) {
    if (!asys || !out_list) return -1;
    ApprovalSystemImpl* impl = (ApprovalSystemImpl*)asys;
    
    uint32_t copied = 0;
    for (uint32_t i = 0; i < impl->server_count && copied < max_count; i++) {
        if (only_approved && !impl->servers[i].is_approved) continue;
        if (!impl->servers[i].is_public) continue;
        
        memcpy(&out_list[copied++], &impl->servers[i], sizeof(ServerInfo));
    }
    
    return copied;
}

int32_t approval_heartbeat(ApprovalSystem asys, uint32_t server_id) {
    if (!asys) return -1;
    ApprovalSystemImpl* impl = (ApprovalSystemImpl*)asys;
    
    for (uint32_t i = 0; i < impl->server_count; i++) {
        if (impl->servers[i].id == server_id) {
            impl->servers[i].last_heartbeat = time(nullptr);
            return 0;
        }
    }
    
    return -1;
}

int32_t approval_remove_server(ApprovalSystem asys, uint32_t server_id) {
    if (!asys) return -1;
    ApprovalSystemImpl* impl = (ApprovalSystemImpl*)asys;
    
    for (uint32_t i = 0; i < impl->server_count; i++) {
        if (impl->servers[i].id == server_id) {
            // Shift remaining
            for (uint32_t j = i; j < impl->server_count - 1; j++) {
                impl->servers[j] = impl->servers[j + 1];
            }
            impl->server_count--;
            return 0;
        }
    }
    
    return -1;
}

bool approval_is_valid_server(ApprovalSystem asys, uint32_t server_id) {
    if (!asys) return false;
    ApprovalSystemImpl* impl = (ApprovalSystemImpl*)asys;
    
    time_t now = time(nullptr);
    
    for (uint32_t i = 0; i < impl->server_count; i++) {
        if (impl->servers[i].id == server_id) {
            // Check if approved and not timed out (60 second timeout)
            if (!impl->servers[i].is_approved) return false;
            if (now - impl->servers[i].last_heartbeat > 60) return false;
            return true;
        }
    }
    
    return false;
}
