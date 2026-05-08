// asset_manager.cpp - Asset management implementation

#include "asset_manager.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#endif

struct AssetInfoImpl {
    const char* name;
    const char* file_path;
    AssetType type;
    uint32_t size_bytes;
    uint8_t* data;
    bool is_loaded;
    time_t last_modified;
};

struct AssetManagerImpl {
    const char* assets_root;
    AssetInfoImpl* assets;
    uint32_t asset_capacity;
    uint32_t asset_count;
};

static uint32_t get_file_size(const char* file_path) {
    FILE* f = fopen(file_path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    uint32_t size = ftell(f);
    fclose(f);
    return size;
}

static time_t get_file_modified(const char* file_path) {
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExA(file_path, GetFileExInfoStandard, &fad)) return 0;
    return fad.ftLastWriteTime.dwLowDateTime;
#else
    struct stat st;
    if (stat(file_path, &st) != 0) return 0;
    return st.st_mtime;
#endif
}

AssetManager asset_manager_create(const char* assets_root) {
    AssetManagerImpl* mgr = (AssetManagerImpl*)malloc(sizeof(AssetManagerImpl));
    memset(mgr, 0, sizeof(AssetManagerImpl));
    
    mgr->assets_root = strdup(assets_root);
    mgr->asset_capacity = 1024;
    mgr->assets = (AssetInfoImpl*)malloc(sizeof(AssetInfoImpl) * mgr->asset_capacity);
    memset(mgr->assets, 0, sizeof(AssetInfoImpl) * mgr->asset_capacity);
    mgr->asset_count = 0;
    
    return mgr;
}

void asset_manager_destroy(AssetManager mgr) {
    if (!mgr) return;
    AssetManagerImpl* impl = (AssetManagerImpl*)mgr;
    
    for (uint32_t i = 0; i < impl->asset_count; i++) {
        if (impl->assets[i].data) free(impl->assets[i].data);
        free((void*)impl->assets[i].name);
        free((void*)impl->assets[i].file_path);
    }
    
    free((void*)impl->assets_root);
    free(impl->assets);
    free(impl);
}

int32_t asset_register(AssetManager mgr, const char* name, const char* file_path, AssetType type) {
    if (!mgr) return -1;
    AssetManagerImpl* impl = (AssetManagerImpl*)mgr;
    
    // Check if already exists
    for (uint32_t i = 0; i < impl->asset_count; i++) {
        if (strcmp(impl->assets[i].name, name) == 0) {
            // Update
            free((void*)impl->assets[i].file_path);
            impl->assets[i].file_path = strdup(file_path);
            impl->assets[i].type = type;
            return 0;
        }
    }
    
    // Expand if needed
    if (impl->asset_count >= impl->asset_capacity) {
        impl->asset_capacity *= 2;
        impl->assets = (AssetInfoImpl*)realloc(impl->assets, 
                                                sizeof(AssetInfoImpl) * impl->asset_capacity);
    }
    
    // Add new
    AssetInfoImpl* asset = &impl->assets[impl->asset_count++];
    memset(asset, 0, sizeof(AssetInfoImpl));
    asset->name = strdup(name);
    asset->file_path = strdup(file_path);
    asset->type = type;
    asset->is_loaded = false;
    asset->data = nullptr;
    asset->size_bytes = 0;
    
    return 0;
}

int32_t asset_load(AssetManager mgr, const char* name) {
    if (!mgr) return -1;
    AssetManagerImpl* impl = (AssetManagerImpl*)mgr;
    
    for (uint32_t i = 0; i < impl->asset_count; i++) {
        if (strcmp(impl->assets[i].name, name) == 0) {
            AssetInfoImpl* asset = &impl->assets[i];
            
            // Get file size
            asset->size_bytes = get_file_size(asset->file_path);
            if (asset->size_bytes == 0) return -1;
            
            // Load into memory
            FILE* f = fopen(asset->file_path, "rb");
            if (!f) return -1;
            
            if (asset->data) free(asset->data);
            asset->data = (uint8_t*)malloc(asset->size_bytes);
            fread(asset->data, 1, asset->size_bytes, f);
            fclose(f);
            
            asset->is_loaded = true;
            asset->last_modified = get_file_modified(asset->file_path);
            return 0;
        }
    }
    
    return -1;  // Not found
}

void asset_unload(AssetManager mgr, const char* name) {
    if (!mgr) return;
    AssetManagerImpl* impl = (AssetManagerImpl*)mgr;
    
    for (uint32_t i = 0; i < impl->asset_count; i++) {
        if (strcmp(impl->assets[i].name, name) == 0) {
            AssetInfoImpl* asset = &impl->assets[i];
            if (asset->data) {
                free(asset->data);
                asset->data = nullptr;
            }
            asset->is_loaded = false;
            asset->size_bytes = 0;
            return;
        }
    }
}

const AssetInfo* asset_get(AssetManager mgr, const char* name) {
    if (!mgr) return nullptr;
    AssetManagerImpl* impl = (AssetManagerImpl*)mgr;
    
    for (uint32_t i = 0; i < impl->asset_count; i++) {
        if (strcmp(impl->assets[i].name, name) == 0) {
            return (const AssetInfo*)&impl->assets[i];
        }
    }
    
    return nullptr;
}

int32_t asset_load_all_of_type(AssetManager mgr, AssetType type) {
    if (!mgr) return -1;
    AssetManagerImpl* impl = (AssetManagerImpl*)mgr;
    int32_t loaded = 0;
    
    for (uint32_t i = 0; i < impl->asset_count; i++) {
        if (impl->assets[i].type == type) {
            if (asset_load(mgr, impl->assets[i].name) == 0) {
                loaded++;
            }
        }
    }
    
    return loaded;
}

bool asset_exists(AssetManager mgr, const char* name) {
    if (!mgr) return false;
    AssetManagerImpl* impl = (AssetManagerImpl*)mgr;
    
    for (uint32_t i = 0; i < impl->asset_count; i++) {
        if (strcmp(impl->assets[i].name, name) == 0) {
            return true;
        }
    }
    
    return false;
}

uint32_t asset_count(AssetManager mgr) {
    if (!mgr) return 0;
    AssetManagerImpl* impl = (AssetManagerImpl*)mgr;
    return impl->asset_count;
}

void asset_check_reload(AssetManager mgr, const char* name) {
    if (!mgr) return;
    AssetManagerImpl* impl = (AssetManagerImpl*)mgr;
    
    for (uint32_t i = 0; i < impl->asset_count; i++) {
        if (strcmp(impl->assets[i].name, name) == 0) {
            AssetInfoImpl* asset = &impl->assets[i];
            if (!asset->is_loaded) return;
            
            time_t new_modified = get_file_modified(asset->file_path);
            if (new_modified != asset->last_modified) {
                // Reload
                asset_load(mgr, name);
            }
            return;
        }
    }
}
