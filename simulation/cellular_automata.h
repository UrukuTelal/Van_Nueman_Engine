#ifndef CELLULAR_AUTOMATA_H
#define CELLULAR_AUTOMATA_H

#include <cstddef>
#include <cstdint>
#include <vector>
#include "../include/Entity.h"
#include "../biology/organism_types.h"

static constexpr size_t MAX_CA_GRID_DIM = 64;

struct CAGridSoA {
    struct Flags {
        uint8_t alive : 1;
        uint8_t in_dream : 1;
        uint8_t in_shadow : 1;
        uint8_t decaying : 1;
        uint8_t reproducing : 1;
        uint8_t : 3;
    };

    size_t width = 0;
    size_t height = 0;
    size_t depth = 0;
    size_t total_cells = 0;

    float* energy = nullptr;
    float* matter = nullptr;
    float* velocity_x = nullptr;
    float* velocity_y = nullptr;
    float* velocity_z = nullptr;
    uint8_t* organism_type = nullptr;
    uint8_t* life_state = nullptr;
    uint8_t* shadow_state = nullptr;
    uint8_t* dream_state = nullptr;
    uint32_t* entity_id = nullptr;
    Flags* flags = nullptr;
};

struct PillarGridSoA {
    size_t total_cells = 0;

    float* alignment = nullptr;
    float* resonance = nullptr;
    float* stability = nullptr;
    float* entropy = nullptr;
    float* coherence = nullptr;
    float* flux = nullptr;
    float* manifestation = nullptr;
    float* dissolution = nullptr;
    float* pressure = nullptr;
};

struct FluxData {
    float* energy_flux_x = nullptr;
    float* energy_flux_y = nullptr;
    float* energy_flux_z = nullptr;
    float* matter_flux_x = nullptr;
    float* matter_flux_y = nullptr;
    float* matter_flux_z = nullptr;
    float* net_energy_change = nullptr;
    float* net_matter_change = nullptr;
};

inline size_t get_cell_index(const CAGridSoA& grid, size_t x, size_t y, size_t z = 0) {
    return (z * grid.height + y) * grid.width + x;
}

inline void get_cell_coords(const CAGridSoA& grid, size_t index, size_t& x, size_t& y, size_t& z) {
    z = index / (grid.width * grid.height);
    size_t remainder = index % (grid.width * grid.height);
    y = remainder / grid.width;
    x = remainder % grid.width;
}

bool ca_grid_allocate(CAGridSoA& grid, size_t width, size_t height, size_t depth);
void ca_grid_deallocate(CAGridSoA& grid);
bool pillar_grid_allocate(PillarGridSoA& pillars, size_t width, size_t height, size_t depth);
void pillar_grid_deallocate(PillarGridSoA& pillars);
bool flux_data_allocate(FluxData& flux, size_t cell_count);
void flux_data_deallocate(FluxData& flux);

struct CellState {
    Van_Nueman::OrganismBase organism;
    float temp;
    float depth;
    float flux;
    uint32_t entity_id;
    bool alive;
};

class CellularAutomata {
public:
    CellularAutomata(uint32_t width, uint32_t height);
    CellState* get_cell(uint32_t x, uint32_t y);
    void update_cell(uint32_t x, uint32_t y, const CellState& state);
    void step(float dt);
private:
    uint32_t width_, height_;
    std::vector<CellState> cells_;
};

#endif
