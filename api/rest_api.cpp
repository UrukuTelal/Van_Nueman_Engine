#include "rest_api.h"
#include <sstream>
#include <cstring>
#include <iostream>

static const size_t MAX_BODY_SIZE = 65536;

RestApi::RestApi(SimulationTickLoop* simulation)
    : simulation_(simulation) {
    god_field.init();
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
    std::cout << "  GET  /api/god-nodes" << std::endl;
    std::cout << "  POST /api/god-nodes/create" << std::endl;
    std::cout << "  POST /api/god-nodes/decommission" << std::endl;
    return true;
}

HttpResponse RestApi::handle_request(const std::string& method,
                                      const std::string& path,
                                      const std::string& body) {
    // Reject oversized request bodies or paths
    if (body.size() > MAX_BODY_SIZE) {
        return {413, "text/plain", "Request body too large"};
    }
    if (path.size() > MAX_BODY_SIZE) {
        return {414, "text/plain", "URI too long"};
    }

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
    
    // ── Deity endpoints ──────────────────────────────────────
    if (method == "GET" && path == "/api/god-nodes") {
        return get_god_nodes();
    }
    
    if (method == "POST" && path == "/api/god-nodes/create") {
        return post_god_node_create(body);
    }
    
    if (method == "POST" && path == "/api/god-nodes/decommission") {
        return post_god_node_decommission(body);
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
    
    const auto& ecs = simulation_->get_agent_ecs();
    if (agent_id < 0 || static_cast<size_t>(agent_id) >= ecs.size() || !ecs.active(static_cast<vn::simulation::AgentECS::Index>(agent_id))) {
        return {404, "application/json", "{\"error\":\"Agent not found\"}"};
    }
    
    auto idx = static_cast<vn::simulation::AgentECS::Index>(agent_id);
    auto pillarState = ecs.get_pillars(idx);
    float x = ecs.x(idx);
    float y = ecs.y(idx);
    float z = ecs.z(idx);
    
    PillarVector pillars;
    for (int i = 0; i < NumPillars; i++) pillars[i] = pillarState[i];
    
    std::ostringstream json;
    json << "{";
    json << "\"id\":" << agent_id << ",";
    json << "\"position\":{\"x\":" << x << ",\"y\":" << y << ",\"z\":" << z << "},";
    json << "\"pillars\":" << pillar_vector_to_json(pillars);
    json << "}";
    
    return {200, "application/json", json.str()};
}

HttpResponse RestApi::get_agents_list() {
    if (!simulation_) {
        return {500, "application/json", "{\"error\":\"No simulation\"}"};
    }
    
    const auto& ecs = simulation_->get_agent_ecs();
    std::ostringstream json;
    json << "{\"agents\":[";
    
    bool first = true;
    for (size_t i = 0; i < ecs.size(); ++i) {
        if (!ecs.active(static_cast<vn::simulation::AgentECS::Index>(i))) continue;
        if (!first) json << ",";
        first = false;
        float x = ecs.x(static_cast<vn::simulation::AgentECS::Index>(i));
        float y = ecs.y(static_cast<vn::simulation::AgentECS::Index>(i));
        float z = ecs.z(static_cast<vn::simulation::AgentECS::Index>(i));
        json << "{\"id\":" << static_cast<int>(i) << ",\"x\":" << x << ",\"y\":" << y << ",\"z\":" << z << "}";
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
    if (ticks < 1) ticks = 1;
    if (ticks > 10000) ticks = 10000;
    
    for (int i = 0; i < ticks; i++) {
        simulation_->tick(1.0f / 60.0f);
    }
    
    std::ostringstream json;
    json << "{\"ticks_processed\":" << ticks << ",\"tick_count\":" << simulation_->get_world_state().tick_count << "}";
    return {200, "application/json", json.str()};
}

std::string RestApi::pillar_vector_to_json(const PillarVector& pillars) {
    std::ostringstream json;
    json << "{";
    const char* names[] = {"awareness", "willpower", "force", "influence", "resistance",
                            "integrity", "cohesion", "relation", "presence", "warmth",
                            "memory", "attraction", "harm", "distortion", "flux", "depth"};
    for (int i = 0; i < NumPillars; i++) {
        if (i > 0) json << ",";
        json << "\"" << names[i] << "\":" << pillars[i];
    }
    json << "}";
    return json.str();
}

// ── Deity endpoint implementations ──────────────────────────────

HttpResponse RestApi::get_god_nodes() {
    std::ostringstream json;
    json << "{";
    json << "\"active_count\":" << god_field.active_count() << ",";
    json << "\"node_count\":" << god_field.node_count << ",";
    json << "\"nodes\":[";
    
    bool first = true;
    for (int i = 0; i < god_field.node_count; i++) {
        if (!first) json << ",";
        first = false;
        json << "{";
        json << "\"uid\":\"" << god_field.nodes[i].uid << "\",";
        json << "\"name\":\"" << god_field.nodes[i].name << "\",";
        json << "\"depth\":" << god_field.nodes[i].depth << ",";
        json << "\"is_alive\":" << (god_field.nodes[i].is_alive() ? "true" : "false") << ",";
        json << "\"subscriber_count\":" << god_field.nodes[i].subscriber_count << ",";
        json << "\"age\":" << god_field.nodes[i].age;
        json << "}";
    }
    json << "],";
    json << "\"decommissioned_count\":" << god_field.decommissioned_count;
    json << "}";
    
    return {200, "application/json", json.str()};
}

HttpResponse RestApi::post_god_node_create(const std::string& body) {
    // Simple JSON name extraction
    std::string search = "\"name\":\"";
    size_t pos = body.find(search);
    if (pos == std::string::npos) {
        return {400, "application/json", "{\"error\":\"Missing name\"}"};
    }
    pos += search.length();
    size_t end = body.find("\"", pos);
    std::string name = body.substr(pos, end - pos);
    
    GodNode* node = god_field.create(name.c_str());
    if (!node) {
        return {500, "application/json", "{\"error\":\"Max god nodes reached\"}"};
    }
    
    std::ostringstream json;
    json << "{";
    json << "\"uid\":\"" << node->uid << "\",";
    json << "\"name\":\"" << node->name << "\",";
    json << "\"depth\":" << node->depth << ",";
    json << "\"is_alive\":true";
    json << "}";
    return {200, "application/json", json.str()};
}

HttpResponse RestApi::post_god_node_decommission(const std::string& body) {
    std::string search = "\"god_uid\":\"";
    size_t pos = body.find(search);
    if (pos == std::string::npos) {
        return {400, "application/json", "{\"error\":\"Missing god_uid\"}"};
    }
    pos += search.length();
    size_t end = body.find("\"", pos);
    std::string uid = body.substr(pos, end - pos);
    
    bool success = god_field.decommission(uid.c_str());
    if (!success) {
        return {404, "application/json", "{\"error\":\"God node not found\"}"};
    }
    
    return {200, "application/json", "{\"success\":true}"};
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
