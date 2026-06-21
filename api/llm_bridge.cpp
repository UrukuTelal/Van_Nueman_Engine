#include "llm_bridge.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <cmath>

#ifdef HAS_CURL
#include <curl/curl.h>
#endif

// Callback for CURL to capture response
#ifdef HAS_CURL
static size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    std::string* response = static_cast<std::string*>(userdata);
    size_t total = size * nmemb;
    response->append(ptr, total);
    return total;
}
#endif

// ── Bloch Sphere Utilities ────────────────────────────────────────────

inline float bloch_to_z(float theta) {
    // Z-projection = (cos(theta) + 1) / 2
    return (std::cos(theta) + 1.0f) * 0.5f;
}

inline float z_to_bloch(float value) {
    float clamped = (value < 0.0f) ? 0.0f : (value > 1.0f) ? 1.0f : value;
    return std::acos(2.0f * clamped - 1.0f);
}

inline float get_coherence(float theta) {
    // |cos(theta)| as coherence percentage
    return std::abs(std::cos(theta));
}

inline float theta_to_phi(float) {
    // Default phi = 0 for scalar inputs
    return 0.0f;
}

LLMBridge::LLMBridge() : ollama_url_(DEFAULT_OLLAMA_URL), curl_handle_(nullptr) {
}

LLMBridge::~LLMBridge() {
#ifdef HAS_CURL
    if (curl_handle_) {
        curl_easy_cleanup(static_cast<CURL*>(curl_handle_));
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
    curl_handle_ = curl_easy_init();
    return curl_handle_ != nullptr;
#else
    std::cerr << "Warning: CURL not available, LLM bridge disabled" << std::endl;
    return false;
#endif
}

LLMResponse LLMBridge::query(const LLMRequest& request) {
    LLMResponse response;
    response.success = false;
    
#ifdef HAS_CURL
    if (!curl_handle_) {
        response.error_message = "LLMBridge not initialized";
        return response;
    }
    
    // Build JSON payload with manual escaping
    auto json_escape = [](const std::string& s) -> std::string {
        std::string out;
        out.reserve(s.size() + 8);
        for (char c : s) {
            switch (c) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default: if (static_cast<unsigned char>(c) < 0x20) break; else out += c;
            }
        }
        return out;
    };
    std::string prompt_text = build_prompt(request.current_state, request.prompt);
    std::stringstream json_payload;
    json_payload << R"({"model":")" << json_escape(request.model)
                << R"(","prompt":")" << json_escape(prompt_text)
                << R"(","stream":false,"options":{"temperature":)" 
                << request.temperature << R"(,"num_predict":)" << request.max_tokens << "}}";
    
    std::string url = ollama_url_ + std::string("/api/generate");
    std::string response_data;
    
    std::string payload = json_payload.str();
    
    CURL* curl = static_cast<CURL*>(curl_handle_);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)payload.size());
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
    prompt << "Using the Pillar Framework (16-dimensional Bloch sphere state vector):\n";
    prompt << "Current pillar states (Bloch sphere):\n";
    const char* pillar_names[] = {
        "Awareness", "Willpower", "Force", "Influence", "Resistance",
        "Integrity", "Cohesion", "Relation", "Presence", "Warmth",
        "Memory", "Attraction", "Harm", "Distortion", "Flux", "Depth"
    };
    for (int i = 0; i < NumPillars; i++) {
        float theta = state[i];
        float z = bloch_to_z(theta);
        float coherence = get_coherence(theta);
        prompt << pillar_names[i] << ": \u03B8=" << std::fixed << std::setprecision(3) << theta
               << " \u03C6=0.000"
               << " Z=" << std::setprecision(3) << z
               << " coherence=" << std::setprecision(0) << (coherence * 100.0f) << "%\n";
    }
    prompt << "\nTASK: " << task << "\n";
    prompt << "Respond with updated pillar values in format:\n";
    prompt << "P0:theta:1.047:phi:0.000 P1:theta:1.571:phi:0.000 ... P15:theta:1.571:phi:0.000\n";
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
    
    // Parse pillar values from content.
    // Supports two formats:
    //   Bloch: P0:theta:1.047:phi:0.000
    //   Scalar: P0:0.XX
    for (int i = 0; i < NumPillars; i++) {
        std::string search = "P" + std::to_string(i) + ":";
        size_t pos = content.find(search);
        if (pos != std::string::npos) {
            pos += search.length();
            
            // Check for Bloch format (P0:theta:...)
            std::string remaining = content.substr(pos);
            if (remaining.find("theta:") == 0) {
                // Bloch format: P0:theta:1.047:phi:0.000
                pos += 6; // skip "theta:"
                char* endptr;
                float theta_val = std::strtof(&content[pos], &endptr);
                if (theta_val >= 0.0f && theta_val <= 3.14159265f) {
                    out_state[i] = vn::fp20_t(theta_val);
                }
                // phi is read but not stored (PillarVector is theta only)
            } else {
                // Scalar format: P0:0.XX
                char* endptr;
                float val = std::strtof(&content[pos], &endptr);
                if (val >= 0.0f && val <= 1.0f) {
                    // Convert scalar [0,1] to theta via acos(2*val - 1)
                    out_state[i] = vn::fp20_t(z_to_bloch(val));
                }
            }
        }
    }
    return true;
}
