#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(local_size_x = 8, local_size_y = 8) in;

layout(binding = 2, rgba8) uniform writeonly image2D outputImg;

void main() {
    ivec2 gid = ivec2(gl_GlobalInvocationID.xy);
    // Write solid BLUE to entire image
    imageStore(outputImg, gid, vec4(0.0, 0.0, 1.0, 1.0));
}
