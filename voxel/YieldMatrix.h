#pragma once

#include "../vn/ScaledInt.h"
#include "../include/Entity.h"
#include <cstdint>

static constexpr uint32_t YIELD_VERTICES = 24;

struct YieldMatrix {
    vn::fp20_t pillar_to_vertex[NumPillars][YIELD_VERTICES];

    YieldMatrix() {
        for (uint32_t p = 0; p < NumPillars; p++)
            for (uint32_t v = 0; v < YIELD_VERTICES; v++)
                pillar_to_vertex[p][v] = vn::fp20_t(0.5f);
    }

    static YieldMatrix default_rock() {
        YieldMatrix m;
        for (uint32_t v = 0; v < YIELD_VERTICES; v++) {
            m.pillar_to_vertex[Integrity][v] = vn::fp20_t(0.8f);
            m.pillar_to_vertex[Cohesion][v] = vn::fp20_t(0.7f);
            m.pillar_to_vertex[Harm][v] = vn::fp20_t(0.3f);
        }
        return m;
    }

    static YieldMatrix default_organic() {
        YieldMatrix m;
        for (uint32_t v = 0; v < YIELD_VERTICES; v++) {
            m.pillar_to_vertex[Integrity][v] = vn::fp20_t(0.4f);
            m.pillar_to_vertex[Cohesion][v] = vn::fp20_t(0.5f);
            m.pillar_to_vertex[Harm][v] = vn::fp20_t(0.6f);
            m.pillar_to_vertex[Flux][v] = vn::fp20_t(0.7f);
        }
        return m;
    }

    enum CellType : uint32_t {
        CELL_STEM = 0,
        CELL_MUSCLE = 1,
        CELL_NERVE = 2,
        CELL_BONE = 3,
        CELL_SKIN = 4,
        CELL_BLOOD = 5,
        CELL_GLAND = 6,
        CELL_GUT = 7,
        CELL_COUNT = 8
    };

    static YieldMatrix default_stem() {
        return YieldMatrix{};
    }

    static YieldMatrix default_muscle() {
        YieldMatrix m;
        for (uint32_t v = 0; v < YIELD_VERTICES; v++) {
            m.pillar_to_vertex[Force][v] = vn::fp20_t(0.8f);
            m.pillar_to_vertex[Flux][v] = vn::fp20_t(0.8f);
            m.pillar_to_vertex[Warmth][v] = vn::fp20_t(0.6f);
            m.pillar_to_vertex[Harm][v] = vn::fp20_t(0.2f);
        }
        return m;
    }

    static YieldMatrix default_nerve() {
        YieldMatrix m;
        for (uint32_t v = 0; v < YIELD_VERTICES; v++) {
            m.pillar_to_vertex[Awareness][v] = vn::fp20_t(0.9f);
            m.pillar_to_vertex[Memory][v] = vn::fp20_t(0.8f);
            m.pillar_to_vertex[Attraction][v] = vn::fp20_t(0.5f);
            m.pillar_to_vertex[Harm][v] = vn::fp20_t(0.3f);
            m.pillar_to_vertex[Force][v] = vn::fp20_t(0.3f);
            m.pillar_to_vertex[Distortion][v] = vn::fp20_t(0.4f);
        }
        return m;
    }

    static YieldMatrix default_bone() {
        YieldMatrix m;
        for (uint32_t v = 0; v < YIELD_VERTICES; v++) {
            m.pillar_to_vertex[Integrity][v] = vn::fp20_t(0.9f);
            m.pillar_to_vertex[Resistance][v] = vn::fp20_t(0.8f);
            m.pillar_to_vertex[Cohesion][v] = vn::fp20_t(0.7f);
            m.pillar_to_vertex[Harm][v] = vn::fp20_t(0.1f);
            m.pillar_to_vertex[Presence][v] = vn::fp20_t(0.6f);
        }
        return m;
    }

    static YieldMatrix default_skin() {
        YieldMatrix m;
        for (uint32_t v = 0; v < YIELD_VERTICES; v++) {
            m.pillar_to_vertex[Resistance][v] = vn::fp20_t(0.8f);
            m.pillar_to_vertex[Presence][v] = vn::fp20_t(0.7f);
            m.pillar_to_vertex[Integrity][v] = vn::fp20_t(0.6f);
            m.pillar_to_vertex[Cohesion][v] = vn::fp20_t(0.5f);
            m.pillar_to_vertex[Harm][v] = vn::fp20_t(0.4f);
            m.pillar_to_vertex[Distortion][v] = vn::fp20_t(0.3f);
        }
        return m;
    }

    static YieldMatrix default_blood() {
        YieldMatrix m;
        for (uint32_t v = 0; v < YIELD_VERTICES; v++) {
            m.pillar_to_vertex[Warmth][v] = vn::fp20_t(0.9f);
            m.pillar_to_vertex[Flux][v] = vn::fp20_t(0.7f);
            m.pillar_to_vertex[Cohesion][v] = vn::fp20_t(0.6f);
            m.pillar_to_vertex[Attraction][v] = vn::fp20_t(0.5f);
            m.pillar_to_vertex[Influence][v] = vn::fp20_t(0.5f);
        }
        return m;
    }

    static YieldMatrix default_gland() {
        YieldMatrix m;
        for (uint32_t v = 0; v < YIELD_VERTICES; v++) {
            m.pillar_to_vertex[Distortion][v] = vn::fp20_t(0.7f);
            m.pillar_to_vertex[Attraction][v] = vn::fp20_t(0.8f);
            m.pillar_to_vertex[Warmth][v] = vn::fp20_t(0.5f);
            m.pillar_to_vertex[Memory][v] = vn::fp20_t(0.4f);
            m.pillar_to_vertex[Awareness][v] = vn::fp20_t(0.4f);
        }
        return m;
    }

    static YieldMatrix default_gut() {
        YieldMatrix m;
        for (uint32_t v = 0; v < YIELD_VERTICES; v++) {
            m.pillar_to_vertex[Attraction][v] = vn::fp20_t(0.8f);
            m.pillar_to_vertex[Cohesion][v] = vn::fp20_t(0.7f);
            m.pillar_to_vertex[Warmth][v] = vn::fp20_t(0.5f);
            m.pillar_to_vertex[Flux][v] = vn::fp20_t(0.4f);
            m.pillar_to_vertex[Integrity][v] = vn::fp20_t(0.4f);
        }
        return m;
    }

    static YieldMatrix from_cell_type(CellType type) {
        switch (type) {
        case CELL_STEM:   return default_stem();
        case CELL_MUSCLE: return default_muscle();
        case CELL_NERVE:  return default_nerve();
        case CELL_BONE:   return default_bone();
        case CELL_SKIN:   return default_skin();
        case CELL_BLOOD:  return default_blood();
        case CELL_GLAND:  return default_gland();
        case CELL_GUT:    return default_gut();
        default:          return default_stem();
        }
    }

    vn::fp20_t compute_yield_threshold(uint32_t vertex_idx, const vn::fp20_t pillar_forces[NumPillars]) const {
        vn::fp20_t total(0);
        for (uint32_t p = 0; p < NumPillars; p++) {
            total = total + pillar_forces[p] * pillar_to_vertex[p][vertex_idx];
        }
        return total;
    }
};
