#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <cmath>

#define SCREEN_WIDTH 1920
#define SCREEN_HEIGHT 1080
#define MAX_RAY_STEPS 256
#define SVO_MAX_LODS 8

// Float3 helpers
__device__ float3 make_float3(float x, float y, float z) {
    float3 r; r.x = x; r.y = y; r.z = z; return r;
}
__device__ float3 operator+(float3 a, float3 b) { return make_float3(a.x+b.x, a.y+b.y, a.z+b.z); }
__device__ float3 operator-(float3 a, float3 b) { return make_float3(a.x-b.x, a.y-b.y, a.z-b.z); }
__device__ float3 operator*(float3 a, float s) { return make_float3(a.x*s, a.y*s, a.z*s); }
__device__ float dot(float3 a, float3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
__device__ float3 cross(float3 a, float3 b) {
    return make_float3(a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x);
}
__device__ float length(float3 v) { return sqrtf(dot(v, v)); }
__device__ float3 normalize(float3 v) { float l = length(v); return l > 0 ? v * (1.0f/l) : v; }

// Ray structure
struct Ray {
    float3 origin;
    float3 direction;
};

// Camera
struct Camera {
    float3 position;
    float3 forward;
    float3 right;
    float3 up;
    float fov;
    float aspect;
};

// SVO Node (simplified for rendering)
struct SVO_Node_Render {
    uint32_t children[8];
    uint8_t valid_mask;
    uint8_t material;
    uint8_t r, g, b, a;
};

// Rendering constants
#define MAT_OCTAHEDRON 0
#define MAT_CUBE 1
#define MAT_POLYHEDRA 2

// SVO traversal + ray march (from architecture spec)
__device__ float svo_ray_intersect(SVO_Node_Render* nodes, uint32_t node_idx,
                                    Ray ray, float3 box_min, float3 box_max,
                                    float* t_out, float3* normal_out) {
    // Simple AABB test first
    float t_min = 0.0f, t_max = 1e10f;
    float3 inv_dir = make_float3(1.0f / (ray.direction.x + 1e-8f),
                                  1.0f / (ray.direction.y + 1e-8f),
                                  1.0f / (ray.direction.z + 1e-8f));

    float3 t1 = (box_min - ray.origin) * inv_dir;
    float3 t2 = (box_max - ray.origin) * inv_dir;
    float3 t_min_v = make_float3(fminf(t1.x, t2.x), fminf(t1.y, t2.y), fminf(t1.z, t2.z));
    float3 t_max_v = make_float3(fmaxf(t1.x, t2.x), fmaxf(t1.y, t2.y), fmaxf(t1.z, t2.z));

    t_min = fmaxf(fmaxf(t_min_v.x, t_min_v.y), t_min_v.z);
    t_max = fminf(fminf(t_max_v.x, t_max_v.y), t_max_v.z);

    if (t_min > t_max || t_max < 0) return 0.0f;

    *t_out = t_min;

    // Calculate normal (simplified)
    float3 hit = ray.origin + ray.direction * t_min;
    float3 center = (box_min + box_max) * 0.5f;
    float3 local = hit - center;
    // Determine which face was hit
    float3 abs_local = make_float3(fabsf(local.x), fabsf(local.y), fabsf(local.z));
    if (abs_local.x >= abs_local.y && abs_local.x >= abs_local.z)
        *normal_out = make_float3(local.x > 0 ? 1 : -1, 0, 0);
    else if (abs_local.y >= abs_local.x && abs_local.y >= abs_local.z)
        *normal_out = make_float3(0, local.y > 0 ? 1 : -1, 0);
    else
        *normal_out = make_float3(0, 0, local.z > 0 ? 1 : -1);

    return t_max - t_min;  // "depth" of intersection
}

// SVO traversal for ray (recursive in kernel using stack)
__device__ float trace_svo(SVO_Node_Render* nodes, uint32_t root_idx,
                            Ray ray, float3 chunk_origin, float chunk_size,
                            float* t_out, float3* normal_out, uint8_t* material) {
    // Simplified: just check root node AABB
    float3 box_min = chunk_origin;
    float3 box_max = chunk_origin + make_float3(chunk_size, chunk_size, chunk_size);

    float t = 0;
    float3 n = make_float3(0, 0, 0);
    float depth = svo_ray_intersect(nodes, root_idx, ray, box_min, box_max, &t, &n);

    if (depth > 0) {
        *t_out = t;
        *normal_out = n;
        *material = nodes[root_idx].material;
        return depth;
    }
    return 0.0f;
}

// Main render kernel (1080p output)
__global__ void render_kernel(Camera cam, SVO_Node_Render* svo_nodes, uint32_t svo_root,
                               float3* output_color, uint32_t width, uint32_t height) {
    uint32_t px = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t py = blockIdx.y * blockDim.y + threadIdx.y;
    if (px >= width || py >= height) return;

    // Calculate ray direction
    float u = (px + 0.5f) / width;
    float v = (py + 0.5f) / height;
    u = (u * 2.0f - 1.0f) * cam.fov * cam.aspect;
    v = (v * 2.0f - 1.0f) * cam.fov;

    Ray ray;
    ray.origin = cam.position;
    ray.direction = normalize(cam.forward * 1.0f + cam.right * u + cam.up * v);

    // Trace
    float t = 0;
    float3 normal = make_float3(0, 0, 0);
    uint8_t mat = 0;
    float depth = trace_svo(svo_nodes, svo_root, ray, make_float3(0, 0, 0), 256.0f, &t, &normal, &mat);

    // Shading
    float3 color;
    if (depth > 0) {
        // Simple diffuse lighting
        float3 light_dir = normalize(make_float3(1, 1, 1));
        float diff = fmaxf(dot(normal, light_dir), 0.0f);

        // Material color
        float3 mat_color;
        if (mat == MAT_OCTAHEDRON) mat_color = make_float3(0.8f, 0.6f, 0.4f);
        else if (mat == MAT_CUBE) mat_color = make_float3(0.5f, 0.5f, 0.5f);
        else mat_color = make_float3(0.3f, 0.7f, 0.9f);  // POLYHEDRA

        color = mat_color * (0.2f + 0.8f * diff);
    } else {
        // Background
        color = make_float3(0.05f, 0.05f, 0.1f);
    }

    // Write output
    uint32_t idx = py * width + px;
    output_color[idx] = color;
}

// Delta compression for network (from architecture: "Delta compress changed SVO nodes")
__global__ void render_delta_compress_kernel(float3* current, float3* previous,
                                              uint8_t* delta_flags, uint32_t pixel_count) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= pixel_count) return;

    float3 c = current[idx];
    float3 p = previous[idx];
    float diff = fabsf(c.x - p.x) + fabsf(c.y - p.y) + fabsf(c.z - p.z);
    delta_flags[idx] = (diff > 0.01f) ? 1 : 0;
}
