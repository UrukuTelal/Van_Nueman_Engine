#ifndef VAN_NUEMAN_LLM_BRIDGE_H
#define VAN_NUEMAN_LLM_BRIDGE_H

#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include "../include/Entity.h"

// Pillar state vector (16 dimensions as per PCMSRM)
using PillarVector = std::array<float, NUM_PILLARS>;

struct LLMRequest {
    std::string model;
    std::string prompt;
    PillarVector current_state;
    float temperature = 0.7f;
    int max_tokens = 512;
};

struct LLMResponse {
    bool success;
    std::string content;
    PillarVector updated_state;
    std::string error_message;
};

class LLMBridge {
public:
    LLMBridge();
    ~LLMBridge();
    
    // Initialize HTTP client
    bool initialize();
    
    // Send request to Ollama and get response
    LLMResponse query(const LLMRequest& request);
    
    // Build prompt from pillar state
    static std::string build_prompt(const PillarVector& state, const std::string& task);
    
    // Parse JSON response to extract pillar values
    static bool parse_response(const std::string& json_response, PillarVector& out_state);
    
    // Get default Ollama URL
    static constexpr const char* DEFAULT_OLLAMA_URL = "http://localhost:11434";
    
private:
    std::string ollama_url;
    void* curl_handle;  // libcurl handle
};

#endif // VAN_NUEMAN_LLM_BRIDGE_H
