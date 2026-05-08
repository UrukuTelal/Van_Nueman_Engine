// skelly_api_c.cpp - C wrapper for Skelly API (links with skelly_api.spv)
#include <cstdint>
#include <cstring>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque context
typedef struct SkellyAPIContextImpl* SkellyAPIContext;

// Forward declarations from skelly_api.spv (compiled with vncc)
extern "C" SkellyAPIContext skelly_api_init();
extern "C" void skelly_api_cleanup(SkellyAPIContext ctx);
extern "C" int32_t skelly_api_create_creature(SkellyAPIContext ctx, int32_t parent_idx, 
                                        float x, float y, float z, const char* name);
extern "C" int32_t skelly_api_add_organ(SkellyAPIContext ctx, const char* name, 
                                     int32_t type, int32_t segment_idx, float volume);
extern "C" void skelly_api_set_organ_activity(SkellyAPIContext ctx, const char* organ_name, float intensity);
extern "C" void skelly_api_step(SkellyAPIContext ctx, float dt);
extern "C" void skelly_api_get_state(SkellyAPIContext ctx, float* energy_out, 
                                  float* pressure_avg_out, bool* is_alive_out);

// Wrapper implementations (matching skelly_api.h)
SkellyAPIContext __real_skelly_api_init() {
    return skelly_api_init();
}

void __real_skelly_api_cleanup(SkellyAPIContext ctx) {
    skelly_api_cleanup(ctx);
}

int32_t __real_skelly_api_create_creature(SkellyAPIContext ctx, int32_t parent_idx,
                                         float x, float y, float z, const char* name) {
    return skelly_api_create_creature(ctx, parent_idx, x, y, z, name);
}

// ... etc

#ifdef __cplusplus
}
#endif
