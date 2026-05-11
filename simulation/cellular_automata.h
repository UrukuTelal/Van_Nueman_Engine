#ifndef CELLULAR_AUTOMATA_H
#define CELLULAR_AUTOMATA_H

#include <cstdint>
#include <vector>
#include "../include/Entity.h"
#include "../biology/organism_types.h"

struct CellState {
    OrganismBase organism;
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
