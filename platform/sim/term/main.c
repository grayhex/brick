/*
 * Terminal front-end of the simulator: the console in a terminal window, ~60 Hz, keyboard input.
 *
 * The picture is drawn with Braille characters (2 x 4 dots per character) in reverse video, so the ink shows as dark
 * dots on a light background (positive polarity, as on the planned LCD). The reference grid (cell 8) is 132 x 160 dots =
 * 66 x 40 characters.
 *
 * A terminal reports key presses (with auto-repeat) but never releases, so a key counts as held for HOLD_TICKS after its
 * last press event. That is enough for menus and for stepping a piece; the browser front-end has real key state.
 */
#include "sim.h"

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define HOLD_TICKS 10

static volatile sig_atomic_t g_quit;
static struct termios g_saved;
static bool g_raw;

static void on_signal(int sig)
{
    (void)sig;
    g_quit = 1;
}

static void restore_terminal(void)
{
    if (g_raw) {
        tcsetattr(STDIN_FILENO, TCSANOW, &g_saved);
        g_raw = false;
        fputs("\x1b[0m\x1b[?25h\x1b[?1049l", stdout);
        fflush(stdout);
    }
}

static bool enter_raw(void)
{
    if (!isatty(STDIN_FILENO) || tcgetattr(STDIN_FILENO, &g_saved) != 0) {
        return false;
    }
    struct termios t = g_saved;
    t.c_lflag &= (tcflag_t) ~(ICANON | ECHO);
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &t) != 0) {
        return false;
    }
    g_raw = true;
    atexit(restore_terminal);
    fputs("\x1b[?1049h\x1b[?25l\x1b[2J", stdout);
    return true;
}

typedef struct {
    int key;
    bk_buttons_t button;
} keymap_t;

enum { KEY_UP = 1000, KEY_DOWN, KEY_RIGHT, KEY_LEFT };

static const keymap_t KEYS[] = {
    {KEY_UP, BK_BTN_UP},  {KEY_DOWN, BK_BTN_DOWN}, {KEY_LEFT, BK_BTN_LEFT}, {KEY_RIGHT, BK_BTN_RIGHT}, {' ', BK_BTN_ACTION},
    {'z', BK_BTN_ACTION}, {'Z', BK_BTN_ACTION},    {'a', BK_BTN_MODE},      {'A', BK_BTN_MODE},        {'s', BK_BTN_SELECT},
    {'S', BK_BTN_SELECT}, {'d', BK_BTN_START},     {'D', BK_BTN_START},     {'\r', BK_BTN_START},
};

/* Next key from the input buffer; -1 when empty. Arrow keys arrive as ESC [ A..D (or ESC O A..D). */
static int next_key(unsigned char *buf, size_t *len)
{
    if (*len == 0) {
        return -1;
    }
    int key = buf[0];
    size_t used = 1;
    if (key == 0x1b && *len >= 3 && (buf[1] == '[' || buf[1] == 'O')) {
        switch (buf[2]) {
        case 'A':
            key = KEY_UP;
            break;
        case 'B':
            key = KEY_DOWN;
            break;
        case 'C':
            key = KEY_RIGHT;
            break;
        case 'D':
            key = KEY_LEFT;
            break;
        default:
            break;
        }
        used = 3;
    }
    memmove(buf, buf + used, *len - used);
    *len -= used;
    return key;
}

