// RUN: vn-mlir-opt %s -pass-pipeline="builtin.module(func.func(vnes-lower-to-linalg))" -split-input-file | FileCheck %s

// CHECK-LABEL: func @test_lower_darcy
func.func @test_lower_darcy() -> tensor<4x4xf32> {
    %water = arith.constant dense<2.0> : tensor<4x4xf32>
    %cond = arith.constant dense<0.5> : tensor<4x4xf32>
    // CHECK: linalg.generic
    // CHECK-NOT: vnes.darcy_flux
    %f = vnes.darcy_flux %water, %cond alpha(0.01) : tensor<4x4xf32>
    return %f : tensor<4x4xf32>
}

// -----

// CHECK-LABEL: func @test_lower_monod
func.func @test_lower_monod() -> tensor<4x4xf32> {
    %bio = arith.constant dense<2.0> : tensor<4x4xf32>
    %nut = arith.constant dense<1.0> : tensor<4x4xf32>
    // CHECK: linalg.generic
    // CHECK-NOT: vnes.monod_growth
    %g = vnes.monod_growth %bio, %nut uptake(0.5) half(0.1) : tensor<4x4xf32>
    return %g : tensor<4x4xf32>
}

// -----

// CHECK-LABEL: func @test_lower_decay
func.func @test_lower_decay() -> tensor<4x4xf32> {
    %det = arith.constant dense<3.0> : tensor<4x4xf32>
    // CHECK: linalg.generic
    // CHECK-NOT: vnes.decay
    %d = vnes.decay %det rate(0.001) : tensor<4x4xf32>
    return %d : tensor<4x4xf32>
}
