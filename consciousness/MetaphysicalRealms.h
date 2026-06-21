#pragma once

#include "../agents/cognition.h"
#include "../voxel/BCCIndex.h"
#include "../voxel/InteriorSubdivision.h"
#include "../include/Entity.h"
#include "../audio/wht_scaled.h"
#include <vector>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <unordered_map>

enum class MetaphysicalRealm : uint8_t {
    Physical = 0,
    Dream = 1,
    Bicameral = 2,
    Metacognitive = 3,
    Paradigmatic = 4,
    Aether = 5,
    Abyss = 6,
    BlackHoleUniverse = 7,
    MultiversalCluster = 8,
    All = 9
};

inline const char* metaphysical_realm_name(MetaphysicalRealm r) {
    switch (r) {
        case MetaphysicalRealm::Physical: return "Physical";
        case MetaphysicalRealm::Dream: return "Dream";
        case MetaphysicalRealm::Bicameral: return "Bicameral";
        case MetaphysicalRealm::Metacognitive: return "Metacognitive";
        case MetaphysicalRealm::Paradigmatic: return "Paradigmatic";
        case MetaphysicalRealm::Aether: return "Aether";
        case MetaphysicalRealm::Abyss: return "Abyss";
        case MetaphysicalRealm::BlackHoleUniverse: return "BlackHoleUniverse";
        case MetaphysicalRealm::MultiversalCluster: return "MultiversalCluster";
        case MetaphysicalRealm::All: return "All";
        default: return "Unknown";
    }
}

inline MetaphysicalRealm realm_for_depth(uint32_t depth) {
    if (depth <= 2) return MetaphysicalRealm::Physical;
    if (depth <= 4) return MetaphysicalRealm::Dream;
    if (depth == 5) return MetaphysicalRealm::Bicameral;
    if (depth == 6) return MetaphysicalRealm::Metacognitive;
    if (depth == 7) return MetaphysicalRealm::Paradigmatic;
    if (depth <= 9) return MetaphysicalRealm::Aether;
    if (depth <= 11) return MetaphysicalRealm::Abyss;
    if (depth <= 13) return MetaphysicalRealm::BlackHoleUniverse;
    if (depth == 14) return MetaphysicalRealm::MultiversalCluster;
    return MetaphysicalRealm::All;
}

struct RealmGeometry {
    MetaphysicalRealm realm;
    uint32_t depth_layer;
    BCCCoord bcc_center;
    float bcc_radius;
    float base_pillar_field[NumPillars];

    RealmGeometry() : realm(MetaphysicalRealm::Physical), depth_layer(0),
                      bcc_center({0,0,0,0}), bcc_radius(1.0f) {
        for (int i = 0; i < NumPillars; i++) base_pillar_field[i] = 0.5f;
    }
};

struct NestedUniverse {
    BCCCoord origin;
    PillarVector pillar_state;
    float scale;

    NestedUniverse() : scale(1.0f) {
        pillar_state.fill(vn::fp20_t(0.5f));
    }
};

struct MultiversalPocket {
    BCCCoord origin;
    float depth_offset;
    float pillar_bias[NumPillars];

    MultiversalPocket() : depth_offset(0) {
        for (int i = 0; i < NumPillars; i++) pillar_bias[i] = 0.5f;
    }
};

class RealmNavigator {
public:
    RealmNavigator() {}

    RealmGeometry get_realm_geometry(uint32_t depth) {
        RealmGeometry geom;
        geom.realm = realm_for_depth(depth);
        geom.depth_layer = depth;
        geom.bcc_center = {0, 0, 0, 0};
        geom.bcc_radius = 1.0f + static_cast<float>(depth) * 0.5f;

        for (int p = 0; p < NumPillars; p++)
            geom.base_pillar_field[p] = 0.5f;

        switch (geom.realm) {
            case MetaphysicalRealm::Physical:
                geom.base_pillar_field[Integrity] = 0.8f;
                geom.base_pillar_field[Cohesion] = 0.7f;
                geom.base_pillar_field[Flux] = 0.3f;
                break;
            case MetaphysicalRealm::Dream:
                geom.base_pillar_field[Flux] = 0.8f;
                geom.base_pillar_field[Memory] = 0.7f;
                geom.base_pillar_field[Warmth] = 0.6f;
                geom.base_pillar_field[Integrity] = 0.3f;
                break;
            case MetaphysicalRealm::Bicameral:
                geom.base_pillar_field[Awareness] = 0.7f;
                geom.base_pillar_field[Relation] = 0.8f;
                geom.base_pillar_field[Distortion] = 0.4f;
                break;
            case MetaphysicalRealm::Metacognitive:
                geom.base_pillar_field[Awareness] = 0.9f;
                geom.base_pillar_field[Willpower] = 0.7f;
                geom.base_pillar_field[Memory] = 0.8f;
                geom.base_pillar_field[Distortion] = 0.6f;
                break;
            case MetaphysicalRealm::Paradigmatic:
                geom.base_pillar_field[Integrity] = 0.9f;
                geom.base_pillar_field[Presence] = 0.8f;
                geom.base_pillar_field[Depth] = 0.7f;
                geom.base_pillar_field[Flux] = 0.2f;
                break;
            case MetaphysicalRealm::Aether:
                geom.base_pillar_field[Force] = 0.9f;
                geom.base_pillar_field[Warmth] = 0.8f;
                geom.base_pillar_field[Presence] = 0.7f;
                geom.base_pillar_field[Harm] = 0.1f;
                break;
            case MetaphysicalRealm::Abyss:
                geom.base_pillar_field[Harm] = 0.9f;
                geom.base_pillar_field[Distortion] = 0.8f;
                geom.base_pillar_field[Depth] = 0.9f;
                geom.base_pillar_field[Warmth] = 0.1f;
                break;
            case MetaphysicalRealm::BlackHoleUniverse:
                geom.base_pillar_field[Depth] = 1.0f;
                geom.base_pillar_field[Cohesion] = 0.9f;
                geom.base_pillar_field[Integrity] = 0.9f;
                geom.base_pillar_field[Flux] = 0.1f;
                break;
            case MetaphysicalRealm::MultiversalCluster:
                geom.base_pillar_field[Relation] = 0.9f;
                geom.base_pillar_field[Attraction] = 0.8f;
                geom.base_pillar_field[Flux] = 0.7f;
                break;
            case MetaphysicalRealm::All:
                for (int p = 0; p < NumPillars; p++)
                    geom.base_pillar_field[p] = 1.0f;
                break;
        }

        return geom;
    }

