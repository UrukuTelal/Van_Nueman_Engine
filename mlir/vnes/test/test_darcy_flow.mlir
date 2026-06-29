// RUN: vn-mlir-opt %s -pass-pipeline="builtin.module(func.func(vnes-fuse-solvers))" -split-input-file | FileCheck %s

// Basic test: a lattice with darcy flux + monod growth + decay
// CHECK-LABEL: func @test_fuse_solvers
func.func @test_fuse_solvers() -> tensor<16x16x16xf32> {
    %dim = arith.constant dense<[16, 16, 16]> : tensor<3xi32>
    %lattice = vnes.lattice tensor<16x16x16x!vnes.cell>

    // Extract fields
    %water = vnes.cell_field %lattice field("water") : tensor<16x16x16x!vnes.cell> -> tensor<16x16x16xf32>
    %conductivity = vnes.cell_field %lattice field("conductivity") : tensor<16x16x16x!vnes.cell> -> tensor<16x16x16xf32>
    %biomass = vnes.cell_field %lattice field("biomass") : tensor<16x16x16x!vnes.cell> -> tensor<16x16x16xf32>
    %nutrient = vnes.cell_field %lattice field("nutrient") : tensor<16x16x16x!vnes.cell> -> tensor<16x16x16xf32>
    %detritus = vnes.cell_field %lattice field("detritus") : tensor<16x16x16x!vnes.cell> -> tensor<16x16x16xf32>

    // Solver ops
    // CHECK-NOT: vnes.darcy_flux
    // CHECK-NOT: vnes.monod_growth
    // CHECK-NOT: vnes.decay
    // CHECK: vnes.fused_update
    %flux = vnes.darcy_flux %water, %conductivity alpha(0.01) : tensor<16x16x16xf32>
    %growth = vnes.monod_growth %biomass, %nutrient uptake(0.5) half(0.1) : tensor<16x16x16xf32>
    %decay = vnes.decay %detritus rate(0.001) : tensor<16x16x16xf32>

    // Combine results
    %0 = arith.addf %flux, %growth : tensor<16x16x16xf32>
    %1 = arith.subf %0, %decay : tensor<16x16x16xf32>
    return %1 : tensor<16x16x16xf32>
}
