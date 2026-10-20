#!/usr/bin/env python3
"""Animated GIFs of the menu of every game, from the pictures written by tools/experiments/menu_frames.c.

The GIF is a *presentation* of the logical frames: ink dark, background a light neutral grey (positive polarity, ADR-015);
the frame delay is the clip's `frame_ticks` at 60 ticks per second (GIF delays have a resolution of 10 ms). A real panel
shows these pictures at most `max_fps` times a second and its contrast is not modelled. Needs Pillow (documentation only,
not part of the build or the tests).

    mkdir -p /tmp/menu && ./build/tests/menu_frames /tmp/menu
    python3 tools/frames/menu_gif.py /tmp/menu docs/img        # docs/img/menu_<game id>.gif
"""
import sys
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
from frames_to_png import INK, PAPER, read_pbm  # noqa: E402

SCALE = 2


def frame_image(path):
    w, h, rows = read_pbm(str(path))
    img = Image.new("RGB", (w, h), PAPER)
    px = img.load()
    for y in range(h):
        for x in range(w):
            if rows[y][x]:
                px[x, y] = INK
    return img.resize((w * SCALE, h * SCALE), Image.NEAREST)


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: menu_gif.py frames_dir out_dir")
    src, out = Path(sys.argv[1]), Path(sys.argv[2])
    for info in sorted(src.glob("*.txt")):
        count, ticks, _poster = (int(v) for v in info.read_text().split())
        frames = [frame_image(src / f"{info.stem}_{k:02d}.pbm") for k in range(count)]
        delay_ms = round(ticks * 1000 / 60 / 10) * 10
        target = out / f"menu_{info.stem}.gif"
        frames[0].save(target, save_all=True, append_images=frames[1:], duration=delay_ms, loop=0, optimize=True)
        print(f"{target}: {count} frames, {delay_ms} ms each, loop {count * delay_ms / 1000:.1f} s")


if __name__ == "__main__":
    main()
