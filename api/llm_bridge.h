#ifndef VAN_NUEMAN_LLM_BRIDGE_H
#define VAN_NUEMAN_LLM_BRIDGE_H

#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <memory>
#include "../include/Entity.h"

enum class LLMBackend {
    OLLAMA,
    TENSORRT,
    LM_STUDIO
};

struct TensorRTConfig {
    std::string engine_path;
    int max_batch_size = 1;
    int max_input_len = 2048;
    int max_output_len = 512;
    bool enable_fp16 = true;
};

struct LLMRequest {
    std::string model;
    std::string prompt;
    PillarVector current_state;
    float temperature = 0.7f;
    int max_tokens = 512;
    LLMBackend backend = LLMBackend::OLLAMA;
};

struct LLMResponse {
    bool success;
    std::string content;
    PillarVector updated_state;
    std::string error_message;
    LLMBackend backend_used = LLMBackend::OLLAMA;
    double inference_time_ms = 0.0;
};

class LLMBridge {
public:
    LLMBridge();
    ~LLMBridge();

    bool initialize();

    LLMResponse query(const LLMRequest& request);

    static std::string build_prompt(const PillarVector& state, const std::string& task);

    static bool parse_response(const std::string& json_response, PillarVector& out_state);

    static constexpr const char* DEFAULT_OLLAMA_URL = "http://localhost:11434";
    static constexpr const char* DEFAULT_LM_STUDIO_URL = "http://localhost:1234";

    void set_backend(LLMBackend backend);
    LLMBackend get_backend() const { return current_backend_; }
    bool is_backend_available(LLMBackend backend) const;

private:
    std::string ollama_url_;
    void* curl_handle_;

    LLMBackend current_backend_ = LLMBackend::OLLAMA;

    LLMResponse query_ollama(const LLMRequest& request);
    LLMResponse query_tensorrt(const LLMRequest& request);
    LLMResponse query_lm_studio(const LLMRequest& request);
};

class TensorRTEngine {
public:
    TensorRTEngine();
    ~TensorRTEngine();

    bool load_engine(const std::string& engine_path);
    bool is_loaded() const { return engine_loaded_; }
    void unload();

    std::vector<int> infer(const std::vector<int>& input_ids, int max_new_tokens);

private:
    bool engine_loaded_ = false;
    void* engine_ = nullptr;
    void* context_ = nullptr;
};

class TensorRTInferenceSession {
public:
    explicit TensorRTInferenceSession(std::shared_ptr<TensorRTEngine> engine);
    ~TensorRTInferenceSession();

    std::string generate(const std::string& prompt, int max_new_tokens);

private:
    std::shared_ptr<TensorRTEngine> engine_;
};

#endif // VAN_NUEMAN_LLM_BRIDGE_H
