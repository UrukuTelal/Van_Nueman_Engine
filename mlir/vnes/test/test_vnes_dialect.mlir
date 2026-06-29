// RUN: mlir-opt %s -verify-diagnostics -split-input-file

// Test 1: Basic lattice + cell_field round-trip
// CHECK-LABEL: func @test_lattice_roundtrip
func.func @test_lattice_roundtrip() -> tensor<8x8x8xf32> {
    %lattice = vnes.lattice tensor<8x8x8x!vnes.cell>
    // CHECK: vnes.lattice tensor<8x8x8x!vnes.cell>
    %field = vnes.cell_field %lattice field("water") : tensor<8x8x8x!vnes.cell> -> tensor<8x8x8xf32>
    // CHECK: vnes.cell_field {{.*}} field("water") : tensor<8x8x8x!vnes.cell> -> tensor<8x8x8xf32>
    return %field : tensor<8x8x8xf32>
}

// -----

// Test 2: darcy_flux round-trip
// CHECK-LABEL: func @test_darcy_roundtrip
func.func @test_darcy_roundtrip() -> tensor<4x4xf32> {
    %water = arith.constant dense<1.0> : tensor<4x4xf32>
    %cond = arith.constant dense<0.5> : tensor<4x4xf32>
    // CHECK: vnes.darcy_flux {{.*}} alpha(0.01)
    %flux = vnes.darcy_flux %water, %cond alpha(0.01) : tensor<4x4xf32>
    return %flux : tensor<4x4xf32>
}

// -----

// Test 3: monod_growth round-trip
// CHECK-LABEL: func @test_monod_roundtrip
func.func @test_monod_roundtrip() -> tensor<4x4xf32> {
    %bio = arith.constant dense<2.0> : tensor<4x4xf32>
    %nut = arith.constant dense<1.0> : tensor<4x4xf32>
    // CHECK: vnes.monod_growth {{.*}} uptake(0.5) half(0.1)
    %g = vnes.monod_growth %bio, %nut uptake(0.5) half(0.1) : tensor<4x4xf32>
    return %g : tensor<4x4xf32>
}

// -----

// Test 4: decay round-trip
// CHECK-LABEL: func @test_decay_roundtrip
func.func @test_decay_roundtrip() -> tensor<4x4xf32> {
    %det = arith.constant dense<3.0> : tensor<4x4xf32>
    // CHECK: vnes.decay {{.*}} rate(0.001)
    %d = vnes.decay %det rate(0.001) : tensor<4x4xf32>
    return %d : tensor<4x4xf32>
}

// -----

// Test 5: Verify diagnostics on bad lattice type
// expected-error@+1 {{result must be a ranked tensor of !vnes.cell}}
func.func @test_bad_lattice() {
    %bad = "vnes.lattice"() : () -> tensor<4x4xf32>
    return
}
