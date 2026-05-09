__kernel void test_kernel(__global float* input, __global float* output, uint count) {
    uint id = get_global_id(0);
    if (id >= count) return;
    output[id] = input[id] * 2.0f;
}