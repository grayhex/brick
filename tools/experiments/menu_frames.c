/*
 * The menu of every shipped game as a sequence of pictures: one PBM per frame of its looped gameplay clip, as the console
 * composes it (the same code as on the device, reference grid of 8 dots per cell). tools/frames/menu_gif.py turns them into
 * animated GIFs for the documentation; docs/UI_MENU.md section 4 shows them. The panel is not modelled: a real one shows
 * these pictures at most `max_fps` times a second.
 *
 *   mkdir -p out && ./build/tests/menu_frames out        # out/<game id>_<frame>.pbm and out/<game id>.txt
 *
 * The blink phase of the focused value follows the timeline, so a picture shows the value lit or dark as it would be
 * `frame * frame_ticks` ticks after the menu opened.
 */
#include <stdio.h>
#include <string.h>

#include "brick/catalog.h"

static void write_pbm(const bk_frame_t *f, const char *path)
{
    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        fprintf(stderr, "cannot write %s\n", path);
        return;
    }
    fprintf(fp, "P4\n%u %u\n", (unsigned)f->width, (unsigned)f->height);
    fwrite(f->bits, 1, bk_frame_bytes(f), fp);
    fclose(fp);
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    size_t count = 0;
    const bk_catalog_entry_t *catalog = bk_catalog(&count);
    static bk_hal_t hal; /* no ports: defaults, no storage, no sound */
    static bk_console_t console;

    for (size_t g = 0; g < count; g++) {
        const bk_clip_t *clip = catalog[g].def->preview;
        if (clip == NULL || !bk_console_init(&console, catalog, count, &hal, 8)) {
            continue;
        }
        for (unsigned i = 0; i < BK_SPLASH_TICKS; i++) {
            bk_console_step(&console, 0);
        }
        for (size_t i = 0; i < g; i++) { /* the game is picked with RIGHT, as a player would */
            bk_console_step(&console, BK_BTN_RIGHT);
            bk_console_step(&console, 0);
        }
        char path[512];
        for (unsigned k = 0; k < clip->frame_count; k++) {
            console.ticks = k * clip->frame_ticks;
            console.preview_ticks = k * clip->frame_ticks;
            snprintf(path, sizeof path, "%s/%s_%02u.pbm", dir, catalog[g].def->id, k);
            write_pbm(bk_console_render(&console), path);
        }
        snprintf(path, sizeof path, "%s/%s.txt", dir, catalog[g].def->id);
        FILE *fp = fopen(path, "w");
        if (fp != NULL) {
            fprintf(fp, "%u %u %u\n", (unsigned)clip->frame_count, (unsigned)clip->frame_ticks, (unsigned)clip->poster);
            fclose(fp);
        }
        printf("%s: %u frames, %u ticks each\n", catalog[g].def->id, (unsigned)clip->frame_count,
               (unsigned)clip->frame_ticks);
    }
    return 0;
}
