#ifndef VAN_NUEMAN_SCENE_CHUNK_H
#define VAN_NUEMAN_SCENE_CHUNK_H

#include <cstdint>

#include <string>
#include <vector>
#include <memory>
#include "../include/Entity.h"
#include "../kernels/voxel_svo.h"

constexpr int CHUNK_SIZE = 64;
constexpr int VOXEL_COUNT = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;

struct Voxel {
    uint8_t material;  // 0=empty, 1=cube, 2=octahedron, etc.
    uint8_t r, g, b, a;
};

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
    
    // Helper
    static size_t voxel_index(int x, int y, int z);
};

#endif // VAN_NUEMAN_SCENE_CHUNK_H
