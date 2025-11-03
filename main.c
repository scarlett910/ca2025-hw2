#include <stdbool.h>
#include <stdint.h>
#include <stddef.h> // Cần thiết cho kiểu size_t


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

#define TEST_OUTPUT(msg, length) printstr(msg, length)

#define TEST_LOGGER(msg)                     \
    {                                        \
        char _msg[] = msg;                   \
        TEST_OUTPUT(_msg, sizeof(_msg) - 1); \
    }

extern uint64_t get_cycles(void);
extern uint64_t get_instret(void);

extern void hanoi_asm(void);

/* Bare metal memcpy implementation */
void *memcpy(void *dest, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *) dest;
    const uint8_t *s = (const uint8_t *) src;
    while (n--)
        *d++ = *s++;
    return dest;
}

/* Software division for RV32I (no M extension) */
static unsigned long udiv(unsigned long dividend, unsigned long divisor)
{
    if (divisor == 0)
        return 0;

    unsigned long quotient = 0;
    unsigned long remainder = 0;

    for (int i = 31; i >= 0; i--) {
        remainder <<= 1;
        remainder |= (dividend >> i) & 1;

        if (remainder >= divisor) {
            remainder -= divisor;
            quotient |= (1UL << i);
        }
    }

    return quotient;
}

static unsigned long umod(unsigned long dividend, unsigned long divisor)
{
    if (divisor == 0)
        return 0;

    unsigned long remainder = 0;

    for (int i = 31; i >= 0; i--) {
        remainder <<= 1;
        remainder |= (dividend >> i) & 1;

        if (remainder >= divisor) {
            remainder -= divisor;
        }
    }

    return remainder;
}

/* Software multiplication for RV32I (no M extension) */
static uint32_t umul(uint32_t a, uint32_t b)
{
    uint32_t result = 0;
    while (b) {
        if (b & 1)
            result += a;
        a <<= 1;
        b >>= 1;
    }
    return result;
}

/* Provide __mulsi3 for GCC */
uint32_t __mulsi3(uint32_t a, uint32_t b)
{
    return umul(a, b);
}

/* Simple integer to hex string conversion */
static void print_hex(unsigned long val)
{
    char buf[20];
    char *p = buf + sizeof(buf) - 1;
    *p = '\n';
    p--;

    if (val == 0) {
        *p = '0';
        p--;
    } else {
        while (val > 0) {
            int digit = val & 0xf;
            *p = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
            p--;
            val >>= 4;
        }
    }

    p++;
    printstr(p, (buf + sizeof(buf) - p));
}

/* Simple integer to decimal string conversion */
static void print_dec(unsigned long val)
{
    char buf[20];
    char *p = buf + sizeof(buf) - 1;
    *p = '\n';
    p--;

    if (val == 0) {
        *p = '0';
        p--;
    } else {
        while (val > 0) {
            *p = '0' + umod(val, 10);
            p--;
            val = udiv(val, 10);
        }
    }

    p++;
    printstr(p, (buf + sizeof(buf) - p));
}

/* ================================================
 * HOMEWORK 1: UF8 IMPLEMENTATION
 * ================================================ */
typedef uint8_t uf8;

static inline unsigned clz(uint32_t x) {
    int n = 32, c = 16;
    do {
        uint32_t y = x >> c;
        if (y) {
            n -= c;
            x = y;
        }
        c >>= 1;
    } while (c);
    return n - x;
}

uint32_t uf8_decode(uf8 fl) {
    uint32_t mantissa = fl & 0x0f;
    uint8_t exponent = fl >> 4;
    uint32_t offset = (0x7FFF >> (15 - exponent)) << 4;
    return (mantissa << exponent) + offset;
}

