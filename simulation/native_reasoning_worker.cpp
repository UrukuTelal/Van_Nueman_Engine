#include "native_reasoning_worker.h"
#include "../quantum/cognitive_pipeline.h"
#include "../cognition/fugure/fugure.h"
#include <iostream>

#include <zmq.h>

NativeReasoningWorker::NativeReasoningWorker(float creativity, float coherence)
    : context_(nullptr), socket_(nullptr), running_(false), next_request_id_(1),
      creativity_(creativity), coherence_(coherence) {
}

NativeReasoningWorker::~NativeReasoningWorker() {
    shutdown();
}

bool NativeReasoningWorker::initialize(const char* zmq_endpoint) {
    context_ = zmq_ctx_new();
    if (!context_) {
        std::cerr << "NativeReasoningWorker: Failed to create ZMQ context" << std::endl;
        return false;
    }

    socket_ = zmq_socket(context_, ZMQ_PAIR);
    if (!socket_) {
        std::cerr << "NativeReasoningWorker: Failed to create ZMQ socket" << std::endl;
        zmq_ctx_destroy(context_);
        context_ = nullptr;
        return false;
    }

    int rc = zmq_bind(socket_, zmq_endpoint);
    if (rc != 0) {
        std::cerr << "NativeReasoningWorker: Failed to bind to " << zmq_endpoint << std::endl;
        zmq_close(socket_);
        zmq_ctx_destroy(context_);
        socket_ = nullptr;
        context_ = nullptr;
        return false;
    }

    running_ = true;
    worker_thread_ = std::thread(&NativeReasoningWorker::worker_thread_func, this, zmq_endpoint);

    std::cout << "NativeReasoningWorker: initialized on " << zmq_endpoint
              << " (creativity=" << creativity_ << ", coherence=" << coherence_ << ")" << std::endl;
    return true;
}

void NativeReasoningWorker::shutdown() {
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

bool NativeReasoningWorker::send_request(const LLMWorkerRequest& req) {
    if (!socket_) return false;

    LLMWorkerRequest req_copy = req;
    req_copy.request_id = next_request_id_++;

    int rc = zmq_send(socket_, &req_copy, sizeof(LLMWorkerRequest), 0);
    return rc == sizeof(LLMWorkerRequest);
}

bool NativeReasoningWorker::poll_response(LLMWorkerResponse& resp, int timeout_ms) {
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

void NativeReasoningWorker::worker_thread_func(const char* endpoint) {
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

    // Create the native cognitive pipeline (replaces LLMBridge + Ollama)
    vn::quantum::NativeCognitivePipeline pipeline(creativity_, coherence_);

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
        resp.success = true;
        resp.reasoning[0] = '\0';

        // Decompress WHT → PSV
        float decoded[NumPillars];
        LLMWorker::decompress_from_wht(req.wht_coefficients, decoded);

        // Build input PSV
        PillarStateVector input;
        for (int i = 0; i < NumPillars; i++) {
            input.pillars[i] = decoded[i];
        }

        // Run native cognitive pipeline (replaces LLM inference)
        // The pipeline uses Hopf-PID + WHT tokenizer + ThoughtEngine
        // instead of an external model.
        PillarStateVector output = pipeline.process(input);

        // FUGURE adversarial defense:
        // Isolates, accelerates to saturation, maps, and strips any
        // distortion induced by the pipeline or environment.
        {
            auto& fugure = FugureRuntime::instance();
            FugureExtractionResult fr = fugure.process(output);
            if (fr.extraction_confidence > 0.5f) {
                output = fr.true_payload;
            }
        }

        // Compress response back to theta values
        for (int i = 0; i < NumPillars; i++) {
            resp.pillars_theta[i] = output.pillars[i];
        }

        zmq_send(sock, &resp, sizeof(LLMWorkerResponse), 0);
    }

    zmq_close(sock);
    zmq_ctx_destroy(ctx);
}
