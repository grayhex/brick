/* The C library of the WebAssembly build (platform/sim/web/libc) against the host's libc: fmod bit for bit. */
#include "../../platform/sim/web/libc/libc_min.c"

#include "bk_test.h"

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static uint64_t rng_state = 0x9e3779b97f4a7c15ULL;

static uint64_t rnd(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}

static bool same(double a, double b)
{
    bits_t x = {a};
    bits_t y = {b};
    if (isnan(a) && isnan(b)) {
        return true;
    }
    return x.u == y.u; /* includes the sign of zero */
}

static void expect(double x, double y)
{
    double want = fmod(x, y);
    double got = bk_min_fmod(x, y);
    bk_test_checks++;
    if (!same(want, got)) {
        bk_test_failures++;
        fprintf(stderr, "FAIL fmod(%a, %a): got %a, libm %a\n", x, y, got, want);
    }
}

static double from_bits(uint64_t u)
{
    bits_t b = {0};
    b.u = u;
    return b.d;
}

static void test_fmod_specials(void)
{
    const double v[] = {0.0,
                        -0.0,
                        1.0,
                        -1.0,
                        0.5,
                        3.0,
                        -3.0,
                        5.5,
                        1e-310,
                        -1e-310,
                        4.9406564584124654e-324,
                        2.2250738585072014e-308,
                        1.7976931348623157e308,
                        -1.7976931348623157e308,
                        INFINITY,
                        -INFINITY,
                        NAN,
                        1e300,
                        1e-300,
                        7.0,
                        0.1,
                        10.0};
    for (size_t i = 0; i < sizeof v / sizeof v[0]; i++) {
        for (size_t j = 0; j < sizeof v / sizeof v[0]; j++) {
            expect(v[i], v[j]);
        }
    }
}

static void test_fmod_gravity_timer_domain(void)
{
    /* the console passes (tick - lastFall) and a period counts * 2048 / 690000 s * 60 */
    static const double counts[] = {150, 120, 100, 82, 60, 41, 35, 31, 27, 25};
    for (unsigned i = 0; i < 10; i++) {
        double period = counts[i] * 2048.0 / 11500.0;
        for (int k = 0; k < 200000; k++) {
            double x = (double)(rnd() % 40000000u) / 8.0 + (double)(rnd() % 1000u) * period;
            expect(x, period);
            expect(floor(x), period);
        }
    }
}

static void test_fmod_random_bit_patterns(void)
{
    for (int k = 0; k < 1000000; k++) {
        expect(from_bits(rnd()), from_bits(rnd()));
    }
}

static void test_fmod_wide_exponent_range_and_subnormals(void)
{
    for (int k = 0; k < 500000; k++) {
        /* x much larger than y, y subnormal or tiny, both signs */
        uint64_t sx = rnd() & 0x8000000000000000ULL;
        uint64_t sy = rnd() & 0x8000000000000000ULL;
        double x = from_bits(sx | ((uint64_t)(900 + rnd() % 1146) << 52) | (rnd() & 0xfffffffffffffULL));
        double y = from_bits(sy | ((uint64_t)(rnd() % 80) << 52) | (rnd() & 0xfffffffffffffULL));
        expect(x, y);
        expect(y, x);
        expect(x, from_bits(sy | ((rnd() & 0xfffffffffffffULL) | 1ULL))); /* subnormal divisor */
    }
}

static void test_memory_functions(void)
{
    uint8_t a[64], b[64];
    for (size_t i = 0; i < sizeof a; i++) {
        a[i] = (uint8_t)(i * 7 + 1);
    }
    bk_min_memcpy(b, a, sizeof a);
    CHECK(memcmp(a, b, sizeof a) == 0);
    CHECK_EQ(bk_min_memcmp(a, b, sizeof a), 0);
    b[63] ^= 1;
    CHECK(bk_min_memcmp(a, b, sizeof a) < 0 || bk_min_memcmp(a, b, sizeof a) > 0);
    CHECK_EQ(bk_min_memcmp(a, b, 63), 0);
    bk_min_memset(b, 0xa5, sizeof b);
    for (size_t i = 0; i < sizeof b; i++) {
        CHECK_EQ(b[i], 0xa5);
    }
    uint8_t m1[32], m2[32];
    for (size_t off = 0; off < 8; off++) { /* overlapping moves in both directions */
        for (size_t i = 0; i < sizeof m1; i++) {
            m1[i] = m2[i] = (uint8_t)i;
        }
        bk_min_memmove(m1 + off, m1, 20);
        memmove(m2 + off, m2, 20);
        CHECK(memcmp(m1, m2, sizeof m1) == 0);
        for (size_t i = 0; i < sizeof m1; i++) {
            m1[i] = m2[i] = (uint8_t)i;
        }
        bk_min_memmove(m1, m1 + off, 20);
        memmove(m2, m2 + off, 20);
        CHECK(memcmp(m1, m2, sizeof m1) == 0);
    }
    CHECK_EQ(bk_min_strlen(""), 0);
    CHECK_EQ(bk_min_strlen("brick"), 5);
    char d1[8], d2[8];
    memset(d1, 'x', sizeof d1);
    memset(d2, 'x', sizeof d2);
    bk_min_strncpy(d1, "ab", 6);
    strncpy(d2, "ab", 6);
    CHECK(memcmp(d1, d2, sizeof d1) == 0);
    bk_min_strcpy(d1, "xyz");
    strcpy(d2, "xyz");
    CHECK(memcmp(d1, d2, sizeof d1) == 0);
    volatile size_t four = 4; /* truncating copy: not a literal, so the compiler does not warn about the intended case */
    bk_min_strncpy(d1, "abcdefgh", four);
    strncpy(d2, "abcdefgh", four);
    CHECK(memcmp(d1, d2, sizeof d1) == 0);
}

int main(void)
{
    TEST_RUN(test_fmod_specials);
    TEST_RUN(test_fmod_gravity_timer_domain);
    TEST_RUN(test_fmod_random_bit_patterns);
    TEST_RUN(test_fmod_wide_exponent_range_and_subnormals);
    TEST_RUN(test_memory_functions);
    return bk_test_finish("minlibc");
}