/* One frame as text: Braille cells, reverse video, one line per 4 dot rows. */
static void draw(const bk_sim_t *s, double fps_shown)
{
    const bk_frame_t *f = bk_sim_frame(s);
    static char out[1 << 16];
    size_t n = 0;
    n += (size_t)snprintf(out + n, sizeof out - n, "\x1b[H");
    if (f != NULL) {
        for (int y = 0; y < f->height; y += 4) {
            n += (size_t)snprintf(out + n, sizeof out - n, "\x1b[7m");
            for (int x = 0; x < f->width; x += 2) {
                unsigned bits = 0;
                static const unsigned DOT[4][2] = {{0x01, 0x08}, {0x02, 0x10}, {0x04, 0x20}, {0x40, 0x80}};
                for (int dy = 0; dy < 4; dy++) {
                    for (int dx = 0; dx < 2; dx++) {
                        if (bk_frame_get(f, x + dx, y + dy)) {
                            bits |= DOT[dy][dx];
                        }
                    }
                }
                unsigned cp = 0x2800u + bits;
                out[n++] = (char)(0xE0 | (cp >> 12));
                out[n++] = (char)(0x80 | ((cp >> 6) & 0x3f));
                out[n++] = (char)(0x80 | (cp & 0x3f));
            }
            n += (size_t)snprintf(out + n, sizeof out - n, "\x1b[0m\x1b[K\r\n");
        }
    }
    n += (size_t)snprintf(
        out + n, sizeof out - n,
        "\x1b[0m\x1b[Ktick %u  shown %u  waiting %u  panel %s%u  light %u  ble %s  tone %u Hz  %.0f pic/s%s\r\n",
        (unsigned)s->console.ticks, (unsigned)s->console.presents, (unsigned)s->console.dropped_frames,
        s->cfg.max_fps != 0 ? "max " : "any ", (unsigned)s->cfg.max_fps, (unsigned)s->light, s->ble ? "on" : "off",
        (unsigned)s->tone_hz, fps_shown, s->recording ? "  [REC]" : (s->replaying ? "  [REPLAY]" : ""));
    n += (size_t)snprintf(out + n, sizeof out - n,
                          "\x1b[Karrows: D-pad   space/z: ACTION   a: MODE   s: SELECT   d/Enter: START   q: quit\r\n");
    (void)!write(STDOUT_FILENO, out, n);
}

static bool read_file(const char *path, uint8_t *buf, size_t cap, size_t *len)
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        return false;
    }
    *len = fread(buf, 1, cap, fp);
    bool ok = !ferror(fp) && fgetc(fp) == EOF;
    fclose(fp);
    return ok;
}

static bool write_file(const char *path, const uint8_t *buf, size_t len)
{
    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        return false;
    }
    bool ok = fwrite(buf, 1, len, fp) == len;
    return fclose(fp) == 0 && ok;
}

static void usage(const char *argv0)
{
    fprintf(stderr,
            "Usage: %s [--cell N | --panel WxH] [--fps N] [--seed N] [--record FILE] [--replay FILE [--fast]]\n"
            "  --cell N        reference grid, N dots per game cell (even, 8..16; default 8)\n"
            "  --panel WxH     fit the frame to a native panel size, e.g. 336x536\n"
            "  --fps N         panel can show at most N new pictures per second (0 = every tick; default 0)\n"
            "  --seed N        entropy of the games (default: current time)\n"
            "  --record FILE   save the button trace of this session on exit (BKT1)\n"
            "  --replay FILE   replay a trace; --fast runs it without waiting and prints the final picture hash\n",
            argv0);
}

