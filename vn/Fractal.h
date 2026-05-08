#ifndef VN_FRACTAL_H
#define VN_FRACTAL_H

// Fractal attribute support for Van Nueman
// Used by vncc compiler for SVO structures

#ifndef __attribute__
#define __attribute__(x)
#endif

// Marks a structure as having fractal (recursive) properties
#define FRACTAL __attribute__((annotate("fractal")))

// Marks a function for the SVO kernel
#define SVO_KERNEL __attribute__((annotate("svo_kernel")))

#endif // VN_FRACTAL_H
