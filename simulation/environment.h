#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include <cstdint>
#include <vector>
#include <cmath>

// Cell state for emergent ALife
struct CellState {
    float temp;        // temperature (Celsius)
    float depth;       // depth (km)  
    float flux;     // flux level (0-1)
    uint32_t population;
    uint32_t organism_base;  // ORGANISM_CARBON/SILICON/HYBRID
    
    CellState() : temp(25.0f), depth(0.0f), flux(0.1f), population(0), organism_base(0) {}
};

// Environment grid for multiscale CA
struct EnvironmentGrid {
    uint32_t width, height;
    std::vector<CellState> cells;
    
    void init(uint32_t w, uint32_t h) {
        width = w;
        height = h;
        cells.resize(w * h);
    }
    
    CellState& get_cell(uint32_t x, uint32_t y) {
        return cells[y * width + x];
    }
    
    // Evolution step - apply fractal CA rules
    void step() {
        std::vector<CellState> next = cells;
        
        for (uint32_t y = 1; y < height - 1; y++) {
            for (uint32_t x = 1; x < width - 1; x++) {
                CellState& self = cells[y * width + x];
                CellState& n = next[y * width + x];
                
                // Average neighbors for flux/depth inheritance
                float flux_sum = 0.0f, depth_sum = 0.0f;
                uint32_t pop_sum = 0;
                
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        if (dx == 0 && dy == 0) continue;
                        CellState& nb = cells[(y + dy) * width + (x + dx)];
                        flux_sum += nb.flux;
                        depth_sum += nb.depth;
                        pop_sum += nb.population;
                    }
                }
                
                // Evolve with logarithmic feedback
                n.flux = 0.5f * self.flux + 0.125f * flux_sum / 8.0f;
                n.depth = 0.5f * self.depth + 0.125f * depth_sum / 8.0f;
                n.population = (self.population + pop_sum / 8) / 2;
            }
        }
        
        cells = next;
    }
    
    uint32_t count_organisms(uint32_t base) {
        uint32_t count = 0;
        for (auto& c : cells) {
            if (c.organism_base == base) count += c.population;
        }
        return count;
    }
};

#endif // ENVIRONMENT_H