int main(int argc, char **argv)
{
    bk_sim_config_t cfg = {.cell = 8, .width = 0, .height = 0, .max_fps = 0, .seed = (uint32_t)time(NULL)};
    const char *record_path = NULL;
    const char *replay_path = NULL;
    bool fast = false;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        bool has_val = i + 1 < argc;
        if (strcmp(a, "--cell") == 0 && has_val) {
            cfg.cell = (unsigned)atoi(argv[++i]);
        } else if (strcmp(a, "--panel") == 0 && has_val) {
            if (sscanf(argv[++i], "%ux%u", &cfg.width, &cfg.height) != 2) {
                usage(argv[0]);
                return 2;
            }
        } else if (strcmp(a, "--fps") == 0 && has_val) {
            cfg.max_fps = (uint8_t)atoi(argv[++i]);
        } else if (strcmp(a, "--seed") == 0 && has_val) {
            cfg.seed = (uint32_t)strtoul(argv[++i], NULL, 0);
        } else if (strcmp(a, "--record") == 0 && has_val) {
            record_path = argv[++i];
        } else if (strcmp(a, "--replay") == 0 && has_val) {
            replay_path = argv[++i];
        } else if (strcmp(a, "--fast") == 0) {
            fast = true;
        } else {
            usage(argv[0]);
            return 2;
        }
    }

    static bk_sim_t sim;
    if (!bk_sim_init(&sim, &cfg)) {
        fprintf(stderr, "invalid configuration (cell %u, panel %ux%u)\n", cfg.cell, cfg.width, cfg.height);
        return 2;
    }
    if (replay_path != NULL) {
        size_t len = 0;
        if (!read_file(replay_path, sim.io, sizeof sim.io, &len) || !bk_sim_trace_load(&sim, sim.io, len)) {
            fprintf(stderr, "cannot replay %s: not a valid BKT1 trace\n", replay_path);
            return 2;
        }
    } else if (record_path != NULL) {
        bk_sim_record_start(&sim);
    }

    if (fast) {
        while (!bk_sim_replay_done(&sim)) {
            bk_sim_tick(&sim);
        }
        printf("ticks %u  presents %u  dropped %u  picture hash %08x\n", (unsigned)sim.console.ticks,
               (unsigned)sim.console.presents, (unsigned)sim.console.dropped_frames, (unsigned)bk_sim_frame_hash(&sim));
        return 0;
    }

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    signal(SIGHUP, on_signal);
    if (!enter_raw()) {
        fprintf(stderr, "stdin is not a terminal (use --replay FILE --fast for headless runs)\n");
        return 2;
    }

    unsigned char inbuf[64];
    size_t inlen = 0;
    uint32_t hold_until[8] = {0};
    uint32_t last_seq = ~0u;
    struct timespec next;
    clock_gettime(CLOCK_MONOTONIC, &next);
    uint32_t window_start_tick = 0;
    uint32_t window_start_seq = 0;
    double fps_shown = 0;
    while (!g_quit) {
        struct pollfd pfd = {.fd = STDIN_FILENO, .events = POLLIN, .revents = 0};
        if (poll(&pfd, 1, 0) > 0 && inlen < sizeof inbuf) {
            ssize_t r = read(STDIN_FILENO, inbuf + inlen, sizeof inbuf - inlen);
            if (r > 0) {
                inlen += (size_t)r;
            }
        }
        int key;
        while ((key = next_key(inbuf, &inlen)) >= 0) {
            if (key == 'q' || key == 'Q') {
                g_quit = 1;
            }
            for (size_t i = 0; i < sizeof KEYS / sizeof KEYS[0]; i++) {
                if (KEYS[i].key == key) {
                    for (unsigned b = 0; b < 8; b++) {
                        if (KEYS[i].button == (1u << b)) {
                            hold_until[b] = sim.console.ticks + HOLD_TICKS;
                        }
                    }
                }
            }
        }
        bk_buttons_t held = 0;
        for (unsigned b = 0; b < 8; b++) {
            if (sim.console.ticks < hold_until[b]) {
                held |= (bk_buttons_t)(1u << b);
            }
        }
        bk_sim_set_buttons(&sim, held);
        bk_sim_tick(&sim);

        if (sim.frame_seq != last_seq) {
            last_seq = sim.frame_seq;
            uint32_t dt = sim.console.ticks - window_start_tick;
            if (dt >= 30) {
                fps_shown = (double)(sim.frame_seq - window_start_seq) * 60.0 / (double)dt;
                window_start_tick = sim.console.ticks;
                window_start_seq = sim.frame_seq;
            }
            draw(&sim, fps_shown);
        }

        next.tv_nsec += 1000000000L / 60;
        if (next.tv_nsec >= 1000000000L) {
            next.tv_nsec -= 1000000000L;
            next.tv_sec++;
        }
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec > next.tv_sec + 1) {
            next = now; /* far behind (suspended terminal): do not try to catch up */
        }
        while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL) == EINTR && !g_quit) {
        }
    }
    restore_terminal();
    fprintf(stderr, "ticks %u  presents %u  dropped %u  picture hash %08x\n", (unsigned)sim.console.ticks,
            (unsigned)sim.console.presents, (unsigned)sim.console.dropped_frames, (unsigned)bk_sim_frame_hash(&sim));

    if (record_path != NULL) {
        bk_sim_record_stop(&sim);
        size_t n = bk_sim_trace_save(&sim, sim.io, sizeof sim.io);
        if (n == 0 || !write_file(record_path, sim.io, n)) {
            fprintf(stderr, "cannot write %s\n", record_path);
            return 1;
        }
        fprintf(stderr, "recorded %u ticks to %s\n", (unsigned)sim.trace_len, record_path);
    }
    return 0;
}
