#include "rsqrt.h"
#include <stddef.h>

// Import print functions from main.c
extern uint32_t umul(uint32_t a, uint32_t b);
extern unsigned long udiv(unsigned long dividend, unsigned long divisor);
void print_dec_inline(unsigned long val);

/* Software 64-bit multiplication using 32-bit multiply */
uint64_t __muldi3(uint64_t a, uint64_t b) {
    uint32_t a_lo = (uint32_t)(a & 0xFFFFFFFF);
    uint32_t a_hi = (uint32_t)(a >> 32);
    uint32_t b_lo = (uint32_t)(b & 0xFFFFFFFF);
    uint32_t b_hi = (uint32_t)(b >> 32);
    
    // Step 1: Compute a_lo * b_lo as 64-bit
    // Use long multiplication: split into 16-bit chunks
    uint32_t a_lo_lo = a_lo & 0xFFFF;
    uint32_t a_lo_hi = a_lo >> 16;
    uint32_t b_lo_lo = b_lo & 0xFFFF;
    uint32_t b_lo_hi = b_lo >> 16;
    
    uint64_t p0 = (uint64_t)umul(a_lo_lo, b_lo_lo);
    uint64_t p1 = (uint64_t)umul(a_lo_lo, b_lo_hi) << 16;
    uint64_t p2 = (uint64_t)umul(a_lo_hi, b_lo_lo) << 16;
    uint64_t p3 = (uint64_t)umul(a_lo_hi, b_lo_hi) << 32;
    
    uint64_t result = p0 + p1 + p2 + p3;
    
    // Step 2: Add cross products
    uint32_t mid1 = umul(a_hi, b_lo);
    result += ((uint64_t)mid1 << 32);
    
    uint32_t mid2 = umul(a_lo, b_hi);
    result += ((uint64_t)mid2 << 32);
    
    return result;
}

#define TEST_OUTPUT(msg, length) printstr(msg, length)
#define TEST_LOGGER(msg)                     \
    {                                        \
        char _msg[] = msg;                   \
        TEST_OUTPUT(_msg, sizeof(_msg) - 1); \
    }

#define printstr(ptr, length)                   \
    do {                                        \
        asm volatile(                           \
            "add a7, x0, 0x40;"                 \
            "add a0, x0, 0x1;" /* stdout */     \
            "add a1, x0, %0;"                   \
            "mv a2, %1;" /* length character */ \
            "ecall;"                            \
            :                                   \
            : "r"(ptr), "r"(length)             \
            : "a0", "a1", "a2", "a7");          \
    } while (0)

// Lookup table for initial approximation (32 entries)
static const uint32_t rsqrt_table[32] = {
    65536, 46341, 32768, 23170, 16384,
    11585, 8192,  5793,  4096,  2896,
    2048,  1448,  1024,  724,   512,
    362,   256,   181,   128,   90,
    64,    45,    32,    23,    16,
    11,    8,     6,     4,     3,
    2,     1
};
static inline unsigned clz(uint32_t x) {
    if (x == 0) return 32;
    unsigned n = 0;
    uint32_t bit = 0x80000000;
    while ((x & bit) == 0) {
        n++;
        bit >>= 1;
    }
    return n;
}

static inline uint32_t newton_step(uint32_t x, uint32_t y) {
    uint32_t y2 = ((uint64_t)y * y) >> 16;
    uint32_t xy2 = ((uint64_t)x * y2) >> 16;
    uint32_t three_scaled = 3u << 16;
    
    if (xy2 > three_scaled) {
        xy2 = three_scaled;
    }
    
    uint32_t factor = three_scaled - xy2;
    uint64_t result = (uint64_t)y * factor;
    
    return (uint32_t)(result >> 17);
}

