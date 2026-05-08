#include "rest_api.h"
#include <sstream>
#include <cstring>
#include <iostream>

RestApi::RestApi(SimulationTickLoop* simulation)
    : simulation_(simulation) {
}

bool RestApi::start(int port) {
    // Placeholder: in real implementation, use Crow or cpp-httplib
    std::cout << "REST API would start on port " << port << std::endl;
    std::cout << "Endpoints:" << std::endl;
    std::cout << "  GET  /api/world/state" << std::endl;
    std::cout << "  GET  /api/agents" << std::endl;
    std::cout << "  GET  /api/agents/{id}" << std::endl;
    std::cout << "  POST /api/simulation/tick" << std::endl;
    std::cout << "  GET  /api/chunks/{x}/{y}/{z}" << std::endl;
    return true;
}

HttpResponse RestApi::handle_request(const std::string& method,
                                      const std::string& path,
                                      const std::string& body) {
    // Simple routing
    if (method == "GET" && path == "/api/world/state") {
        return get_world_state();
    }
    
    if (method == "GET" && path.rfind("/api/agents/", 0) == 0) {
        // Extract agent ID
        size_t last_slash = path.rfind('/');
        try {
            int id = std::stoi(path.substr(last_slash + 1));
            return get_agent(id);
        } catch (...) {
            return {404, "text/plain", "Invalid agent ID"};
        }
    }
    
    if (method == "GET" && path == "/api/agents") {
        return get_agents_list();
    }
    
    if (method == "POST" && path == "/api/simulation/tick") {
        return post_tick(body);
    }
    
    if (method == "GET" && path.rfind("/api/chunks/", 0) == 0) {
        // Parse x/y/z from path
        // Simplified - would use regex in real implementation
        return {501, "text/plain", "Chunk endpoint not fully implemented"};
    }
    
    return {404, "text/plain", "Not found"};
}

HttpResponse RestApi::get_world_state() {
    if (!simulation_) {
        return {500, "application/json", "{\"error\":\"No simulation\"}"};
    }
    
    const auto& state = simulation_->get_world_state();
    std::ostringstream json;
    json << "{";
    json << "\"temperature\":" << state.temperature << ",";
    json << "\"hazard_level\":" << state.hazard_level << ",";
    json << "\"resource_density\":" << state.resource_density << ",";
    json << "\"tick_count\":" << state.tick_count;
    json << "}";
    
    return {200, "application/json", json.str()};
}

HttpResponse RestApi::get_agent(int agent_id) {
    if (!simulation_) {
        return {500, "application/json", "{\"error\":\"No simulation\"}"};
    }
    
    const auto& agents = simulation_->get_agents();
    if (agent_id < 0 || agent_id >= static_cast<int>(agents.size()) || !agents[agent_id].active) {
        return {404, "application/json", "{\"error\":\"Agent not found\"}"};
    }
    
    const auto& agent = agents[agent_id];
    const auto& pillars = agent.cognition->get_pillars();
    
    std::ostringstream json;
    json << "{";
    json << "\"id\":" << agent.id << ",";
    json << "\"position\":{\"x\":" << agent.x << ",\"y\":" << agent.y << ",\"z\":" << agent.z << "},";
    json << "\"pillars\":" << pillar_vector_to_json(pillars);
    json << "}";
    
    return {200, "application/json", json.str()};
}

HttpResponse RestApi::get_agents_list() {
    if (!simulation_) {
        return {500, "application/json", "{\"error\":\"No simulation\"}"};
    }
    
    const auto& agents = simulation_->get_agents();
    std::ostringstream json;
    json << "{\"agents\":[";
    
    bool first = true;
    for (const auto& agent : agents) {
        if (!agent.active) continue;
        if (!first) json << ",";
        first = false;
        json << "{\"id\":" << agent.id << ",\"x\":" << agent.x << ",\"y\":" << agent.y << ",\"z\":" << agent.z << "}";
    }
    
    json << "]}";
    return {200, "application/json", json.str()};
}

HttpResponse RestApi::post_tick(const std::string& body) {
    if (!simulation_) {
        return {500, "application/json", "{\"error\":\"No simulation\"}"};
    }
    
    int ticks = 1;
    parse_json_int(body, "ticks", ticks);
    
    for (int i = 0; i < ticks; i++) {
        simulation_->tick(1.0f / 60.0f);
    }
    
    std::ostringstream json;
    json << "{\"ticks_processed\":" << ticks << ",\"tick_count\":" << simulation_->get_world_state().tick_count << "}";
    return {200, "application/json", json.str()};
}

std::string RestApi::pillar_vector_to_json(const std::array<float, NUM_PILLARS>& pillars) {
    std::ostringstream json;
    json << "{";
    const char* names[] = {"awareness", "willpower", "force", "influence", "resistance",
                            "integrity", "cohesion", "relation", "presence", "warmth",
                            "memory", "attraction", "harm", "distortion", "flux", "depth"};
    for (int i = 0; i < NUM_PILLARS; i++) {
        if (i > 0) json << ",";
        json << "\"" << names[i] << "\":" << pillars[i];
    }
    json << "}";
    return json.str();
}

bool RestApi::parse_json_int(const std::string& body, const std::string& key, int& out) {
    std::string search = "\"" + key + "\":";
    size_t pos = body.find(search);
    if (pos == std::string::npos) return false;
    pos += search.length();
    // Skip whitespace
    while (pos < body.length() && (body[pos] == ' ' || body[pos] == '\t')) pos++;
    // Parse integer
    char* endptr;
    long val = strtol(&body[pos], &endptr, 10);
    if (endptr == &body[pos]) return false;
    out = static_cast<int>(val);
    return true;
}