    void navigate_realms(uint32_t from_depth, uint32_t to_depth,
                          PillarStateVector& traveler) {
        int diff = static_cast<int>(to_depth) - static_cast<int>(from_depth);
        if (diff == 0) return;

        int num_layers = abs(diff) + 1;
        int log2n = 0;
        int temp = 1;
        while (temp < num_layers) { temp <<= 1; log2n++; }

        vn::fp20_t data[32];
        int count = temp < 32 ? temp : 32;
        if (count < 2) return;

        for (int p = 0; p < NumPillars; p++) {
            for (int i = 0; i < count; i++) {
                float t = static_cast<float>(i) / (count - 1);
                float interp = traveler[p] * (1.0f - t) + 0.5f * t;
                data[i] = vn::fp20_t(interp);
            }

            fwht_fp(data, log2n, false);

            traveler[p] = data[count - 1];
        }
    }

    std::vector<NestedUniverse> enter_black_hole_universe(const BCCCoord& center,
                                                           const PillarStateVector& parent_state,
                                                           int subdivision_levels) {
        std::vector<NestedUniverse> universes;

        auto child_coords = bcc_interior_children(center, 1);
        if (subdivision_levels > 1) {
            for (const auto& child : child_coords) {
                auto sub = bcc_interior_fill_depth(child, subdivision_levels - 1);
                for (const auto& sub_coord : sub) {
                    NestedUniverse nu;
                    nu.origin = sub_coord;
                    for (int p = 0; p < NumPillars; p++) {
                        float inverted = 1.0f - parent_state[p];
                        float jitter = static_cast<float>(std::rand() % 200 - 100) / 1000.0f;
                        inverted += jitter;
                        if (inverted < 0.0f) inverted = 0.0f;
                        if (inverted > 1.0f) inverted = 1.0f;
                        nu.pillar_state[p] = vn::fp20_t(inverted);
                    }
                    nu.scale = 1.0f / (1 << subdivision_levels);
                    universes.push_back(nu);
                }
            }
        } else {
            for (const auto& child : child_coords) {
                NestedUniverse nu;
                nu.origin = child;
                for (int p = 0; p < NumPillars; p++) {
                    float inverted = 1.0f - parent_state[p];
                    nu.pillar_state[p] = vn::fp20_t(inverted);
                }
                nu.scale = 0.5f;
                universes.push_back(nu);
            }
        }

        return universes;
    }

    std::vector<MultiversalPocket> enter_multiversal_cluster(const BCCCoord& center) {
        std::vector<MultiversalPocket> pockets;
        int num_pockets = 3 + (std::rand() % 5);

        for (int i = 0; i < num_pockets; i++) {
            MultiversalPocket pocket;
            pocket.origin.i = center.i + (std::rand() % 20 - 10);
            pocket.origin.j = center.j + (std::rand() % 20 - 10);
            pocket.origin.k = center.k + (std::rand() % 20 - 10);
            pocket.origin.l = (pocket.origin.i + pocket.origin.j + pocket.origin.k) & 1;
            pocket.depth_offset = static_cast<float>(std::rand() % 7 - 3);

            for (int p = 0; p < NumPillars; p++) {
                pocket.pillar_bias[p] = static_cast<float>(std::rand() % 256) / 255.0f;
            }

            pockets.push_back(pocket);
        }

        return pockets;
    }

    void enter_all_state(PillarStateVector& entity_pillars,
                          PillarStateVector& prior_state) {
        for (int p = 0; p < NumPillars; p++)
            prior_state[p] = entity_pillars[p];

        for (int p = 0; p < NumPillars; p++)
            entity_pillars[p] = vn::fp20_t(1.0f);
    }

    void exit_all_state(PillarStateVector& entity_pillars,
                         const PillarStateVector& prior_state) {
        for (int p = 0; p < NumPillars; p++) {
            float prior = prior_state[p];
            float current = entity_pillars[p];
            float relaxed = prior + (current - prior) * 0.1f;
            if (relaxed > 1.0f) relaxed = 1.0f;
            entity_pillars[p] = vn::fp20_t(relaxed);
        }

        float memory_imprint = (entity_pillars[Memory] + prior_state[Memory]) * 0.5f + 0.1f;
        if (memory_imprint > 1.0f) memory_imprint = 1.0f;
        entity_pillars[Memory] = vn::fp20_t(memory_imprint);
    }
};
