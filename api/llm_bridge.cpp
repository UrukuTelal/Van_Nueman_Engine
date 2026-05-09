#include "llm_bridge.h"
#include <iostream>
#include <sstream>
#include <cstring>

#ifdef HAS_CURL
#include <curl/curl.h>
#endif

// Callback for CURL to capture response
#ifndef HAS_CURL
static size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    std::string* response = static_cast<std::string*>(userdata);
    size_t total = size * nmemb;
    response->append(ptr, total);
    return total;
}
#endif

LLMBridge::LLMBridge() : ollama_url_(DEFAULT_OLLAMA_URL), curl_handle_(nullptr) {
}

LLMBridge::~LLMBridge() {
#ifdef HAS_CURL
    if (curl_handle) {
        curl_easy_cleanup(static_cast<CURL*>(curl_handle));
    }
    curl_global_cleanup();
#endif
}

bool LLMBridge::initialize() {
#ifdef HAS_CURL
    CURLcode code = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (code != CURLE_OK) {
        return false;
    }
    curl_handle = curl_easy_init();
    return curl_handle != nullptr;
#else
    std::cerr << "Warning: CURL not available, LLM bridge disabled" << std::endl;
    return false;
#endif
}

LLMResponse LLMBridge::query(const LLMRequest& request) {
    LLMResponse response;
    response.success = false;
    
#ifdef HAS_CURL
    if (!curl_handle) {
        response.error_message = "LLMBridge not initialized";
        return response;
    }
    
    // Build JSON payload
    std::stringstream json_payload;
    json_payload << R"({"model":")" << request.model << R"(","prompt":")" 
                << build_prompt(request.current_state, request.prompt) 
                << R"(","stream":false,"options":{"temperature":)" 
                << request.temperature << R"(,"num_predict":)" << request.max_tokens << "}}";
    
    std::string url = ollama_url + std::string("/api/generate");
    std::string response_data;
    
    CURL* curl = static_cast<CURL*>(curl_handle);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload.str().c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
    
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    
    if (res != CURLE_OK) {
        response.error_message = curl_easy_strerror(res);
        return response;
    }
    
    // Parse response
    response.success = parse_response(response_data, response.updated_state);
    response.content = response_data;
#else
    response.error_message = "CURL not available";
#endif
    
    return response;
}

std::string LLMBridge::build_prompt(const PillarVector& state, const std::string& task) {
    std::stringstream prompt;
    prompt << "Using the Pillar Framework (16-dimensional state vector):\n";
    prompt << "Current pillar states (0.0-1.0):\n";
    const char* pillar_names[] = {
        "Awareness", "Willpower", "Force", "Influence", "Resistance",
        "Integrity", "Cohesion", "Relation", "Presence", "Warmth",
        "Memory", "Attraction", "Harm", "Distortion", "Flux", "Depth"
    };
    for (int i = 0; i < NUM_PILLARS; i++) {
        prompt << pillar_names[i] << ": " << state[i] << "\n";
    }
    prompt << "\nTASK: " << task << "\n";
    prompt << "Respond with updated pillar values in format: P0:0.XX P1:0.XX ... P15:0.XX\n";
    return prompt.str();
}

bool LLMBridge::parse_response(const std::string& json_response, PillarVector& out_state) {
    // Simple JSON parsing for response - look for "response" field
    // Expected format from Ollama: {"response":"...","done":true}
    std::string key = "\"response\":\"";
    size_t start = json_response.find(key);
    if (start == std::string::npos) return false;
    
    start += key.length();
    size_t end = json_response.find("\"", start);
    if (end == std::string::npos) return false;
    
    std::string content = json_response.substr(start, end - start);
    
    // Parse pillar values from content (format: P0:0.XX P1:0.XX ...)
    for (int i = 0; i < NUM_PILLARS; i++) {
        std::string search = "P" + std::to_string(i) + ":";
        size_t pos = content.find(search);
        if (pos != std::string::npos) {
            pos += search.length();
            // Extract float value
            char* endptr;
            float val = std::strtof(&content[pos], &endptr);
            if (val >= 0.0f && val <= 1.0f) {
                out_state[i] = val;
            }
        }
    }
    return true;
}
