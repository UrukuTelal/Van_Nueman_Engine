#ifndef VAN_NUEMAN_API_WEBSOCKET_H
#define VAN_NUEMAN_API_WEBSOCKET_H

#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include "../include/Entity.h"
#include "../simulation/tick_loop.h"

// WebSocket frame types
enum class WSFrameType {
    TEXT,
    BINARY,
    PING,
    PONG,
    CLOSE
};

// WebSocket message
struct WSMessage {
    WSFrameType type;
    std::vector<uint8_t> data;
    bool fin;
};

// WebSocket server for real-time simulation streaming
class WebSocketApi {
public:
    WebSocketApi(SimulationTickLoop* simulation);
    ~WebSocketApi() = default;
    
    // Start WebSocket server
    bool start(int port = 8081);
    
    // Send agent telemetry
    void broadcast_agent_update(int agent_id);
    void broadcast_pillar_update(const std::array<float, NUM_PILLARS>& pillars);
    void broadcast_tick();
    
    // Handle incoming messages
    void handle_message(const std::string& client_id, const WSMessage& msg);
    
private:
    SimulationTickLoop* simulation_;
    
    // JSON builders
    static std::string build_agent_json(int agent_id, float x, float y, float z,
                                        const std::array<float, NUM_PILLARS>& pillars);
    static std::string build_pillar_json(const std::array<float, NUM_PILLARS>& pillars);
    static std::string build_tick_json(uint32_t tick_count);
};

#endif // VAN_NUEMAN_API_WEBSOCKET_H
