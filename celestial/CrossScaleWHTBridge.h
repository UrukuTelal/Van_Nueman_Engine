#pragma once

#include <PillarEnum.h>
#include "UniversalExpansion.h"
#include "../audio/wht_scaled.h"
#include <cmath>

struct ScaleLayer {
    int scale_level;
    float scale_factor;
    vn::fp20_t pillar_state[NumPillars];
};

inline void cross_scale_wht_bridge(ScaleLayer* layers, int num_layers) {
    if (num_layers <= 0) return;

    int log2n = 0;
    int temp = num_layers;
    while (temp > 1) { temp >>= 1; log2n++; }
    if ((1 << log2n) != num_layers) return;

    for (int p = 0; p < NumPillars; p++) {
        vn::fp20_t data[32];
        int count = num_layers < 32 ? num_layers : 32;
        for (int i = 0; i < count; i++) {
            data[i] = layers[i].pillar_state[p];
        }

        fwht_fp(data, log2n, false);

        for (int i = 0; i < count; i++) {
            layers[i].pillar_state[p] = data[i];
        }
    }
}

inline void cross_scale_inverse_wht_bridge(ScaleLayer* layers, int num_layers) {
    if (num_layers <= 0) return;

    int log2n = 0;
    int temp = num_layers;
    while (temp > 1) { temp >>= 1; log2n++; }
    if ((1 << log2n) != num_layers) return;

    for (int p = 0; p < NumPillars; p++) {
        vn::fp20_t data[32];
        int count = num_layers < 32 ? num_layers : 32;
        for (int i = 0; i < count; i++) {
            data[i] = layers[i].pillar_state[p];
        }

        ifwht_fp(data, log2n, false);

        for (int i = 0; i < count; i++) {
            layers[i].pillar_state[p] = data[i];
        }
    }
}

struct Universe {
    CosmologicalConfig config;
    CosmicField field;
    ScaleLayer scale_layers[16];
    int num_scale_layers;
    float cosmic_time;

    Universe() : num_scale_layers(0), cosmic_time(0.0f) {}

    void init_scale_layers() {
        num_scale_layers = 8;
        for (int i = 0; i < num_scale_layers; i++) {
            scale_layers[i].scale_level = i + 1;
            scale_layers[i].scale_factor = powf(10.0f, static_cast<float>(i - 4));
            for (int p = 0; p < NumPillars; p++) {
                scale_layers[i].pillar_state[p] = vn::fp20_t(0.5f);
            }
        }
    }

    void tick(float dt) {
        cosmic_time += dt;
        cosmological_tick(field, config, dt);
        detect_large_scale_structure(field);

        cross_scale_wht_bridge(scale_layers, num_scale_layers);
        cross_scale_inverse_wht_bridge(scale_layers, num_scale_layers);
    }
};
