#ifndef VAN_NUEMAN_LLM_WORKER_H
#define VAN_NUEMAN_LLM_WORKER_H

#include <cstdint>
#include <thread>
#include <atomic>
#include <cstring>
#include "../vn/PillarTypes.h"
#include "../audio/wht.h"
#include "../include/Entity.h"

#define LLM_WORKER_TASK_MAX 512
#define LLM_WORKER_REASONING_MAX 1024

struct LLMWorkerRequest {
    uint32_t agent_id;
    float pillars_theta[NumPillars];
    float wht_coefficients[WHT_N];
    char task[LLM_WORKER_TASK_MAX];
    uint64_t request_id;
};

struct LLMWorkerResponse {
    uint32_t agent_id;
    float pillars_theta[NumPillars];
    uint64_t request_id;
    bool success;
    char reasoning[LLM_WORKER_REASONING_MAX];
};

class LLMWorker {
public:
    LLMWorker();
    ~LLMWorker();

    bool initialize(const char* zmq_endpoint = "inproc://llm_worker");
    void shutdown();

    bool send_request(const LLMWorkerRequest& req);
    bool poll_response(LLMWorkerResponse& resp, int timeout_ms = 0);

    static void compress_to_wht(const float pillars[NumPillars], float wht_out[WHT_N]);
    static void decompress_from_wht(const float wht_in[WHT_N], float pillars_out[NumPillars]);

    uint64_t pending_count() const { return next_request_id_ - 1; }

private:
    void worker_thread_func(const char* endpoint);

    void* context_;
    void* socket_;
    std::thread worker_thread_;
    std::atomic<bool> running_;
    uint64_t next_request_id_;
};

#endif
