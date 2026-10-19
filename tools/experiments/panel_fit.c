/*
 * What would the status column look like, in millimetres, on each panel candidate of docs/DISPLAY_CANDIDATES.md?
 *
 * Input: native pixels and active area of each candidate (taken from that document, portrait mounting; the 4.4" and
 * 2.7" panels are rotated by 90 degrees there). The compositor fits itself to the panel (bk_layout_fit) and this program
 * converts the resulting dot sizes to millimetres. It is ARITHMETIC on datasheet figures, not a legibility test:
 * whether 1.9 mm digits are comfortable to read needs real panels (hardware gate 1 in HARDWARE_RESEARCH.md).
 *
 *   cmake --build build --target panel_fit && ./build/tests/panel_fit
 */
#include <stdio.h>

#include "brick/render.h"

typedef struct {
    const char *name;
    unsigned w, h;     /* native pixels, portrait */
    double mm_w, mm_h; /* active area, portrait */
} candidate_t;

static const candidate_t CANDIDATES[] = {
    {"Sharp LS032B7DD02 3.16in", 336, 536, 42.672, 68.072},
    {"Sharp LS044Q7DH01 4.4in (rotated)", 240, 320, 67.2, 89.6},
    {"Sharp LS027B7DH01 2.7in (rotated)", 240, 400, 35.28, 58.8},
};

int main(void)
{
    printf("%-36s %5s %9s %8s %9s %10s %10s %9s\n", "panel", "cell", "cell(mm)", "status", "status(mm)", "digit(mm)",
           "label(mm)", "preview");
    for (unsigned i = 0; i < sizeof CANDIDATES / sizeof CANDIDATES[0]; i++) {
        const candidate_t *c = &CANDIDATES[i];
        bk_layout_t l;
        if (!bk_layout_fit(&l, c->w, c->h)) {
            printf("%-36s does not fit\n", c->name);
            continue;
        }
        double mm_per_dot = c->mm_w / c->w;
        unsigned status = l.panel_w + l.ring; /* divider included, as in DISPLAY_CANDIDATES.md */
        printf("%-36s %5u %9.2f %8u %9.1f %4.2f x %4.2f %4.2f tall %4u dots\n", c->name, l.cell, l.cell * mm_per_dot, status,
               status * mm_per_dot, l.digit_w * mm_per_dot, l.digit_h * mm_per_dot, 5 * l.label_scale * mm_per_dot,
               4u * l.preview_cell);
    }
    return 0;
}
