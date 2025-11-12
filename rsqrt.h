#ifndef RSQRT_H
#define RSQRT_H

#include <stdint.h>
#include <stdbool.h>

/**
 * Fast reciprocal square root: computes 65536/√x (16-bit fractional)
 * @param x Input value (must be > 0)
 * @return Approximation of 65536/√x, or 0 if x = 0
 */
uint32_t rsqrt(uint32_t x);

/**
 * Calculate 3D distance using fast reciprocal square root
 * Returns √(dx² + dy² + dz²)
 */
uint32_t fast_distance_3d(uint32_t dx, uint32_t dy, uint32_t dz);

/**
 * Test functions
 */
bool test_rsqrt_accuracy(void);
bool test_fast_distance_3d(void);

#endif // RSQRT_H
