#version 450
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

layout(binding = 2) uniform sampler2D computeOutput;

void main() {
    // Show compute output directly (blue gradient = success)
    vec4 color = texture(computeOutput, uv);
    if (color.r > 0.01 || color.g > 0.01 || color.b > 0.01) {
        outColor = vec4(color.rgb, 1.0); // Show compute output
    } else {
        outColor = vec4(1.0, 0.0, 0.0, 1.0); // Red = no compute
    }
}
