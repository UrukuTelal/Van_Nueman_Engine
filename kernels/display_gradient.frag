#version 450
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

void main() {
    // Direct gradient: blue at bottom, purple at top
    outColor = vec4(uv.x * 0.5, uv.y * 0.5, 0.8, 1.0);
}
