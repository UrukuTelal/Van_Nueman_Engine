// test_main.cpp - Main test runner

#include <cstdio>
#include "test_entity.h"
#include "test_pillar_coupling.h"
#include "test_neuroevolution.h"
#include "test_scaled_int.h"
#include "test_persistence.h"
#include "test_vulkan.h"
#include "test_chunk.h"
#include "test_spatial_hash.h"

int main() {
    printf("Van Nueman Test Suite\n");
    printf("=====================\n\n");

    // Run all tests
    run_entity_tests();
    run_pillar_coupling_tests();
    run_neuroevolution_tests();
    run_scaled_int_tests();
    run_persistence_tests();
    run_vulkan_tests();
    run_chunk_tests();
    run_spatial_hash_tests();

    printf("All tests completed.\n");
    return 0;
}
