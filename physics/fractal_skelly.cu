#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include "../kernels/pillars_shared.cuh"
#include "../kernels/skelly_shared.cuh"
#include "../api/skelly_api.cu"

// Fractal Skelly System: Scale-invariant Skelly for entity/server/federation
// From FULL_ARCHITECTURE.md:
//   Entity Scale: Pillar Vector → Bones + Muscles + Organs + Transports
//   Server Scale: Pillar Vector → Compute + Processes + DataFlow + Network
//   Federation Scale: Pillar Vector → Servers + Bandwidth + Latency + Registry

#define MAX_FEDERATION_SKELLIES 64
#define MAX_SERVER_SKELLIES 1024

// CUDA pillars are stored as floats directly — no fp20_t conversion needed

// Server-level Skelly components (Universal Terms mapping)
// Compute = Bones, Processes = Organs, DataFlow = Muscles, Network = Transports
struct ServerSkelly {
    uint32_t server_id;
    SkellyInstance instance;  // Reuse entity Skelly structure
    // Server-specific mappings:
    // bones = CPU/GPU compute units
    // muscles = bandwidth (data flow)
    // organs = running processes
    // transports = network links
    float compute_capacity;
    float bandwidth_total;
    float latency_avg;
};

// Federation-level Skelly components
// Servers = Nodes, Bandwidth = Muscles, Latency = Turgor, Registry = Brain
struct FederationSkelly {
    uint32_t federation_id;
    SkellyInstance instance;
    // Federation-specific mappings:
    // bones = server nodes
    // muscles = inter-server bandwidth
    // organs = registry/coordination processes
    // transports = federation network links
    uint32_t server_count;
    float total_bandwidth;
    float avg_latency;
};

// Initialize server-level Skelly
__host__ ServerSkelly* fractal_skelly_init_server(SkellyAPIContext* skelly_ctx, uint32_t server_id,
                                                   float compute_cap, float bandwidth) {
    ServerSkelly* server = (ServerSkelly*)malloc(sizeof(ServerSkelly));
    server->server_id = server_id;
    server->compute_capacity = compute_cap;
    server->bandwidth_total = bandwidth;
    server->latency_avg = 0.0f;
    
    // Create Skelly instance for this server
    uint32_t instance_idx = skelly_api_create_creature(skelly_ctx, server_id);
    cudaMemcpy(&server->instance, &skelly_ctx->d_instances[instance_idx], 
               sizeof(SkellyInstance), cudaMemcpyDeviceToHost);
    
    return server;
}

// Initialize federation-level Skelly
__host__ FederationSkelly* fractal_skelly_init_federation(SkellyAPIContext* skelly_ctx, 
                                                           uint32_t fed_id) {
    FederationSkelly* fed = (FederationSkelly*)malloc(sizeof(FederationSkelly));
    fed->federation_id = fed_id;
    fed->server_count = 0;
    fed->total_bandwidth = 0.0f;
    fed->avg_latency = 0.0f;
    
    // Create Skelly instance for federation
    uint32_t instance_idx = skelly_api_create_creature(skelly_ctx, fed_id);
    cudaMemcpy(&fed->instance, &skelly_ctx->d_instances[instance_idx],
               sizeof(SkellyInstance), cudaMemcpyDeviceToHost);
    
    return fed;
}

// Server tick: update server Skelly based on server Pillar State Vector
__global__ void fractal_server_tick_kernel(ServerSkelly* servers, uint32_t server_count,
                                            ServerState* server_states, float dt) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= server_count) return;
    
    ServerSkelly& server = servers[idx];
    ServerState& state = server_states[server.server_id];
    
    // Map Pillars to Skelly components (Universal Terms):
    // Force → Compute (bone strength)
    // Influence → Bandwidth (muscle activation)
    // Resistance → Latency reduction (turgor pressure)
    // Willpower → Process persistence (organ active_state)
    
    // Simplified: just update latency based on Resistance pillar
    float resistance = state.pillars[Resistance];
    server.latency_avg = 1.0f / (1.0f + resistance * 10.0f);
}

// Federation tick: update federation Skelly based on federation Pillar State Vector
__global__ void fractal_federation_tick_kernel(FederationSkelly* feds, uint32_t fed_count,
                                                 FederationState* fed_states, float dt) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= fed_count) return;
    
    FederationSkelly& fed = feds[idx];
    FederationState& state = fed_states[fed.federation_id];
    
    // Map Pillars to Federation Skelly:
    // Cohesion → Server binding (bone integrity)
    // Relation → Inter-server network (muscle groups)
    // Presence → Federation prominence (organ volume)
    // Influence → Culture propagation (transport flow)
    
    // Simplified: update total bandwidth based on Relation pillar
    float relation = state.pillars[Relation];
    fed.total_bandwidth = relation * 1000.0f;  // 1000 units per relation point
}

// Server Skelly: compute bandwidth allocation (muscle activation)
__global__ void fractal_server_bandwidth_kernel(ServerSkelly* servers, uint32_t server_count,
                                                 MuscleGroup* muscles) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= server_count) return;
    
    ServerSkelly& server = servers[idx];
    
    // Bandwidth maps to muscle activation
    float bandwidth_factor = server.bandwidth_total / 1000.0f;
    
    for (uint32_t m = server.instance.muscle_start; 
         m < server.instance.muscle_start + server.instance.muscle_count; m++) {
        MuscleGroup& mg = muscles[m];
        mg.activation = min(1.0f, bandwidth_factor);
    }
}

// Federation Skelly: compute inter-server links (transport flow)
__global__ void fractal_federation_links_kernel(FederationSkelly* feds, uint32_t fed_count,
                                                  Transport* transports) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= fed_count) return;
    
    FederationSkelly& fed = feds[idx];
    
    // Network links mapped to transport flow rate
    for (uint32_t t = fed.instance.transport_start; 
         t < fed.instance.transport_start + fed.instance.transport_count; t++) {
        Transport& transport = transports[t];
        transport.flow_rate = fed.total_bandwidth / (transport.resistance + 1e-6f);
    }
}

// Master fractal step: update all scales
__host__ void fractal_skelly_step(ServerSkelly* d_servers, uint32_t server_count,
                                   FederationSkelly* d_feds, uint32_t fed_count,
                                   ServerState* d_server_states, FederationState* d_fed_states,
                                   MuscleGroup* d_muscles, Transport* d_transports,
                                   SkellyAPIContext* skelly_ctx, float dt) {
    
    // 1. Update server Skelly instances
    if (server_count > 0) {
        uint32_t grid = (server_count + 255) / 256;
        fractal_server_tick_kernel<<<grid, 256>>>(d_servers, server_count, d_server_states, dt);
        fractal_server_bandwidth_kernel<<<grid, 256>>>(d_servers, server_count, d_muscles);
    }
    
    // 2. Update federation Skelly instances
    if (fed_count > 0) {
        uint32_t grid = (fed_count + 255) / 256;
        fractal_federation_tick_kernel<<<grid, 256>>>(d_feds, fed_count, d_fed_states, dt);
        fractal_federation_links_kernel<<<grid, 256>>>(d_feds, fed_count, d_transports);
    }
    
    // 3. Step the underlying Skelly simulation
    skelly_api_step(skelly_ctx, dt);
}
