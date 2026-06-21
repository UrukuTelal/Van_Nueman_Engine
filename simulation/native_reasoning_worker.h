#pragma once

#include <cstdint>
#include <thread>
#include <atomic>
#include <cstring>
#include "../vn/PillarTypes.h"
#include "../audio/wht.h"
#include "../include/Entity.h"
#include "llm_worker.h"  // reuses LLMWorkerRequest / LLMWorkerResponse

// NativeReasoningWorker — background thread that replaces the
// Ollama-based LLMWorker with the NativeCognitivePipeline.
// Same ZMQ inproc pattern, same request/response structs.
// Uses the Hopf-PID ThoughtEngine instead of HTTP POST to Ollama.

class NativeReasoningWorker {
public:
    NativeReasoningWorker(float creativity = 0.2f, float coherence = 0.8f);
    ~NativeReasoningWorker();

    bool initialize(const char* zmq_endpoint = "inproc://native_reasoning");
    void shutdown();

    bool send_request(const LLMWorkerRequest& req);
    bool poll_response(LLMWorkerResponse& resp, int timeout_ms = 0);

    uint64_t pending_count() const { return next_request_id_ - 1; }

private:
    void worker_thread_func(const char* endpoint);

    void* context_;
    void* socket_;
    std::thread worker_thread_;
    std::atomic<bool> running_;
    uint64_t next_request_id_;

    float creativity_;
    float coherence_;
};
