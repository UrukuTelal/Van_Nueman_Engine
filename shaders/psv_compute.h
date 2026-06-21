#ifndef PSV_COMPUTE_H
#define PSV_COMPUTE_H

#include <cstdint>

#define MAX_ENTITIES 1024
#define MAX_DREAM_EPISODES 64

struct EntityPSV {
    float pillars[NumPillars];
    float shadow_patterns[NumPillars];
    float lucid_level;
    float vividness;
    float shadow_emergence;
    float dream_probability;
    float integrity;
};

struct VisualOutput {
    float r, g, b, a;
    float size;
    float glow;
    uint32_t is_dreaming;
};

enum class PSVComputeKernel : uint32_t {
    PsvToVisual = 0,
    ProcessShadows = 1
};

struct PSVComputeDispatcher {
    void* command_buffer;
    void* pipeline;
    void* descriptor_set;
    void* entity_buffer_in;
    void* visual_buffer_out;
    void* shadow_buffer_inout;
    uint32_t entity_count;
};

bool init_psv_compute(PSVComputeDispatcher* dispatcher, void* device, const char* shader_path);
void destroy_psv_compute(PSVComputeDispatcher* dispatcher);

bool psv_to_visual_dispatch(
    PSVComputeDispatcher* dispatcher,
    const EntityPSV* entities_in,
    VisualOutput* visuals_out,
    uint32_t entity_count
);

bool process_shadows_dispatch(
    PSVComputeDispatcher* dispatcher,
    EntityPSV* entities,
    uint32_t entity_count,
    float delta_time
);

#endif // PSV_COMPUTE_H