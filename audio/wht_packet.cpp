#include "wht_packet.h"
#include "wht.h"
#include <cstring>
#include <cmath>

// Full 32D WHT signal protocol initialization.
// Elements 0-15: Pillar State Vector (direct mapping)
// Elements 16-23: Harmonic resonance coefficients (H_n for n=0..7)
// Elements 24-31: Resonance cache data (phase offsets, decay rates)
//
// This eliminates non-deterministic noise from uninitialized elements
// and enables high-fidelity agent communication via the full 32D space.

// Pre-computed harmonic coefficients for elements 16-23
// These encode the first 8 harmonics of the Bloch sphere rotation:
// H_n = sin(n * pi/8) / (n + 1)  — decaying sinusoidal harmonics
static const float HARMONIC_COEFFS[8] = {
    0.0f,           // H_0: DC offset (always 0 for centered signal)
    0.382683432f,   // H_1: sin(pi/8) / 2
    0.353553391f,   // H_2: sin(2pi/8) / 3 = sin(pi/4) / 3
    0.270598050f,   // H_3: sin(3pi/8) / 4
    0.176776695f,   // H_4: sin(4pi/8) / 5 = sin(pi/2) / 5
    0.103323398f,   // H_5: sin(5pi/8) / 6
    0.053419166f,   // H_6: sin(6pi/8) / 7 = sin(3pi/4) / 7
    0.024009345f    // H_7: sin(7pi/8) / 8
};

// Resonance cache defaults for elements 24-31
// phi_phase: cumulative phase from cord sync_tau/sync_phi
// decay_rate: exponential decay constant per element
// cache_tag: 4-bit type tag + 4-bit version for protocol evolution
static const float RESONANCE_CACHE_DEFAULTS[8] = {
    0.1f,   // 24: cord phase offset tau
    0.05f,  // 25: cord phase offset phi
    0.995f, // 26: cord decay per tick
    0.5f,   // 27: initial coherence
    0.0f,   // 28: reserved 0
    0.0f,   // 29: reserved 1
    0.0f,   // 30: reserved 2
    0.0f    // 31: reserved 3
};

void encode_pillar_vector(const float pillars[NumPillars], float signal[WHT_N]) {
    int i;
    // Elements 0-15: Pillar State Vector
    for(i=0;i<NumPillars;i++) signal[i]=pillars[i];
    // Elements 16-23: Harmonic resonance coefficients
    // Encoded as: harmonic * pillar_energy_sum * damping_factor
    float pillar_energy = 0.0f;
    for(i=0;i<NumPillars;i++) pillar_energy += pillars[i] * pillars[i];
    pillar_energy = sqrtf(pillar_energy / (float)NumPillars);  // RMS
    for(i=0;i<8;i++) {
        signal[NumPillars + i] = HARMONIC_COEFFS[i] * pillar_energy;
    }
    // Elements 24-31: Resonance cache data (phi, tau, decay, coherence)
    for(i=0;i<8;i++) {
        signal[WHT_N - 8 + i] = RESONANCE_CACHE_DEFAULTS[i];
    }
    fwht(signal, WHT_LOG2_N);
}

void decode_pillar_vector(const float signal[WHT_N], float pillars[NumPillars]) {
    float temp[WHT_N];
    memcpy(temp, signal, WHT_N*sizeof(float));
    ifwht(temp, WHT_LOG2_N);
    int i;
    for(i=0;i<NumPillars;i++) pillars[i]=temp[i];
    // Remaining 16 elements carry harmonic + resonance data for
    // high-fidelity agent communication (accessible via raw signal access)
}
