// persistence.h - Header for database persistence

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
struct Entity;  // from Entity.h

// Initialize database connection
int32_t db_init(const char* connection_string);

// Save world state to database
int32_t db_save_world_state(const char* state_json, uint32_t size);

// Load world state from database
int32_t db_load_world_state(char* out_buffer, uint32_t buffer_size);

// Save entity state
int32_t db_save_entity(const struct Entity* entity, uint32_t entity_id);

// Load entity state
int32_t db_load_entity(uint32_t entity_id, struct Entity* out_entity);

// Close database connection
void db_close();

#ifdef __cplusplus
}

// VanNuemanDB class for C++ users
class VanNuemanDB {
public:
    VanNuemanDB(const std::string& conn_str);
    ~VanNuemanDB();

    // Build connection string from environment variables with timeout
    static std::string build_connection_string();
    bool is_connected() const;
    bool save_svo_chunk(int server_id, int x, int y, int z, int level,
                        const void* voxel_data, size_t data_size);
    std::vector<uint8_t> load_svo_chunk(int server_id, int x, int y, int z, int level);
    bool save_entity(int64_t entity_id, const char* entity_json);
    bool load_entity(int64_t entity_id, char* out_json, size_t out_size);
    bool save_world_state(const char* state_json);
    std::string load_world_state();
};

#endif
