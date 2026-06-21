#version 450
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable

// ── FLL Glyph Fragment Shader ───────────────────────────────────────
// Rasterization-path variant of fll_glyph.comp.
// Push constants + node buffer layout identical to compute shader.

layout(push_constant) uniform PushConstants {
    vec2  view_center;
    float zoom_level;
    float time;

    float depth_blend;
    float depth_fade;
    uint  active_depth;
    uint  max_display_depth;

    float resonance_threshold;
    float glyph_intensity;
    float color_hue_shift;
    uint  node_count;

    float debug_show_edges;
    float debug_show_quantum;
    float pad0;
    float pad1;
} pc;

// ── Node buffer structs (identical to compute shader) ───────────────
struct GeometricCoefficients {
    vec2  center;
    float rotation;
    float scale;
    vec4  harmonic_a;
    vec4  harmonic_p;
    float twist;
    float asymmetry;
    float inner_radius;
    float outer_radius;
    uint  symmetry_n;
    float padding0;
    float padding1;
    float padding2;
};

struct FractalNodeGPU {
    uint64_t uid;
    uint  depth;
    uint  child_count;
    GeometricCoefficients geo;
};

layout(binding = 0, std430) readonly buffer NodeBuffer {
    FractalNodeGPU nodes[];
} node_buf;

layout(location = 0) in vec2 frag_uv;
layout(location = 0) out vec4 out_color;

// ── SDF helpers ─────────────────────────────────────────────────────

vec2 rotate(vec2 p, float rad) {
    float c = cos(rad);
    float s = sin(rad);
    return vec2(p.x * c - p.y * s, p.x * s + p.y * c);
}

float radial_harmonic_dist(float theta, vec4 amps, vec4 phases) {
    return amps.x * cos(1.0 * theta + phases.x)
         + amps.y * cos(2.0 * theta + phases.y)
         + amps.z * cos(3.0 * theta + phases.z)
         + amps.w * cos(4.0 * theta + phases.w);
}

float symmetry_fold(float theta, uint n) {
    if (n == 0) return theta;
    float period = 6.283185307 / float(n);
    return abs(mod(theta, period) - period * 0.5);
}

float glyph_sdf(vec2 p, GeometricCoefficients geo) {
    vec2 lp = p - geo.center;
    lp = rotate(lp, geo.rotation);

    float r = length(lp);
    float theta = atan(lp.y, lp.x);
    theta = symmetry_fold(theta, geo.symmetry_n);

    float base_r = geo.outer_radius
                 + radial_harmonic_dist(theta, geo.harmonic_a, geo.harmonic_p) * 0.3;

    float twist_angle = geo.twist * r * 2.0;
    theta += twist_angle;
    theta = symmetry_fold(theta, geo.symmetry_n);

    float twisted_r = geo.outer_radius
                    + radial_harmonic_dist(theta, geo.harmonic_a, geo.harmonic_p) * 0.3;
    twisted_r *= 1.0 + geo.asymmetry * cos(theta);

    float d = r - twisted_r;
    float inner = geo.inner_radius - r;
    d = max(d, inner);
    return d;
}

vec3 depth_color(uint depth, float hue_shift) {
    float t = float(depth) * 0.1 + hue_shift;
    float r = 0.5 + 0.5 * cos(6.2831853 * (t + 0.0));
    float g = 0.5 + 0.5 * cos(6.2831853 * (t + 0.333));
    float b = 0.5 + 0.5 * cos(6.2831853 * (t + 0.667));
    return vec3(r, g, b);
}

float depth_transition_weight(uint depth) {
    float d = float(depth);
    float adepth = float(pc.active_depth);
    float dist = abs(d - adepth);
    if (dist > 1.0) return 0.0;
    float blend = 1.0 - dist;
    if (dist < 1.0 && d > adepth) {
        blend *= pc.depth_blend;
    }
    return blend * pc.depth_fade;
}

void main() {
    vec2 uv = frag_uv;

    vec3 accum_color = vec3(0.0);
    float accum_weight = 0.0;
    float min_dist = 1e10;

    uint count = min(pc.node_count, uint(node_buf.nodes.length()));
    for (uint i = 0; i < count; i++) {
        FractalNodeGPU node = node_buf.nodes[i];
        if (node.depth > pc.max_display_depth) continue;

        float w = depth_transition_weight(node.depth);
        if (w < 0.01) continue;

        float d = glyph_sdf(uv, node.geo);

        float glyph = 1.0 - smoothstep(0.0, 0.02 / pc.zoom_level, abs(d));
        if (glyph > 0.01 && d < 0.05 / pc.zoom_level) {
            vec3 dc = depth_color(node.depth, pc.color_hue_shift);
            float intensity = pc.glyph_intensity * glyph * w;
            accum_color += dc * intensity;
            accum_weight += intensity;
        }

        min_dist = min(min_dist, abs(d));
    }

    if (accum_weight > 0.0) {
        out_color.rgb = accum_color / accum_weight;
        out_color.a = min(1.0, accum_weight);
    } else {
        float glow = exp(-min_dist * min_dist * 100.0 * pc.zoom_level) * 0.15;
        out_color = vec4(0.02, 0.02, 0.05 + glow, 1.0);
    }
}
