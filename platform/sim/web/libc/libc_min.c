/*
 * The C library the core needs, so that it runs as WebAssembly without a libc: memcpy, memmove, memset, memcmp, strlen,
 * strcpy, strncpy and fmod (exactly rounded, so it agrees with every correct libm - the gravity timer of Tetris depends on
 * it, ADR-024).
 *
 * The functions are defined as bk_min_* so that tests can compare them with the host's libc in a normal build; the
 * WebAssembly build defines BK_MINLIBC_ALIASES and gets the standard names as well. Build it with -ffreestanding
 * (no loop-to-memcpy idiom recognition, which would make memcpy call itself).
 */
#include <stddef.h>
#include <stdint.h>

void *bk_min_memcpy(void *dst, const void *src, size_t n)
{
    uint8_t *d = dst;
    const uint8_t *s = src;
    while (n-- != 0) {
        *d++ = *s++;
    }
    return dst;
}

void *bk_min_memmove(void *dst, const void *src, size_t n)
{
    uint8_t *d = dst;
    const uint8_t *s = src;
    if (d == s || n == 0) {
        return dst;
    }
    if (d < s) {
        while (n-- != 0) {
            *d++ = *s++;
        }
    } else {
        d += n;
        s += n;
        while (n-- != 0) {
            *--d = *--s;
        }
    }
    return dst;
}

void *bk_min_memset(void *dst, int c, size_t n)
{
    uint8_t *d = dst;
    while (n-- != 0) {
        *d++ = (uint8_t)c;
    }
    return dst;
}

int bk_min_memcmp(const void *a, const void *b, size_t n)
{
    const uint8_t *x = a;
    const uint8_t *y = b;
    for (; n != 0; n--, x++, y++) {
        if (*x != *y) {
            return *x < *y ? -1 : 1;
        }
    }
    return 0;
}

size_t bk_min_strlen(const char *s)
{
    size_t n = 0;
    while (s[n] != '\0') {
        n++;
    }
    return n;
}

char *bk_min_strcpy(char *dst, const char *src)
{
    char *d = dst;
    while ((*d++ = *src++) != '\0') {
    }
    return dst;
}

char *bk_min_strncpy(char *dst, const char *src, size_t n)
{
    size_t i = 0;
    for (; i < n && src[i] != '\0'; i++) {
        dst[i] = src[i];
    }
    for (; i < n; i++) {
        dst[i] = '\0';
    }
    return dst;
}

typedef union {
    double d;
    uint64_t u;
} bits_t;

/*
 * x = mx * 2^ex and y = my * 2^ey with integer mantissas of at most 53 bits. For |x| >= |y| the exponent of the lowest
 * bit of x is not below that of y, so the remainder is ((mx mod my) * 2^(ex-ey) mod my) * 2^ey: reduce one bit at a
 * time (the intermediate value stays below 2^54) and scale the result back. It is exactly representable.
 */
double bk_min_fmod(double x, double y)
{
    bits_t bx = {x};
    bits_t by = {y};
    uint64_t sign = bx.u >> 63;
    uint64_t ax = bx.u << 1 >> 1;
    uint64_t ay = by.u << 1 >> 1;
    const uint64_t inf = 0x7ff0000000000000ULL;

    if (ay == 0 || ay > inf || ax >= inf) { /* y = 0, y NaN, x infinite or NaN */
        bits_t nan = {0};
        nan.u = 0x7ff8000000000000ULL;
        return nan.d;
    }
    if (ay == inf || ax < ay) {
        return x;
    }
    if (ax == ay) {
        bits_t z = {0};
        z.u = sign << 63;
        return z.d;
    }

    int ex = (int)(ax >> 52);
    int ey = (int)(ay >> 52);
    uint64_t mx = ax & 0xfffffffffffffULL;
    uint64_t my = ay & 0xfffffffffffffULL;
    if (ex != 0) {
        mx |= 1ULL << 52;
    } else {
        ex = 1; /* subnormal: no implicit bit, exponent of the smallest normal */
    }
    if (ey != 0) {
        my |= 1ULL << 52;
    } else {
        ey = 1;
    }
    /* value = m * 2^(e - 1075) */
    uint64_t r = mx % my;
    for (int e = ex; e > ey && r != 0; e--) {
        r <<= 1;
        if (r >= my) {
            r -= my;
        }
    }
    bits_t out = {0};
    if (r != 0) {
        /* r * 2^(ey - 1075): r < 2^53 converts exactly; scale by a power of two built from bits. */
        int k = ey - 1075;
        bits_t scale = {0};
        if (k >= -1022) {
            scale.u = (uint64_t)(k + 1023) << 52;
        } else {
            scale.u = 1ULL << (k + 1074);
        }
        out.d = (double)r * scale.d;
    }
    out.u |= sign << 63;
    return out.d;
}

#ifdef BK_MINLIBC_ALIASES
void *memcpy(void *dst, const void *src, size_t n)
{
    return bk_min_memcpy(dst, src, n);
}
void *memmove(void *dst, const void *src, size_t n)
{
    return bk_min_memmove(dst, src, n);
}
void *memset(void *dst, int c, size_t n)
{
    return bk_min_memset(dst, c, n);
}
int memcmp(const void *a, const void *b, size_t n)
{
    return bk_min_memcmp(a, b, n);
}
size_t strlen(const char *s)
{
    return bk_min_strlen(s);
}
char *strcpy(char *dst, const char *src)
{
    return bk_min_strcpy(dst, src);
}
char *strncpy(char *dst, const char *src, size_t n)
{
    return bk_min_strncpy(dst, src, n);
}
double fmod(double x, double y)
{
    return bk_min_fmod(x, y);
}
#endif