uint32_t rsqrt(uint32_t x) {
    if (x == 0) return 0xFFFFFFFF;
    
    // Tìm exponent (MSB position)
    uint32_t exp = 31 - clz(x);
    
    // Lookup table
    uint32_t y_base = rsqrt_table[exp];
    uint32_t y_next = (exp == 31) ? 1 : rsqrt_table[exp + 1];
    
    // Linear interpolation - DÙNG SHIFT thay vì CHIA!
    uint32_t delta = y_base - y_next;
    uint64_t frac_num_scaled = (uint64_t)(x - (1U << exp)) << 16;
    uint32_t frac = (uint32_t)(frac_num_scaled >> exp);
    uint32_t interp = (uint32_t)(((uint64_t)delta * frac) >> 16);
    uint32_t y = y_base - interp;
    
    // Newton iteration 1
    uint32_t y2 = (uint32_t)(((uint64_t)y * y) >> 16);
    uint32_t xy2 = (uint32_t)((uint64_t)x * y2);
    y = (uint32_t)(((uint64_t)y * ((3U << 16) - xy2)) >> 17);
    
    // Newton iteration 2
    y2 = (uint32_t)(((uint64_t)y * y) >> 16);
    xy2 = (uint32_t)((uint64_t)x * y2);
    y = (uint32_t)(((uint64_t)y * ((3U << 16) - xy2)) >> 17);
    
    return y;
}

uint32_t fast_distance_3d(uint32_t dx, uint32_t dy, uint32_t dz) {
    uint64_t dist_sq = (uint64_t)dx * dx + 
                       (uint64_t)dy * dy + 
                       (uint64_t)dz * dz;
    
    if (dist_sq > 0xFFFFFFFF) {
        dist_sq >>= 16;
    }
    
    uint32_t dist_sq_32 = (uint32_t)dist_sq;
    if (dist_sq_32 == 0) return 0;
    
    uint32_t inv_sqrt = rsqrt(dist_sq_32);
    uint64_t dist = ((uint64_t)dist_sq_32 * inv_sqrt) >> 16;
    
    return (uint32_t)dist;
}

/* ================================================
 * TEST FUNCTIONS
 * ================================================ */

bool test_rsqrt_accuracy(void) {
    TEST_LOGGER("\n--- Testing rsqrt accuracy ---\n");
    
    bool passed = true;
    
    struct {
        uint32_t input;
        uint32_t expected_approx;
    } test_cases[] = {
        {1, 65536},
        {4, 32768},
        {16, 16384},
        {64, 8192},
        {256, 4096}
    };
    
    for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
        uint32_t result = rsqrt(test_cases[i].input);
        uint32_t expected = test_cases[i].expected_approx;
        
        int32_t error = (int32_t)result - (int32_t)expected;
        uint32_t abs_error = (error < 0) ? -error : error;
        
        uint32_t pct_error = (abs_error * 100) / expected;
        
        TEST_LOGGER("rsqrt(");
        print_dec_inline(test_cases[i].input);
        TEST_LOGGER(") = ");
        print_dec_inline(result);
        TEST_LOGGER(" (expected ~");
        print_dec_inline(expected);
        TEST_LOGGER(", error ");
        print_dec_inline(pct_error);
        TEST_LOGGER("%)\n");
        
        if (pct_error > 10) {
            TEST_LOGGER("  FAIL: Error too large!\n");
            passed = false;
        }
    }
    
    return passed;
}

bool test_fast_distance_3d(void) {
    TEST_LOGGER("\n--- Testing fast_distance_3d ---\n");
    
    bool passed = true;
    
    struct {
        uint32_t dx, dy, dz;
        uint32_t expected_approx;
    } test_cases[] = {
        {3, 4, 0, 5},
        {5, 12, 0, 13},
        {8, 15, 0, 17}
    };
    
    for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
        uint32_t result = fast_distance_3d(test_cases[i].dx, 
                                          test_cases[i].dy, 
                                          test_cases[i].dz);
        uint32_t expected = test_cases[i].expected_approx;
        
        TEST_LOGGER("distance(");
        print_dec_inline(test_cases[i].dx);
        TEST_LOGGER(", ");
        print_dec_inline(test_cases[i].dy);
        TEST_LOGGER(", ");
        print_dec_inline(test_cases[i].dz);
        TEST_LOGGER(") = ");
        print_dec_inline(result);
        TEST_LOGGER(" (expected ~");
        print_dec_inline(expected);
        TEST_LOGGER(")\n");
        
        int32_t error = (int32_t)result - (int32_t)expected;
        uint32_t abs_error = (error < 0) ? -error : error;
        
        if (expected > 0) {
            uint32_t pct_error = (abs_error * 100) / expected;
            if (pct_error > 15) {
                TEST_LOGGER("  FAIL: Error too large!\n");
                passed = false;
            }
        }
    }
    
    return passed;
}
