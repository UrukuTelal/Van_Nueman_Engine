#ifndef PSV_VISUALIZATION_H
#define PSV_VISUALIZATION_H

#include <cstdint>
#include <cmath>
#include "../vn/PillarTypes.h"
#include "../shaders/psv_compute.h"

// PSV to visual mapping
// Color = PSV[Awareness] (R), PSV[Harm] (G), PSV[Depth] (B)
// Size = PSV[Depth] * base_scale
// Alpha = PSV[Integrity]

struct PsVizConfig {
    float base_scale;
    bool use_depth_size;
    bool use_awareness_color;
    bool use_harm_color;
};

inline float psv_clamp(float val, float min_val, float max_val) {
    return val < min_val ? min_val : (val > max_val ? max_val : val);
}

inline void psv_to_vis(const PillarStateVector& psv, float3& color, float& size, float& alpha) {
    color.x = psv.pillars[0];
    color.y = psv.pillars[12];
    color.z = psv.pillars[15];
    size = 1.0f + psv.pillars[15] * 2.0f;
    alpha = psv.pillars[5];
}

inline void psv_to_vis(const PillarStateVector& psv, float& lucid_level, float& vividness,
                       float& shadow_emergence, VisualOutput& out) {
    float awareness = (psv.pillars[0] + psv.pillars[1] + psv.pillars[2] +
                       psv.pillars[3] + psv.pillars[4] + psv.pillars[5]) / 6.0f;
    float harm = (psv.pillars[6] + psv.pillars[7] + psv.pillars[8] +
                  psv.pillars[9] + psv.pillars[10] + psv.pillars[11]) / 6.0f;
    float depth = (psv.pillars[12] + psv.pillars[13] + psv.pillars[14] + psv.pillars[15]) / 4.0f;

    float dream_influence = vividness * lucid_level;
    float shadow_influence = shadow_emergence * (lucid_level * 0.5f);

    out.r = psv_clamp(awareness + dream_influence, 0.0f, 1.0f);
    out.g = psv_clamp(harm + shadow_influence, 0.0f, 1.0f);
    out.b = psv_clamp(depth + lucid_level * 0.3f, 0.0f, 1.0f);
    out.a = psv_clamp(psv.pillars[5] * (1.0f - lucid_level * 0.3f), 0.0f, 1.0f);
    out.size = 1.0f + depth * 2.0f + lucid_level * 0.5f;
    out.glow = lucid_level * 0.5f + shadow_emergence * 0.3f;
    out.is_dreaming = (lucid_level > 0.1f) ? 1 : 0;
}

#ifndef VN_NO_VULKAN

struct PSVVisualizationGPU {
    void* device;
    void* command_buffer;
    PSVComputeDispatcher dispatcher;
    bool initialized;
};

bool init_psv_visualization_gpu(PSVVisualizationGPU* gpu, void* device);
void destroy_psv_visualization_gpu(PSVVisualizationGPU* gpu);

inline bool gpu_psv_to_vis(PSVVisualizationGPU* gpu,
                           const EntityPSV* entities_in,
                           VisualOutput* visuals_out,
                           uint32_t entity_count) {
    if (!gpu || !gpu->initialized) {
        return false;
    }
    return psv_to_visual_dispatch(&gpu->dispatcher, entities_in, visuals_out, entity_count);
}

inline bool gpu_process_shadows(PSVVisualizationGPU* gpu,
                                 EntityPSV* entities,
                                 uint32_t entity_count,
                                 float delta_time) {
    if (!gpu || !gpu->initialized) {
        return false;
    }
    return process_shadows_dispatch(&gpu->dispatcher, entities, entity_count, delta_time);
}

#else

struct PSVVisualizationGPU {
    bool initialized;
};

inline bool init_psv_visualization_gpu(PSVVisualizationGPU*, void*) {
    return false;
}

inline void destroy_psv_visualization_gpu(PSVVisualizationGPU*) {
}

inline bool gpu_psv_to_vis(PSVVisualizationGPU*,
                           const EntityPSV* entities,
                           VisualOutput* visuals_out,
                           uint32_t entity_count) {
    for (uint32_t i = 0; i < entity_count; i++) {
        PillarStateVector psv;
        for (int j = 0; j < NUM_PILLARS; j++) {
            psv.pillars[j] = entities[i].pillars[j];
        }
        psv_to_vis(psv, entities[i].lucid_level, entities[i].vividness,
                   entities[i].shadow_emergence, visuals_out[i]);
    }
    return true;
}

inline bool gpu_process_shadows(PSVVisualizationGPU*,
                                 EntityPSV* entities,
                                 uint32_t entity_count,
                                 float delta_time) {
    for (uint32_t i = 0; i < entity_count; i++) {
        float harm_avg = (entities[i].pillars[6] + entities[i].pillars[7] +
                         entities[i].pillars[8] + entities[i].pillars[9] +
                         entities[i].pillars[10] + entities[i].pillars[11]) / 6.0f;
        for (int j = 0; j < NUM_PILLARS; j++) {
            float drift = (entities[i].pillars[j] - entities[i].shadow_patterns[j])
                          * harm_avg * delta_time * 0.01f;
            entities[i].shadow_patterns[j] += drift;
            entities[i].shadow_patterns[j] = psv_clamp(entities[i].shadow_patterns[j], -1.0f, 1.0f);
        }
    }
    return true;
}

#endif

#endif // PSV_VISUALIZATION_H