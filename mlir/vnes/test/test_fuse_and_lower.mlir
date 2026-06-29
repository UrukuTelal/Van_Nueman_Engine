// RUN: vn-mlir-opt %s -pass-pipeline="builtin.module(func.func(vnes-fuse-solvers, vnes-lower-to-linalg))" -split-input-file | FileCheck %s

// Fusion + lowering: darcy + monod + decay fused into single linalg.generic
// CHECK-LABEL: func @test_fuse_lower
func.func @test_fuse_lower() -> tensor<16x16x16xf32> {
    %lattice = vnes.lattice tensor<16x16x16x!vnes.cell>
    %water = vnes.cell_field %lattice field("water") : tensor<16x16x16x!vnes.cell> -> tensor<16x16x16xf32>
    %cond = vnes.cell_field %lattice field("conductivity") : tensor<16x16x16x!vnes.cell> -> tensor<16x16x16xf32>
    %bio = vnes.cell_field %lattice field("biomass") : tensor<16x16x16x!vnes.cell> -> tensor<16x16x16xf32>
    %nut = vnes.cell_field %lattice field("nutrient") : tensor<16x16x16x!vnes.cell> -> tensor<16x16x16xf32>
    %det = vnes.cell_field %lattice field("detritus") : tensor<16x16x16x!vnes.cell> -> tensor<16x16x16xf32>

    // CHECK-NOT: vnes.darcy_flux
    // CHECK-NOT: vnes.monod_growth
    // CHECK-NOT: vnes.decay
    // CHECK-NOT: vnes.fused_update
    // CHECK: linalg.generic
    %flux = vnes.darcy_flux %water, %cond alpha(0.01) : tensor<16x16x16xf32>
    %growth = vnes.monod_growth %bio, %nut uptake(0.5) half(0.1) : tensor<16x16x16xf32>
    %decay = vnes.decay %det rate(0.001) : tensor<16x16x16xf32>

    %0 = arith.addf %flux, %growth : tensor<16x16x16xf32>
    %1 = arith.subf %0, %decay : tensor<16x16x16xf32>
    return %1 : tensor<16x16x16xf32>
}

// -----

// Lower only (no fusion needed): single decay op
// CHECK-LABEL: func @test_lower_only
func.func @test_lower_only() -> tensor<8x8xf32> {
    %det = arith.constant dense<2.0> : tensor<8x8xf32>
    // CHECK: linalg.generic
    // CHECK-NOT: vnes.decay
    %d = vnes.decay %det rate(0.01) : tensor<8x8xf32>
    return %d : tensor<8x8xf32>
}
