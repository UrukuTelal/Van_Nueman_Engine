#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <math.h>
#include "../kernels/pillars_compute.cu"  // Shared pillar definitions

// Fractal Pillar API: works for entity, server, and federation scales
// Ported from APIs/pillars_api.py (Python/Taichi reference)

#define MAX_API_ENTITIES 500000
#define MAX_API_SERVERS 1024
#define MAX_API_FEDERATIONS 64

// Pillar API context
struct PillarsAPIContext {
    Entity* d_entities;
    ServerState* d_servers;
    FederationState* d_federations;
    uint32_t entity_count;
    uint32_t server_count;
    uint32_t federation_count;
    
    // Grid for spatial partitioning (128x128 for entity level)
    uint32_t* d_grid_count;
    uint32_t* d_grid_entities;
    float* d_f_attraction;
};

// Initialize Pillar API
__host__ PillarsAPIContext* pillars_api_init(uint32_t max_entities, uint32_t max_servers) {
    PillarsAPIContext* ctx = (PillarsAPIContext*)malloc(sizeof(PillarsAPIContext));
    
    cudaMalloc(&ctx->d_entities, max_entities * sizeof(Entity));
    cudaMalloc(&ctx->d_servers, max_servers * sizeof(ServerState));
    cudaMalloc(&ctx->d_federations, MAX_API_FEDERATIONS * sizeof(FederationState));
    
    // Grid allocation (128x128 as per kernels/pillars_compute.cu)
    cudaMalloc(&ctx->d_grid_count, 128 * 128 * sizeof(uint32_t));
    cudaMalloc(&ctx->d_grid_entities, 128 * 128 * 32 * sizeof(uint32_t));  // MAX_PER_CELL=32
    cudaMalloc(&ctx->d_f_attraction, 128 * 128 * sizeof(float));
    
    ctx->entity_count = 0;
    ctx->server_count = 0;
    ctx->federation_count = 0;
    
    // Initialize interaction matrix
    pillars_init_kernel<<<1,1>>>();
    
    return ctx;
}

// Reset simulation (from Python reference)
__host__ void pillars_api_reset(PillarsAPIContext* ctx, uint32_t count) {
    // Launch reset kernel (initializes random positions and pillar values)
    // Simplified: just set count, actual kernel would match Python reference
    ctx->entity_count = count;
    cudaMemset(ctx->d_entities, 0, count * sizeof(Entity));
}

// Build spatial grid (from Python reference _build_grid)
__global__ void pillars_build_grid_kernel(Entity* entities, uint32_t count,
                                           uint32_t* grid_count, uint32_t* grid_entities,
                                           int GRID_RES) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= count) return;
    if (!entities[idx].alive) return;
    
    int cx = (int)(entities[idx].position.x * GRID_RES) % GRID_RES;
    int cy = (int)(entities[idx].position.y * GRID_RES) % GRID_RES;
    cx = max(0, min(GRID_RES-1, cx));
    cy = max(0, min(GRID_RES-1, cy));
    
    uint32_t cell_idx = cy * GRID_RES + cx;
    uint32_t local_idx = atomicAdd(&grid_count[cell_idx], 1);
    if (local_idx < 32) {  // MAX_PER_CELL
        grid_entities[cell_idx * 32 + local_idx] = idx;
    }
}

// Move entities (from Python reference _move)
__global__ void pillars_move_kernel(Entity* entities, uint32_t count, float dt) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= count) return;
    if (!entities[idx].alive) return;
    
    entities[idx].position.x += entities[idx].velocity.x * dt;
    entities[idx].position.y += entities[idx].velocity.y * dt;
    entities[idx].position.z += entities[idx].velocity.z * dt;
    
    // Wrap around (as per Python reference)
    entities[idx].position.x = fmodf(entities[idx].position.x + 1.0f, 1.0f);
    entities[idx].position.y = fmodf(entities[idx].position.y + 1.0f, 1.0f);
    entities[idx].position.z = fmodf(entities[idx].position.z + 1.0f, 1.0f);
}

// Main update entry point (from Python reference update())
__host__ void pillars_api_update(PillarsAPIContext* ctx, float dt) {
    if (ctx->entity_count == 0) return;
    
    // 1. Build grid
    cudaMemset(ctx->d_grid_count, 0, 128 * 128 * sizeof(uint32_t));
    pillars_build_grid_kernel<<<(ctx->entity_count + 255) / 256, 256>>>(
        ctx->d_entities, ctx->entity_count, ctx->d_grid_count, ctx->d_grid_entities, 128
    );
    
    // 2. Update pillars (from kernels/pillars_compute.cu)
    pillars_update_kernel<<<(ctx->entity_count + 255) / 256, 256>>>(
        ctx->d_entities, ctx->entity_count, dt
    );
    
    // 3. Move entities
    pillars_move_kernel<<<(ctx->entity_count + 255) / 256, 256>>>(
        ctx->d_entities, ctx->entity_count, dt
    );
    
    // 4. Aggregate server pillars (1 Hz background, but called here for simplicity)
    if (ctx->server_count > 0) {
        pillars_server_aggregate_kernel<<<ctx->server_count, 1>>>(
            ctx->d_entities, ctx->entity_count, ctx->d_servers, ctx->server_count
        );
    }
}

// Get entity data for external systems (from Python reference get_entity_data)
__host__ void pillars_api_get_data(PillarsAPIContext* ctx, Entity* h_entities, uint32_t max_count) {
    uint32_t count = min(ctx->entity_count, max_count);
    cudaMemcpy(h_entities, ctx->d_entities, count * sizeof(Entity), cudaMemcpyDeviceToHost);
}

// Fractal update: server level
__host__ void pillars_api_update_server(PillarsAPIContext* ctx, uint32_t server_id) {
    // Server-level pillar logic (simplified)
    // Would run the same pillar update loop but on ServerState
}

// Fractal update: federation level (1 Hz background)
__host__ void pillars_api_update_federation(PillarsAPIContext* ctx, float dt) {
    if (ctx->federation_count == 0 || ctx->server_count == 0) return;
    
    // Feedback loops between servers (log decay)
    // Need distance matrix between servers (simplified: identity for now)
    float* d_distance_matrix;
    cudaMalloc(&d_distance_matrix, ctx->server_count * ctx->server_count * sizeof(float));
    cudaMemset(d_distance_matrix, 0, ctx->server_count * ctx->server_count * sizeof(float));
    
    // Fill diagonal with 1.0 (distance to self)
    // Simplified: all servers are distance 1 apart
    for (uint32_t i = 0; i < ctx->server_count; i++) {
        for (uint32_t j = 0; j < ctx->server_count; j++) {
            float dist = (i == j) ? 0.0f : 1.0f;
            cudaMemcpy(&d_distance_matrix[i * ctx->server_count + j], &dist, sizeof(float), cudaMemcpyHostToDevice);
        }
    }
    
    pillars_feedback_kernel<<<ctx->server_count, 1>>>(
        ctx->d_servers, ctx->server_count, d_distance_matrix, dt
    );
    
    cudaFree(d_distance_matrix);
}

// Cleanup
__host__ void pillars_api_cleanup(PillarsAPIContext* ctx) {
    if (!ctx) return;
    cudaFree(ctx->d_entities);
    cudaFree(ctx->d_servers);
    cudaFree(ctx->d_federations);
    cudaFree(ctx->d_grid_count);
    cudaFree(ctx->d_grid_entities);
    cudaFree(ctx->d_f_attraction);
    free(ctx);
}
