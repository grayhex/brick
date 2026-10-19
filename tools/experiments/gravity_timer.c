/*
 * Experiment behind ADR-018: does an exact integer gravity timer behave like the reference's double arithmetic?
 *
 * The reference keeps the time of the last gravity step as a double and does `last = tick - fmod(tick - last, period)`.
 * The period is rational (counts * 2048 / 11500 ticks), so an integer phase counter in 1/11500-tick units is exact.
 * This program runs both for 20 million ticks per speed and counts ticks on which they disagree about a gravity step:
 *   mode 0: no key presses; mode 1: a soft drop (which resets the timer) every ~400 ticks; mode 2: every ~20 ticks.
 *
 *   cc -O2 -ffp-contract=off tools/experiments/gravity_timer.c -o gravity_timer -lm && ./gravity_timer
 *
 * Result recorded in docs/DECISIONS.md (ADR-018). Not part of the build or the test suite.
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
static const uint8_t C[10] = {150, 120, 100, 82, 60, 41, 35, 31, 27, 25};
static uint32_t rs = 12345;
static uint32_t rnd(void)
{
    rs = rs * 1664525u + 1013904223u;
    return rs >> 8;
}
int main(void)
{
    for (int sp = 0; sp < 10; sp++) {
        double ms = (double)((int64_t)C[sp] * 2048 * 1000) / 690000.0;
        double P = (ms * 60) / 1000.0;
        int64_t N = (int64_t)C[sp] * 2048; /* period = N / 11500 ticks */
        for (int mode = 0; mode < 3; mode++) {
            double last = 0;
            int64_t phi = 0; /* phase in 1/11500 ticks */
            uint64_t diffs = 0, falls = 0, total = 20000000;
            for (uint64_t t = 1; t <= total; t++) {
                int fd = 0, fi = 0;
                if ((double)t - last >= P) {
                    last = (double)t - fmod((double)t - last, P);
                    fd = 1;
                }
                phi += 11500;
                if (phi >= N) {
                    phi %= N;
                    fi = 1;
                }
                falls += fd;
                if (fd != fi) {
                    diffs++;
                }
                /* mode 1: soft drop resets gravity at random; mode 2: frequent resets (held DOWN bursts) */
                int reset = mode == 1 ? (rnd() % 400 == 0) : mode == 2 ? (rnd() % 20 == 0) : 0;
                if (reset) {
                    last = (double)t;
                    phi = 0;
                }
            }
            printf("speed %2d mode %d: %llu falls, %llu differing ticks\n", sp + 1, mode, (unsigned long long)falls,
                   (unsigned long long)diffs);
        }
    }
    return 0;
}
