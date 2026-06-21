#include "websocket_api.h"
#include <sstream>
#include <iostream>

WebSocketApi::WebSocketApi(SimulationTickLoop* simulation)
    : simulation_(simulation) {
}

bool WebSocketApi::start(int port) {
    // Placeholder: in real implementation, use uWebSockets or similar
    std::cout << "WebSocket API would start on port " << port << std::endl;
    std::cout << "Real-time streaming of simulation data" << std::endl;
    return true;
}

void WebSocketApi::broadcast_agent_update(int agent_id) {
    if (!simulation_) return;
    
    auto& ecs = simulation_->get_agent_ecs();
    auto idx = static_cast<vn::simulation::AgentECS::Index>(agent_id);
    if (!ecs.is_valid(idx) || !ecs.active(idx)) return;
    
    float x = ecs.x(idx);
    float y = ecs.y(idx);
    float z = ecs.z(idx);
    auto pillarState = ecs.get_pillars(idx);
    PillarVector pillars;
    for (int i = 0; i < NumPillars; i++) pillars[i] = pillarState[i];
    std::string json = build_agent_json(agent_id, x, y, z, pillars);
    std::cout << "Broadcast agent update: " << json << std::endl;
}

void WebSocketApi::broadcast_pillar_update(const std::array<float, NumPillars>& pillars) {
    PillarVector pv;
    for (int i = 0; i < NumPillars; i++) pv[i] = vn::fp20_t(pillars[i]);
    std::string json = build_pillar_json(pv);
    std::cout << "Broadcast pillar update: " << json << std::endl;
}

void WebSocketApi::broadcast_tick() {
    if (!simulation_) return;
    
    uint32_t tick = simulation_->get_world_state().tick_count;
    std::string json = build_tick_json(tick);
    std::cout << "Broadcast tick: " << json << std::endl;
}

void WebSocketApi::handle_message(const std::string& client_id, const WSMessage& msg) {
    if (msg.type != WSFrameType::TEXT) return;
    
    std::string message(msg.data.begin(), msg.data.end());
    std::cout << "Received from " << client_id << ": " << message << std::endl;
    
    // Parse and handle commands
    if (message.find("\"command\":\"tick\"") != std::string::npos) {
        broadcast_tick();
    }
}

std::string WebSocketApi::build_agent_json(int agent_id, float x, float y, float z,
                                           const PillarVector& pillars) {
    std::ostringstream json;
    json << "{";
    json << "\"type\":\"agent_update\",";
    json << "\"id\":" << agent_id << ",";
    json << "\"position\":{\"x\":" << x << ",\"y\":" << y << ",\"z\":" << z << "},";
    json << "\"pillars\":";
    
    json << "{";
    const char* names[] = {"awareness", "willpower", "force", "influence", "resistance",
                            "integrity", "cohesion", "relation", "presence", "warmth",
                            "memory", "attraction", "harm", "distortion", "flux", "depth"};
    for (int i = 0; i < NumPillars; i++) {
        if (i > 0) json << ",";
        json << "\"" << names[i] << "\":" << pillars[i];
    }
    json << "}";
    
    json << "}";
    return json.str();
}

std::string WebSocketApi::build_pillar_json(const PillarVector& pillars) {
    std::ostringstream json;
    json << "{";
    json << "\"type\":\"pillar_update\",";
    json << "\"pillars\":";
    
    json << "{";
    const char* names[] = {"awareness", "willpower", "force", "influence", "resistance",
                            "integrity", "cohesion", "relation", "presence", "warmth",
                            "memory", "attraction", "harm", "distortion", "flux", "depth"};
    for (int i = 0; i < NumPillars; i++) {
        if (i > 0) json << ",";
        json << "\"" << names[i] << "\":" << pillars[i];
    }
    json << "}";
    
    json << "}";
    return json.str();
}

std::string WebSocketApi::build_tick_json(uint32_t tick_count) {
    std::ostringstream json;
    json << "{";
    json << "\"type\":\"tick\",";
    json << "\"tick\":" << tick_count;
    json << "}";
    return json.str();
}
