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
    
    const auto& agents = simulation_->get_agents();
    if (agent_id < 0 || agent_id >= static_cast<int>(agents.size())) return;
    
    const auto& agent = agents[agent_id];
    if (!agent.active) return;
    
    std::string json = build_agent_json(agent_id, agent.x, agent.y, agent.z,
                                        agent.cognition->get_pillars());
    // Send to all connected clients
    std::cout << "Broadcast agent update: " << json << std::endl;
}

void WebSocketApi::broadcast_pillar_update(const std::array<float, NUM_PILLARS>& pillars) {
    std::string json = build_pillar_json(pillars);
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
                                           const std::array<float, NUM_PILLARS>& pillars) {
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
    for (int i = 0; i < NUM_PILLARS; i++) {
        if (i > 0) json << ",";
        json << "\"" << names[i] << "\":" << pillars[i];
    }
    json << "}";
    
    json << "}";
    return json.str();
}

std::string WebSocketApi::build_pillar_json(const std::array<float, NUM_PILLARS>& pillars) {
    std::ostringstream json;
    json << "{";
    json << "\"type\":\"pillar_update\",";
    json << "\"pillars\":";
    
    json << "{";
    const char* names[] = {"awareness", "willpower", "force", "influence", "resistance",
                            "integrity", "cohesion", "relation", "presence", "warmth",
                            "memory", "attraction", "harm", "distortion", "flux", "depth"};
    for (int i = 0; i < NUM_PILLARS; i++) {
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
