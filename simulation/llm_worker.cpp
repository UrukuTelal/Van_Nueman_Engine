#include "llm_worker.h"
#include <cstring>

// These static WHT math functions are always needed (both native and Ollama paths)
void LLMWorker::compress_to_wht(const float pillars[NumPillars], float wht_out[WHT_N]) {
    std::memcpy(wht_out, pillars, NumPillars * sizeof(float));
    for (int i = NumPillars; i < WHT_N; i++) {
        wht_out[i] = 0.0f;
    }
    fwht(wht_out, WHT_LOG2_N);
}

void LLMWorker::decompress_from_wht(const float wht_in[WHT_N], float pillars_out[NumPillars]) {
    float temp[WHT_N];
    std::memcpy(temp, wht_in, WHT_N * sizeof(float));
    ifwht(temp, WHT_LOG2_N);
    std::memcpy(pillars_out, temp, NumPillars * sizeof(float));
}

#ifndef VN_USE_NATIVE_REASONING
#include "../api/llm_bridge.h"
#include <iostream>

#include <zmq.h>

LLMWorker::LLMWorker()
    : context_(nullptr), socket_(nullptr), running_(false), next_request_id_(1) {
}

LLMWorker::~LLMWorker() {
    shutdown();
}

bool LLMWorker::initialize(const char* zmq_endpoint) {
    context_ = zmq_ctx_new();
    if (!context_) {
        std::cerr << "LLMWorker: Failed to create ZMQ context" << std::endl;
        return false;
    }

    socket_ = zmq_socket(context_, ZMQ_PAIR);
    if (!socket_) {
        std::cerr << "LLMWorker: Failed to create ZMQ socket" << std::endl;
        zmq_ctx_destroy(context_);
        context_ = nullptr;
        return false;
    }

    int rc = zmq_bind(socket_, zmq_endpoint);
    if (rc != 0) {
        std::cerr << "LLMWorker: Failed to bind to " << zmq_endpoint << std::endl;
        zmq_close(socket_);
        zmq_ctx_destroy(context_);
        socket_ = nullptr;
        context_ = nullptr;
        return false;
    }

    running_ = true;
    worker_thread_ = std::thread(&LLMWorker::worker_thread_func, this, zmq_endpoint);

    std::cout << "LLMWorker: initialized on " << zmq_endpoint << std::endl;
    return true;
}

void LLMWorker::shutdown() {
    if (running_) {
        running_ = false;
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }
    if (socket_) {
        zmq_close(socket_);
        socket_ = nullptr;
    }
    if (context_) {
        zmq_ctx_destroy(context_);
        context_ = nullptr;
    }
}

bool LLMWorker::send_request(const LLMWorkerRequest& req) {
    if (!socket_) return false;

    LLMWorkerRequest req_copy = req;
    req_copy.request_id = next_request_id_++;

    int rc = zmq_send(socket_, &req_copy, sizeof(LLMWorkerRequest), 0);
    return rc == sizeof(LLMWorkerRequest);
}

bool LLMWorker::poll_response(LLMWorkerResponse& resp, int timeout_ms) {
    if (!socket_) return false;

    zmq_pollitem_t item;
    item.socket = socket_;
    item.fd = 0;
    item.events = ZMQ_POLLIN;
    item.revents = 0;

    int rc = zmq_poll(&item, 1, timeout_ms);
    if (rc <= 0) return false;

    rc = zmq_recv(socket_, &resp, sizeof(LLMWorkerResponse), 0);
    return rc == sizeof(LLMWorkerResponse);
}

void LLMWorker::worker_thread_func(const char* endpoint) {
    void* ctx = zmq_ctx_new();
    if (!ctx) return;

    void* sock = zmq_socket(ctx, ZMQ_PAIR);
    if (!sock) {
        zmq_ctx_destroy(ctx);
        return;
    }

    int rc = zmq_connect(sock, endpoint);
    if (rc != 0) {
        zmq_close(sock);
        zmq_ctx_destroy(ctx);
        return;
    }

    ::LLMBridge llm;
    bool llm_ok = llm.initialize();

    zmq_pollitem_t item;
    item.socket = sock;
    item.fd = 0;
    item.events = ZMQ_POLLIN;
    item.revents = 0;

    while (running_) {
        rc = zmq_poll(&item, 1, 100);
        if (rc <= 0) continue;

        LLMWorkerRequest req;
        rc = zmq_recv(sock, &req, sizeof(LLMWorkerRequest), 0);
        if (rc != sizeof(LLMWorkerRequest)) continue;

        LLMWorkerResponse resp;
        resp.agent_id = req.agent_id;
        resp.request_id = req.request_id;
        resp.success = false;
        resp.reasoning[0] = '\0';

        if (llm_ok) {
            float decoded[NumPillars];
            decompress_from_wht(req.wht_coefficients, decoded);

            ::LLMRequest api_req;
            api_req.model = "llama3.2";
            api_req.prompt = req.task;
            api_req.temperature = 0.7f;
            api_req.max_tokens = 256;

            for (int i = 0; i < NumPillars; i++) {
                api_req.current_state[i] = vn::fp20_t(decoded[i]);
            }

            ::LLMResponse api_resp = llm.query(api_req);
            resp.success = api_resp.success;

            if (api_resp.success) {
                for (int i = 0; i < NumPillars; i++) {
                    resp.pillars_theta[i] = api_resp.updated_state[i];
                }
                std::strncpy(resp.reasoning, api_resp.content.c_str(), LLM_WORKER_REASONING_MAX - 1);
                resp.reasoning[LLM_WORKER_REASONING_MAX - 1] = '\0';
            } else {
                std::memcpy(resp.pillars_theta, decoded, NumPillars * sizeof(float));
            }
        } else {
            float decoded[NumPillars];
            decompress_from_wht(req.wht_coefficients, decoded);
            std::memcpy(resp.pillars_theta, decoded, NumPillars * sizeof(float));
            resp.success = true;
        }

        zmq_send(sock, &resp, sizeof(LLMWorkerResponse), 0);
    }

    zmq_close(sock);
    zmq_ctx_destroy(ctx);
}
#endif // VN_USE_NATIVE_REASONING
