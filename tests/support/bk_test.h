/*
 * Minimal dependency-free test framework. A test binary is a main() that calls TEST_RUN(fn) and returns
 * bk_test_finish(). Failed checks are recorded with file:line and do not abort, so one run reports everything.
 */
#ifndef BK_TEST_H
#define BK_TEST_H

#include <stdio.h>
#include <string.h>

extern int bk_test_failures;
extern int bk_test_checks;
extern const char *bk_test_current;

#define CHECK(cond)                                                                                                         \
    do {                                                                                                                    \
        bk_test_checks++;                                                                                                   \
        if (!(cond)) {                                                                                                      \
            bk_test_failures++;                                                                                             \
            fprintf(stderr, "FAIL [%s] %s:%d: %s\n", bk_test_current, __FILE__, __LINE__, #cond);                           \
        }                                                                                                                   \
    } while (0)

#define CHECK_EQ(actual, expected)                                                                                          \
    do {                                                                                                                    \
        long long a_ = (long long)(actual);                                                                                 \
        long long e_ = (long long)(expected);                                                                               \
        bk_test_checks++;                                                                                                   \
        if (a_ != e_) {                                                                                                     \
            bk_test_failures++;                                                                                             \
            fprintf(stderr, "FAIL [%s] %s:%d: %s == %s  (actual %lld, expected %lld)\n", bk_test_current, __FILE__,         \
                    __LINE__, #actual, #expected, a_, e_);                                                                  \
        }                                                                                                                   \
    } while (0)

#define CHECK_STR(actual, expected)                                                                                         \
    do {                                                                                                                    \
        const char *a_ = (actual);                                                                                          \
        const char *e_ = (expected);                                                                                        \
        bk_test_checks++;                                                                                                   \
        if (strcmp(a_, e_) != 0) {                                                                                          \
            bk_test_failures++;                                                                                             \
            fprintf(stderr, "FAIL [%s] %s:%d: %s\n  actual:   \"%s\"\n  expected: \"%s\"\n", bk_test_current, __FILE__,     \
                    __LINE__, #actual, a_, e_);                                                                             \
        }                                                                                                                   \
    } while (0)

#define TEST_RUN(fn)                                                                                                        \
    do {                                                                                                                    \
        bk_test_current = #fn;                                                                                              \
        fn();                                                                                                               \
    } while (0)

int bk_test_finish(const char *suite);

#endif /* BK_TEST_H */