uf8 uf8_encode(uint32_t value) {
    if (value < 16)
        return value;
    int lz = clz(value);
    int msb = 31 - lz;
    uint8_t exponent = 0;
    uint32_t overflow = 0;
    if (msb >= 5) {
        exponent = msb - 4;
        if (exponent > 15)
            exponent = 15;
        for (uint8_t e = 0; e < exponent; e++)
            overflow = (overflow << 1) + 16;
        while (exponent > 0 && value < overflow) {
            overflow = (overflow - 16) >> 1;
            exponent--;
        }
    }
    while (exponent < 15) {
        uint32_t next_overflow = (overflow << 1) + 16;
        if (value < next_overflow)
            break;
        overflow = next_overflow;
        exponent++;
    }
    uint8_t mantissa = (value - overflow) >> exponent;
    return (exponent << 4) | mantissa;
}

//=======================TOWER OF HANOI=======================
static bool test_hanoi_assembly(void) {
    // Call the assembly function
    // It will print the moves and return
    hanoi_asm();
    
    // If we reach here, the function completed
    return true;
}

/* ================================================
 * TEST SUITE CHO HW1
 * ================================================ */

static bool test_uf8_roundtrip(void) {
    int32_t previous_value = -1;
    bool passed = true;

    for (int i = 0; i < 256; i++) {
        uint8_t fl = i;
        int32_t value = uf8_decode(fl);
        uint8_t fl2 = uf8_encode(value);

        if (fl != fl2) {
            TEST_LOGGER("FAIL: "); print_hex(fl);
            TEST_LOGGER("  produces value "); print_dec(value);
            TEST_LOGGER("  but encodes back to "); print_hex(fl2);
            passed = false;
        }

        if (value <= previous_value) {
            TEST_LOGGER("FAIL: "); print_hex(fl);
            TEST_LOGGER("  value "); print_dec(value);
            TEST_LOGGER("  <= previous_value "); print_dec(previous_value);
            passed = false;
        }
        previous_value = value;
    }
    return passed;
}

/* ================================================
 * HÀM MAIN() CHÍNH
 * ================================================ */
int main(void) {
    uint64_t start_cycles, end_cycles, cycles_elapsed;
    uint64_t start_instret, end_instret, instret_elapsed;

    TEST_LOGGER("=== Start Test Suite for Tower of Hanoi ===\n");

    // Lấy thời gian bắt đầu
    start_cycles = get_cycles();
    start_instret = get_instret();

    // Chạy test suite
    bool all_passed_hanoi = test_hanoi_assembly();

    // Lấy thời gian kết thúc
    end_cycles = get_cycles();
    end_instret = get_instret();

    // Tính toán
    cycles_elapsed = end_cycles - start_cycles;
    instret_elapsed = end_instret - start_instret;

    // In kết quả test
    if (all_passed_hanoi) {
        TEST_LOGGER("== Tower of Hanoi: All tests PASSED ==\n");
    } else {
        TEST_LOGGER("== Tower of Hanoi: FAILED ==\n");
    }

    // In kết quả hiệu năng
    TEST_LOGGER("Cycles: ");
    print_dec((unsigned long) cycles_elapsed);
    TEST_LOGGER("Instructions: ");
    print_dec((unsigned long) instret_elapsed);
    TEST_LOGGER("\n=== All Tests Completed ===\n");

    TEST_LOGGER("=== Start Test Suite for Homework 1: UF8 ===\n");

    // Lấy thời gian bắt đầu
    start_cycles = get_cycles();
    start_instret = get_instret();

    // Chạy test suite
    bool all_passed_uf8 = test_uf8_roundtrip();

    // Lấy thời gian kết thúc
    end_cycles = get_cycles();
    end_instret = get_instret();

    // Tính toán
    cycles_elapsed = end_cycles - start_cycles;
    instret_elapsed = end_instret - start_instret;

    // In kết quả test
    if (all_passed_uf8) {
        TEST_LOGGER("== Homework 1: All tests PASSED ==\n");
    } else {
        TEST_LOGGER("== Homework 1: FAILED ==\n");
    }

    // In kết quả hiệu năng
    TEST_LOGGER("Cycles: ");
    print_dec((unsigned long) cycles_elapsed);
    TEST_LOGGER("Instructions: ");
    print_dec((unsigned long) instret_elapsed);
    TEST_LOGGER("\n=== All Tests Completed ===\n");

    return 0; // Sẽ thoát về start.S, sau đó gọi ecall (sys_exit)
}