#pragma once

#include "../vn/ScaledInt.h"
#include <cstdint>
#include <vector>
#include <cstring>

constexpr uint32_t INFLUENCE_GRID_SIZE = 64;
constexpr uint32_t INFLUENCE_COEFFS = 32;

struct RegionInfluence {
    int32_t chunk_x, chunk_y, chunk_z;
    vn::fp20_t wht_coefficients[INFLUENCE_COEFFS];
    uint32_t unresolved_count;
    vn::fp20_t total_energy;
};

class InfluenceBuffer {
public:
    InfluenceBuffer() : regions_(INFLUENCE_GRID_SIZE * INFLUENCE_GRID_SIZE * INFLUENCE_GRID_SIZE) {}

    void accumulate(int32_t cx, int32_t cy, int32_t cz,
                    const vn::fp20_t* entity_pillars,
                    vn::fp20_t weight)
    {
        uint32_t idx = hash_coord(cx, cy, cz);
        RegionInfluence& r = regions_[idx];
        r.chunk_x = cx; r.chunk_y = cy; r.chunk_z = cz;
        for (int i = 0; i < INFLUENCE_COEFFS && i < 16; i++) {
            r.wht_coefficients[i] = r.wht_coefficients[i] + entity_pillars[i] * weight;
        }
        r.unresolved_count++;
        r.total_energy = r.total_energy + weight;
    }

    void subtract(int32_t cx, int32_t cy, int32_t cz,
                  const vn::fp20_t* entity_pillars,
                  vn::fp20_t weight)
    {
        uint32_t idx = hash_coord(cx, cy, cz);
        RegionInfluence& r = regions_[idx];
        for (int i = 0; i < INFLUENCE_COEFFS && i < 16; i++) {
            r.wht_coefficients[i] = r.wht_coefficients[i] - entity_pillars[i] * weight;
        }
        if (r.unresolved_count > 0) r.unresolved_count--;
        r.total_energy = r.total_energy - weight;
    }

    const RegionInfluence* get_region(int32_t cx, int32_t cy, int32_t cz) const {
        uint32_t idx = hash_coord(cx, cy, cz);
        if (regions_[idx].chunk_x == cx &&
            regions_[idx].chunk_y == cy &&
            regions_[idx].chunk_z == cz) {
            return &regions_[idx];
        }
        return nullptr;
    }

    void clear() {
        for (auto& r : regions_) {
            r.unresolved_count = 0;
            r.total_energy = vn::fp20_t(0);
            for (int i = 0; i < INFLUENCE_COEFFS; i++) {
                r.wht_coefficients[i] = vn::fp20_t(0);
            }
        }
    }

private:
    std::vector<RegionInfluence> regions_;

    static uint32_t hash_coord(int32_t x, int32_t y, int32_t z) {
        uint32_t h = (x * 73856093) ^ (y * 19349663) ^ (z * 83492791);
        return h % (INFLUENCE_GRID_SIZE * INFLUENCE_GRID_SIZE * INFLUENCE_GRID_SIZE);
    }
};
