// Van Nueman Skelly API - vncc C++ Version
// Compile: vncc -emit-spirv skelly_api.cpp -o skelly_api.spv
//          vncc -emit-llvm skelly_api.cpp -o skelly_api.bc

#include <cstdint>
#include <cmath>

#define MAX_API_INSTANCES 100000
#define MAX_BONES 1000000
#define MAX_SEGMENTS 1000000
#define MAX_MUSCLES 2000000
#define MAX_STRANDS (MAX_MUSCLES * 8)
#define MAX_ORGANS (MAX_SEGMENTS * 4)
#define MAX_TRANSPORTS MAX_SEGMENTS
#define MAX_FLUIDS MAX_SEGMENTS

// Float3 operations
struct float3 { float x, y, z; };

float3 make_float3(float x, float y, float z) { float3 r = {x, y, z}; return r; }
float3 float3_add(float3 a, float3 b) { return make_float3(a.x+b.x, a.y+b.y, a.z+b.z); }
float3 float3_sub(float3 a, float3 b) { return make_float3(a.x-b.x, a.y-b.y, a.z-b.z); }
float3 float3_mul_scalar(float3 a, float s) { return make_float3(a.x*s, a.y*s, a.z*s); }
float float3_dot(float3 a, float3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
float3 float3_cross(float3 a, float3 b) {
    return make_float3(a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x);
}
float float3_length(float3 v) { return sqrtf(float3_dot(v, v)); }
float3 float3_normalize(float3 v) { float l = float3_length(v); return l > 0 ? float3_mul_scalar(v, 1.0f/l) : v; }

// Skelly structures with fractal attributes
struct BoneNode {
    float3 local_pos;
    float3 global_pos;
    uint32_t parent_idx;
    uint32_t is_fractured;
    float constraint_pitch_min, constraint_pitch_max;
    float constraint_yaw_min, constraint_yaw_max;
    float constraint_roll_min, constraint_roll_max;
} __attribute__((annotate("fractal")));

struct BoneSegment {
    uint32_t start_node, end_node;
    float flexibility;       // 0 = rigid, 1 = very flexible
    float break_threshold;
    uint32_t is_fractured;
    float total_capacity;    // volume capacity for muscles/organs
    uint32_t organ_count;
    uint32_t organ_start;    // index into organ list
} __attribute__((annotate("fractal")));

struct MuscleStrand {
    float base_r;
    float current_r;
    float origin_rot, insertion_rot;
};

struct MuscleGroup {
    uint32_t origin_node, insertion_node;
    uint32_t strand_start;   // index into strand list
    uint32_t strand_count;
    float activation;        // 0-1
    float current_volume;
} __attribute__((annotate("fractal")));

struct Organ {
    uint32_t type;           // 0=pump, 1=valve, 2=power_plant, 3=factory
    float volume;
    float active_state;      // 0-1 pulse intensity
    float energy_output;
    uint32_t segment_idx;    // which segment this organ is attached to
} __attribute__((annotate("fractal")));

struct Transport {
    uint32_t start_node, end_node;
    float flow_rate;
    float resistance;
    float pressure;
    uint32_t is_severed;
    float elasticity;
} __attribute__((annotate("fractal")));

struct InterstitialFluid {
    uint32_t transport_idx;
    uint32_t segment_idx;
    float turgor_pressure;
} __attribute__((annotate("fractal")));

struct SkellyInstance {
    uint32_t entity_id;
    uint32_t bone_start, bone_count;
    uint32_t segment_start, segment_count;
    uint32_t muscle_start, muscle_count;
    uint32_t organ_start, organ_count;
    uint32_t transport_start, transport_count;
} __attribute__((annotate("fractal")));

// Skelly API context implementation
struct SkellyAPIContextImpl {
    SkellyInstance* d_instances;
    BoneNode* d_bones;
    BoneSegment* d_segments;
    MuscleGroup* d_muscles;
    MuscleStrand* d_strands;
    Organ* d_organs;
    Transport* d_transports;
    InterstitialFluid* d_fluids;

    uint32_t instance_count;
    uint32_t bone_count;
    uint32_t segment_count;
    uint32_t muscle_count;
    uint32_t strand_count;
    uint32_t organ_count;
    uint32_t transport_count;
    uint32_t fluid_count;

    // Next free slot pointers
    uint32_t next_instance;
    uint32_t next_bone;
    uint32_t next_segment;
    uint32_t next_muscle;
    uint32_t next_strand;
    uint32_t next_organ;
    uint32_t next_transport;
    uint32_t next_fluid;
};

// Initialize Skelly API
SkellyAPIContext skelly_api_init() {
    SkellyAPIContextImpl* ctx = new SkellyAPIContextImpl();

    ctx->d_instances = new SkellyInstance[MAX_API_INSTANCES];
    ctx->d_bones = new BoneNode[MAX_BONES];
    ctx->d_segments = new BoneSegment[MAX_SEGMENTS];
    ctx->d_muscles = new MuscleGroup[MAX_MUSCLES];
    ctx->d_strands = new MuscleStrand[MAX_STRANDS];
    ctx->d_organs = new Organ[MAX_ORGANS];
    ctx->d_transports = new Transport[MAX_TRANSPORTS];
    ctx->d_fluids = new InterstitialFluid[MAX_FLUIDS];

    ctx->instance_count = 0;
    ctx->bone_count = 0;
    ctx->segment_count = 0;
    ctx->muscle_count = 0;
    ctx->strand_count = 0;
    ctx->organ_count = 0;
    ctx->transport_count = 0;
    ctx->fluid_count = 0;

    ctx->next_instance = 0;
    ctx->next_bone = 0;
    ctx->next_segment = 0;
    ctx->next_muscle = 0;
    ctx->next_strand = 0;
    ctx->next_organ = 0;
    ctx->next_transport = 0;
    ctx->next_fluid = 0;

    return ctx;
}

// Create a creature
int32_t skelly_api_create_creature(SkellyAPIContext ctx, int32_t parent_idx, 
                                     float x, float y, float z, const char* name) {
    if (!ctx || ctx->next_instance >= MAX_API_INSTANCES) return -1;

    uint32_t instance_idx = ctx->next_instance++;
    SkellyInstance& inst = ctx->d_instances[instance_idx];
    inst.entity_id = parent_idx;
    inst.bone_start = ctx->next_bone;
    inst.bone_count = 0;
    inst.segment_start = ctx->next_segment;
    inst.segment_count = 0;
    inst.muscle_start = ctx->next_muscle;
    inst.muscle_count = 0;
    inst.organ_start = ctx->next_organ;
    inst.organ_count = 0;
    inst.transport_start = ctx->next_transport;
    inst.transport_count = 0;

    // Create root bone
    uint32_t root_idx = ctx->next_bone++;
    BoneNode& root = ctx->d_bones[root_idx];
    root.local_pos = make_float3(0, 0, 0);
    root.global_pos = make_float3(0, 0, 0);
    root.parent_idx = (uint32_t)-1;
    root.is_fractured = 0;
    root.constraint_pitch_min = -1.5f; root.constraint_pitch_max = 1.5f;
    root.constraint_yaw_min = -1.5f; root.constraint_yaw_max = 1.5f;
    root.constraint_roll_min = -0.5f; root.constraint_roll_max = 0.5f;
    inst.bone_start = root_idx;
    inst.bone_count = 1;

    // Create core bone
    uint32_t core_idx = ctx->next_bone++;
    BoneNode& core = ctx->d_bones[core_idx];
    core.local_pos = make_float3(0, 0, 20);
    core.global_pos = make_float3(0, 0, 20);
    core.parent_idx = root_idx;
    core.is_fractured = 0;
    core.constraint_pitch_min = -1.5f; core.constraint_pitch_max = 1.5f;
    core.constraint_yaw_min = -1.5f; core.constraint_yaw_max = 1.5f;
    core.constraint_roll_min = -0.5f; core.constraint_roll_max = 0.5f;
    inst.bone_count++;

    // Create core segment
    uint32_t seg_idx = ctx->next_segment++;
    BoneSegment& seg = ctx->d_segments[seg_idx];
    seg.start_node = root_idx;
    seg.end_node = core_idx;
    seg.flexibility = 0.1f;
    seg.break_threshold = 100.0f;
    seg.is_fractured = 0;
    seg.total_capacity = 3.14159f * 2.0f * 2.0f * 20.0f;  // pi*r²*len
    seg.organ_count = 0;
    seg.organ_start = 0;
    inst.segment_count = 1;

    // Create core muscle group
    uint32_t muscle_idx = ctx->next_muscle++;
    MuscleGroup& mg = ctx->d_muscles[muscle_idx];
    mg.origin_node = root_idx;
    mg.insertion_node = core_idx;
    mg.strand_start = ctx->next_strand;
    mg.strand_count = 8;
    mg.activation = 0.0f;
    mg.current_volume = 0.0f;
    ctx->next_strand += 8;
    inst.muscle_count = 1;

    // Create core organ (power_plant)
    uint32_t organ_idx = ctx->next_organ++;
    Organ& organ = ctx->d_organs[organ_idx];
    organ.type = 2;  // power_plant
    organ.volume = 50.0f;
    organ.active_state = 0.0f;
    organ.energy_output = 0.0f;
    organ.segment_idx = seg_idx;
    inst.organ_count = 1;
    seg.organ_start = organ_idx;
    seg.organ_count = 1;

    // Create interstitial fluid for segment
    uint32_t fluid_idx = ctx->next_fluid++;
    InterstitialFluid& fluid = ctx->d_fluids[fluid_idx];
    fluid.transport_idx = ctx->next_transport++;
    fluid.segment_idx = seg_idx;
    fluid.turgor_pressure = 1.0f;
    inst.transport_count = 1;

    ctx->instance_count++;

    return instance_idx;
}

// Set organ activity
void skelly_api_set_organ_activity(SkellyAPIContext ctx, const char* organ_name, float intensity) {
    if (!ctx) return;
    // Simplified: for now, iterate through all organs to find by name
    // In a real implementation, you'd want a hash map or similar
    for (uint32_t i = 0; i < ctx->organ_count; i++) {
        // Would compare organ_name with actual name storage
        // For now, just set intensity on all organs
        ctx->d_organs[i].active_state = (intensity < 0.0f) ? 0.0f : ((intensity > 1.0f) ? 1.0f : intensity);
    }
}

// Step simulation [[fractal]]
void skelly_api_step(SkellyAPIContext ctx, float dt) __attribute__((annotate("fractal"))) {
    if (!ctx || ctx->instance_count == 0) return;

    // 1. Update bone global positions
    for (uint32_t idx = 0; idx < ctx->bone_count; idx++) {
        BoneNode& node = ctx->d_bones[idx];
        if (node.parent_idx == (uint32_t)-1 || node.is_fractured) {
            node.global_pos = node.local_pos;
        } else {
            BoneNode& parent = ctx->d_bones[node.parent_idx];
            node.global_pos = float3_add(parent.global_pos, node.local_pos);
        }
    }

    // 2. Update muscles
    for (uint32_t idx = 0; idx < ctx->muscle_count; idx++) {
        MuscleGroup& mg = ctx->d_muscles[idx];
        BoneNode& origin = ctx->d_bones[mg.origin_node];
        BoneNode& insertion = ctx->d_bones[mg.insertion_node];

        float curr_len = float3_length(float3_sub(insertion.global_pos, origin.global_pos));
        float rest_len = curr_len;

        float expansion = (rest_len > 0 && curr_len > 0) ? sqrtf(rest_len / curr_len) : 1.0f;

        for (uint32_t s = 0; s < mg.strand_count; s++) {
            MuscleStrand& strand = ctx->d_strands[mg.strand_start + s];
            strand.current_r = strand.base_r * expansion * (1.0f + mg.activation);
        }

        mg.current_volume = 0.0f;
        for (uint32_t s = 0; s < mg.strand_count; s++) {
            MuscleStrand& strand = ctx->d_strands[mg.strand_start + s];
            mg.current_volume += 3.14159f * strand.current_r * strand.current_r * curr_len;
        }
    }

    // 3. Operate organs
    for (uint32_t idx = 0; idx < ctx->organ_count; idx++) {
        Organ& organ = ctx->d_organs[idx];

        if (organ.type == 2) {  // power_plant
            organ.energy_output = organ.volume * organ.active_state * 5.0f;
        } else if (organ.type == 3) {  // factory
            organ.energy_output = organ.volume * organ.active_state * 1.2f;
        } else if (organ.type == 0 && organ.segment_idx < MAX_TRANSPORTS) {  // pump
            Transport& t = ctx->d_transports[organ.segment_idx];
            t.flow_rate += (organ.volume * organ.active_state) / (t.resistance + 1e-6f);
        } else if (organ.type == 1) {  // valve
            if (organ.segment_idx < MAX_TRANSPORTS) {
                Transport& t = ctx->d_transports[organ.segment_idx];
                t.resistance = 1.0f + (organ.active_state * 10.0f);
            }
        }

        organ.active_state *= 0.99f;  // decay
    }

    // 4. Update turgor pressure
    for (uint32_t idx = 0; idx < ctx->fluid_count; idx++) {
        InterstitialFluid& fluid = ctx->d_fluids[idx];
        Transport& t = ctx->d_transports[fluid.transport_idx];

        float occ = (float)ctx->muscle_count * 0.1f;
        fluid.turgor_pressure = 1.0f + (occ * 1.5f);
        t.pressure = fluid.turgor_pressure;
        t.flow_rate *= (1.0f / (fluid.turgor_pressure + 1e-6f));
    }
}

// Add organ to segment
int32_t skelly_api_add_organ(SkellyAPIContext ctx, const char* name, int32_t type,
                               int32_t segment_idx, float volume) {
    if (!ctx || segment_idx < 0 || (uint32_t)segment_idx >= ctx->segment_count) return -1;
    
    uint32_t organ_idx = ctx->next_organ++;
    if (organ_idx >= MAX_ORGANS) return -1;
    
    Organ& organ = ctx->d_organs[organ_idx];
    organ.type = type;
    organ.volume = volume;
    organ.active_state = 0.0f;
    organ.energy_output = 0.0f;
    organ.segment_idx = segment_idx;
    
    // Update segment's organ list
    BoneSegment& seg = ctx->d_segments[segment_idx];
    if (seg.organ_count == 0) {
        seg.organ_start = organ_idx;
    }
    seg.organ_count++;
    
    return organ_idx;
}

// Get system state
SystemState skelly_api_get_system_state(SkellyAPIContext ctx) {
    SystemState state;
    state.total_energy = 0.0f;
    for (uint32_t i = 0; i < ctx->organ_count; i++) {
        state.total_energy += ctx->d_organs[i].energy_output;
    }
    
    float total_pressure = 0.0f;
    for (uint32_t i = 0; i < ctx->transport_count; i++) {
        total_pressure += ctx->d_transports[i].pressure;
    }
    if (ctx->transport_count > 0 && ctx->transport_count <= 1024) {
        for (uint32_t i = 0; i < ctx->transport_count; i++) {
            state.pressure_map[i] = ctx->d_transports[i].pressure;
        }
    }
    state.is_alive = 1;
    for (uint32_t i = 0; i < ctx->segment_count; i++) {
        if (ctx->d_segments[i].is_fractured) {
            state.is_alive = 0;
            break;
        }
    }
    return state;
}

// Fractal: server-level Skelly
void skelly_api_create_server_skelly(SkellyAPIContext ctx, uint32_t server_id) {
    // Create a Skelly instance representing server components
    uint32_t instance_idx = skelly_api_create_creature(ctx, server_id, 0, 0, 0, "server");
    // Modify to represent: compute = bones, processes = organs, bandwidth = muscles, latency = turgor
}

// Cleanup
void skelly_api_cleanup(SkellyAPIContext ctx) {
    if (!ctx) return;
    delete[] ctx->d_instances;
    delete[] ctx->d_bones;
    delete[] ctx->d_segments;
    delete[] ctx->d_muscles;
    delete[] ctx->d_strands;
    delete[] ctx->d_organs;
    delete[] ctx->d_transports;
    delete[] ctx->d_fluids;
    delete ctx;
}

// Main entry point
extern "C" void skelly_api_main(
    SkellyAPIContext ctx,
    float dt,
    int mode  // 0 = step, 1 = create creature, 2 = get state
) {
    switch (mode) {
        case 0:
            skelly_api_step(ctx, dt);
            break;
        case 1:
            skelly_api_create_creature(ctx, 0, 0, 0, 0, "creature");
            break;
        case 2: {
            SystemState state = skelly_api_get_system_state(ctx);
            (void)state;  // Use state as needed
            break;
        }
    }
}
