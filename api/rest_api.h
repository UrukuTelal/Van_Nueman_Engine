#ifndef VAN_NUEMAN_API_REST_H
#define VAN_NUEMAN_API_REST_H

#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <memory>
#include "../simulation/tick_loop.h"

// Simple HTTP response
struct HttpResponse {
    int status_code;
    std::string content_type;
    std::string body;
};

// REST API for Van Nueman simulation
class RestApi {
public:
    RestApi(SimulationTickLoop* simulation);
    ~RestApi() = default;
    
    // Start HTTP server (blocking)
    bool start(int port = 8080);
    
    // Handle requests (called by server implementation)
    HttpResponse handle_request(const std::string& method,
                                const std::string& path,
                                const std::string& body);
    
private:
    SimulationTickLoop* simulation_;
    
    // Endpoint handlers
    HttpResponse get_world_state();
    HttpResponse get_agent(int agent_id);
    HttpResponse post_tick(const std::string& body);
    HttpResponse get_chunk(int x, int y, int z);
    HttpResponse get_agents_list();
    
    // Helper: parse JSON (simple implementation)
    static bool parse_json_int(const std::string& body, const std::string& key, int& out);
    static std::string pillar_vector_to_json(const std::array<float, NUM_PILLARS>& pillars);
};

#endif // VAN_NUEMAN_API_REST_H
