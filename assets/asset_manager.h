// asset_manager.h - Asset management system

#pragma once

#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

// Asset types
typedef enum {
    ASSET_IMAGE = 0,
    ASSET_AUDIO = 1,
    ASSET_FONT = 2,
    ASSET_SHADER = 3,
    ASSET_MODEL = 4
} AssetType;

// Asset info
typedef struct AssetInfo {
    const char* name;
    const char* file_path;
    AssetType type;
    uint32_t size_bytes;
    uint8_t* data;  // Loaded data (nullptr if not loaded)
    bool is_loaded;
} AssetInfo;

// Asset manager handle
typedef struct AssetManagerImpl* AssetManager;

// Create asset manager
AssetManager asset_manager_create(const char* assets_root);

// Destroy asset manager
void asset_manager_destroy(AssetManager mgr);

// Register asset
int32_t asset_register(AssetManager mgr, const char* name, const char* file_path, AssetType type);

// Load asset into memory
int32_t asset_load(AssetManager mgr, const char* name);

// Unload asset from memory
void asset_unload(AssetManager mgr, const char* name);

// Get asset data
const AssetInfo* asset_get(AssetManager mgr, const char* name);

// Load all assets of type
int32_t asset_load_all_of_type(AssetManager mgr, AssetType type);

// Check if asset exists on disk
bool asset_exists(AssetManager mgr, const char* name);

// Get asset count
uint32_t asset_count(AssetManager mgr);

// Hot-reload (check file modification time)
void asset_check_reload(AssetManager mgr, const char* name);

#ifdef __cplusplus
}
#endif
