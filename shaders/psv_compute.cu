#ifndef __OPENCL_VERSION__
#define __OPENCL_VERSION__ 120
#endif

#define MAX_ENTITIES 1024
#define MAX_DREAM_EPISODES 64

typedef struct {
    float pillars[NumPillars];
    float shadow_patterns[NumPillars];
    float lucid_level;
    float vividness;
    float shadow_emergence;
    float dream_probability;
    float integrity;
} EntityPSV;

typedef struct {
    float r, g, b, a;
    float size;
    float glow;
    uint is_dreaming;
} VisualOutput;

__kernel void psv_to_visual(
    __global const EntityPSV* entities_in,
    __global VisualOutput* visuals_out,
    uint entity_count
) {
    uint id = get_global_id(0);
    if (id >= entity_count) return;

    EntityPSV e = entities_in[id];
    VisualOutput v;

    float awareness = (e.pillars[0] + e.pillars[1] + e.pillars[2] +
                       e.pillars[3] + e.pillars[4] + e.pillars[5]) / 6.0f;
    float harm = (e.pillars[6] + e.pillars[7] + e.pillars[8] +
                  e.pillars[9] + e.pillars[10] + e.pillars[11]) / 6.0f;
    float depth = (e.pillars[12] + e.pillars[13] + e.pillars[14] + e.pillars[15]) / 4.0f;

    float dream_influence = e.vividness * e.lucid_level;
    float shadow_influence = e.shadow_emergence * (e.lucid_level * 0.5f);

    v.r = clamp(awareness + dream_influence, 0.0f, 1.0f);
    v.g = clamp(harm + shadow_influence, 0.0f, 1.0f);
    v.b = clamp(depth + e.lucid_level * 0.3f, 0.0f, 1.0f);

    v.a = clamp(e.integrity * (1.0f - e.lucid_level * 0.3f), 0.0f, 1.0f);

    v.size = 1.0f + depth * 2.0f + e.lucid_level * 0.5f;

    v.glow = e.lucid_level * 0.5f + e.shadow_emergence * 0.3f;

    v.is_dreaming = (e.lucid_level > 0.1f) ? 1 : 0;

    visuals_out[id] = v;
}

__kernel void process_shadows(
    __global EntityPSV* entities,
    uint entity_count,
    float delta_time
) {
    uint id = get_global_id(0);
    if (id >= entity_count) return;

    EntityPSV e = entities[id];

    float harm_avg = (e.pillars[6] + e.pillars[7] + e.pillars[8] +
                      e.pillars[9] + e.pillars[10] + e.pillars[11]) / 6.0f;

    for (int i = 0; i < NumPillars; i++) {
        float drift = (e.pillars[i] - e.shadow_patterns[i]) * harm_avg * delta_time * 0.01f;
        e.shadow_patterns[i] += drift;
        e.shadow_patterns[i] = clamp(e.shadow_patterns[i], -1.0f, 1.0f);
    }

    entities[id] = e;
}