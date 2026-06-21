#ifndef VAN_NUEMAN_SCENE_CHUNK_H
#define VAN_NUEMAN_SCENE_CHUNK_H

#include <cstdint>

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include "../include/Entity.h"
#include "../kernels/voxel_svo.h"
#include "../voxel/VoxelCell.h"

constexpr int CHUNK_SIZE = 64;
constexpr int CHUNK_BITS = 6;
constexpr int VOXEL_COUNT = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;

struct Voxel {
    uint8_t material;  // 0=empty, 1=cube, 2=octahedron, 3=resource, etc.
    uint8_t r, g, b, a;
};

constexpr uint8_t MATERIAL_RESOURCE = 3;

class Chunk {
public:
    Chunk(int chunk_x, int chunk_y, int chunk_z);
    ~Chunk() = default;
    
    // Voxel access
    Voxel& get_voxel(int x, int y, int z);
    const Voxel& get_voxel(int x, int y, int z) const;
    void set_voxel(int x, int y, int z, const Voxel& voxel);
    
    // Check if position is valid
    static bool is_valid_pos(int x, int y, int z);
    
    // Chunk coordinates
    int get_chunk_x() const { return chunk_x_; }
    int get_chunk_y() const { return chunk_y_; }
    int get_chunk_z() const { return chunk_z_; }
    
    // Dirty flag for GPU updates
    bool is_dirty() const { return dirty_; }
    void set_dirty(bool dirty) { dirty_ = dirty; }
    
    // SVO integration
    SVO_Chunk* get_svo_chunk() { return &svo_chunk_; }
    void update_svo();
    
    // ── Active Cell (Truncated Octahedron) Management ─────────
    VoxelCell* get_active_cell(uint64_t bcc_key);
    VoxelCell* promote_to_active(uint32_t x, uint32_t y, uint32_t z, const YieldMatrix& material);
    void remove_active_cell(uint64_t bcc_key);
    uint32_t active_cell_count() const { return static_cast<uint32_t>(active_cells_.size()); }
    
    // BCC helpers
    uint64_t bcc_key_for(uint32_t x, uint32_t y, uint32_t z) const;
    
    // Active cell iteration (const access only)
    const std::unordered_map<uint64_t, VoxelCell>& get_active_cells() const { return active_cells_; }
    
    // Serialization
    bool save_to_file(const std::string& path) const;
    bool load_from_file(const std::string& path);
    
    // Generate test data
    void generate_test_chunk();
    
private:
    int chunk_x_, chunk_y_, chunk_z_;
    std::vector<Voxel> voxels_;
    bool dirty_;
    SVO_Chunk svo_chunk_;
    std::unordered_map<uint64_t, VoxelCell> active_cells_;
    
    // Helper
    static size_t voxel_index(int x, int y, int z);
};

#endif // VAN_NUEMAN_SCENE_CHUNK_H
