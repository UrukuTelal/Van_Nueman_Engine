// test_main.cpp - Main test runner

#include <cstdio>
#include "test_entity.h"
#include "test_pillar_coupling.h"
#include "test_transform.cpp"
#include "test_neuroevolution.h"
#include "test_scaled_int.h"
#include "test_persistence.h"
#include "test_vulkan.h"
#include "test_chunk.h"
#include "test_spatial_hash.h"
#include "test_wht_fp.h"
#include "test_fracture.h"
#include "test_voxel_mesh.h"
#include "test_voxel_coupling.h"
#include "test_transform_pipeline.h"
#include "test_orbital.h"
#include "test_llm_worker.h"
#include "test_hopf_pid.h"
#include "test_quantum_amplitude.h"
#include "test_quantum_forward_pass.h"
#include "test_wht_tokenizer.h"
#include "test_thought_engine.h"
#include "test_cognitive_pipeline.h"
#include "test_bcc_thought_env.h"
#include "test_adversarial_fixes.h"
#include "test_bloch_cross_validate.h"
#include "test_fll.cpp"
#include "test_fugure.cpp"
#include "test_scale_router.cpp"
#include "test_thought_glyph_bridge.h"
#include "test_wht_audio.h"
#include "test_gpu_cpu_cross_validate.h"

int main() {
    setvbuf(stdout, NULL, _IONBF, 0);  // unbuffered stdout for test output visibility
    printf("Van Nueman Test Suite\n");
    printf("=====================\n\n");

    // Run all tests
    run_entity_tests();
    run_pillar_coupling_tests();
    run_transform_tests();
    run_neuroevolution_tests();
    run_scaled_int_tests();
    run_persistence_tests();
    run_vulkan_tests();
    run_chunk_tests();
    run_spatial_hash_tests();
    run_wht_fp_tests();
    run_fracture_tests();
    run_voxel_mesh_tests();
    run_voxel_coupling_tests();
    run_transform_pipeline_tests();
    run_orbital_tests();
    run_llm_worker_tests();
    run_hopf_pid_tests();
    run_quantum_amplitude_tests();
    run_quantum_forward_pass_tests();
    run_wht_tokenizer_tests();
    run_thought_engine_tests();
    run_cognitive_pipeline_tests();
    run_bcc_thought_env_tests();
    printf("AFTER bcc\n"); fflush(stdout);
    run_adversarial_fix_tests();
    printf("AFTER adversarial\n"); fflush(stdout);
    run_bloch_cross_validation_tests();
    printf("AFTER bloch_cross\n"); fflush(stdout);
    run_fll_tests();
    printf("AFTER fll\n"); fflush(stdout);
    vn::tests::test_thought_glyph_bridge_tests();
    run_fugure_tests();
    run_scale_router_tests();
    printf("AFTER glyph_bridge\n"); fflush(stdout);
    run_wht_audio_tests();
    printf("AFTER wht_audio\n"); fflush(stdout);
    run_gpu_cpu_cross_validation_tests();
    printf("AFTER gpu_cpu_cross\n"); fflush(stdout);

    printf("All tests completed.\n"); fflush(stdout);
    return 0;
}
