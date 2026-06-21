#include "chunk.h"
#include <fstream>
#include <iostream>
#include <cmath>

Chunk::Chunk(int chunk_x, int chunk_y, int chunk_z)
    : chunk_x_(chunk_x), chunk_y_(chunk_y), chunk_z_(chunk_z), dirty_(true), voxels_(VOXEL_COUNT) {
    svo_chunk_.x = chunk_x;
    svo_chunk_.y = chunk_y;
    svo_chunk_.z = chunk_z;
    svo_chunk_.lod_level = 0;
    svo_chunk_.node_count = 0;
    svo_chunk_.dirty = 1;
    svo_chunk_.nodes_offset = 0;
}

size_t Chunk::voxel_index(int x, int y, int z) {
    return static_cast<size_t>(x) * CHUNK_SIZE * CHUNK_SIZE + 
           static_cast<size_t>(y) * CHUNK_SIZE + 
           static_cast<size_t>(z);
}

bool Chunk::is_valid_pos(int x, int y, int z) {
    return x >= 0 && x < CHUNK_SIZE && 
           y >= 0 && y < CHUNK_SIZE && 
           z >= 0 && z < CHUNK_SIZE;
}

Voxel& Chunk::get_voxel(int x, int y, int z) {
    return voxels_[voxel_index(x, y, z)];
}

const Voxel& Chunk::get_voxel(int x, int y, int z) const {
    return voxels_[voxel_index(x, y, z)];
}

void Chunk::set_voxel(int x, int y, int z, const Voxel& voxel) {
    if (!is_valid_pos(x, y, z)) return;
    voxels_[voxel_index(x, y, z)] = voxel;
    dirty_ = true;
    svo_chunk_.dirty = 1;
}

uint64_t Chunk::bcc_key_for(uint32_t x, uint32_t y, uint32_t z) const {
    BCCCoord c;
    c.i = static_cast<int32_t>(x);
    c.j = static_cast<int32_t>(y);
    c.k = static_cast<int32_t>(z);
    c.l = (static_cast<int32_t>(x) + static_cast<int32_t>(y) + static_cast<int32_t>(z)) % 2;
    return bcc_bitmask_key(c, CHUNK_BITS);
}

VoxelCell* Chunk::get_active_cell(uint64_t key) {
    auto it = active_cells_.find(key);
    return (it != active_cells_.end()) ? &it->second : nullptr;
}

VoxelCell* Chunk::promote_to_active(uint32_t x, uint32_t y, uint32_t z, const YieldMatrix& material) {
    if (!is_valid_pos(static_cast<int>(x), static_cast<int>(y), static_cast<int>(z)))
        return nullptr;
    uint64_t key = bcc_key_for(x, y, z);
    if (active_cells_.count(key))
        return &active_cells_[key];
    BCCCoord bc = bcc_from_cartesian(static_cast<int32_t>(x), static_cast<int32_t>(y), static_cast<int32_t>(z));
    VoxelCell& cell = active_cells_[key];
    cell.init(bc, vn::fp20_t(1.0f), static_cast<uint32_t>(active_cells_.size() - 1), material);
    voxels_[voxel_index(static_cast<int>(x), static_cast<int>(y), static_cast<int>(z))].material = 2;
    dirty_ = true;
    return &cell;
}

void Chunk::remove_active_cell(uint64_t key) {
    auto it = active_cells_.find(key);
    if (it != active_cells_.end()) {
        active_cells_.erase(it);
        dirty_ = true;
    }
}

void Chunk::update_svo() {
    if (svo_chunk_.dirty) {
        svo_chunk_.dirty = 0;
        dirty_ = false;
    }
}

bool Chunk::save_to_file(const std::string& path) const {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return false;
    
    uint32_t header[4] = {static_cast<uint32_t>(chunk_x_), static_cast<uint32_t>(chunk_y_), 
                          static_cast<uint32_t>(chunk_z_), static_cast<uint32_t>(VOXEL_COUNT)};
    file.write(reinterpret_cast<const char*>(header), sizeof(header));
    file.write(reinterpret_cast<const char*>(voxels_.data()), 
                voxels_.size() * sizeof(Voxel));
    
    return true;
}

bool Chunk::load_from_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;
    
    uint32_t header[4];
    file.read(reinterpret_cast<char*>(header), sizeof(header));
    if (header[0] != chunk_x_ || header[1] != chunk_y_ || 
        header[2] != chunk_z_ || header[3] != VOXEL_COUNT)
        return false;
    
    file.read(reinterpret_cast<char*>(voxels_.data()), 
               voxels_.size() * sizeof(Voxel));
    dirty_ = true;
    svo_chunk_.dirty = 1;
    return true;
}

void Chunk::generate_test_chunk() {
    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int y = 0; y < CHUNK_SIZE; y++) {
            for (int z = 0; z < CHUNK_SIZE; z++) {
                float dx = x - CHUNK_SIZE/2;
                float dy = y - CHUNK_SIZE/2;
                float dz = z - CHUNK_SIZE/2;
                float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
                
                if (dist < CHUNK_SIZE/3) {
                    Voxel v;
                    v.material = 1;
                    v.r = static_cast<uint8_t>((x * 255) / CHUNK_SIZE);
                    v.g = static_cast<uint8_t>((y * 255) / CHUNK_SIZE);
                    v.b = static_cast<uint8_t>((z * 255) / CHUNK_SIZE);
                    v.a = 255;
                    set_voxel(x, y, z, v);
                }
            }
        }
    }
}
