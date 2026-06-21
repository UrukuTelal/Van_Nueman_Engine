#pragma once

#ifndef VAN_NUEMAN_SCALE_ROUTER_H
#define VAN_NUEMAN_SCALE_ROUTER_H

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <functional>
#include <cmath>
#include "ScaleExponent.h"
#include "../include/Entity.h"
#include "PillarEnum.h"
#include "../include/simulation/AgentECS.h"
#include "BlochMath.h"

constexpr float SCALE_ROUTER_PHI = 1.6180339887498948482045868343656f;
constexpr float SCALE_COUPLING_BASE = 0.1f;

struct ScaleRoute {
    ScaleExponent source_scale;
    ScaleExponent target_scale;
    float coupling_coefficient;
    float attention_attenuation;
    float delta;
};

struct ScaleRoutingTable {
    std::vector<ScaleRoute> routes;
    float total_coherence = 0.0f;
    int active_scale_count = 0;
};

class ScaleRouter {
public:
    ScaleRouter() = default;
    ~ScaleRouter() = default;

    void set_entity_scale(vn::simulation::AgentECS::Index idx, ScaleExponent scale);
    ScaleExponent get_entity_scale(vn::simulation::AgentECS::Index idx) const;

    void build_routing_table();

    ScaleRoute route(ScaleExponent source, ScaleExponent target) const;

    void route_pillars(vn::simulation::AgentECS& ecs,
                       ScaleExponent source_scale,
                       float dt);

    void cross_route_entities(vn::simulation::AgentECS& ecs,
                               ScaleExponent source_scale,
                               ScaleExponent target_scale,
                               float dt);

    void route_all(vn::simulation::AgentECS& ecs, float dt);

    const ScaleRoutingTable& get_routing_table() const { return routing_table_; }

    static float scale_coupling(ScaleExponent delta);
    static float scale_interaction_radius(ScaleExponent scale);

private:
    ScaleRoutingTable routing_table_;
    std::unordered_map<ScaleExponent, std::vector<vn::simulation::AgentECS::Index>> scale_groups_;
    std::unordered_map<vn::simulation::AgentECS::Index, ScaleExponent> entity_scales_;
};

#endif
