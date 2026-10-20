#!/usr/bin/env python3
"""Contact sheets of the menu clips (brick/clip.h): every frame of a game's looped gameplay animation, in order.

The clips are constant data in the game sources (`static const char <NAME>_CLIP_FRAMES[][BK_CLIP_CELLS + 1]`); this tool
reads them from there, so a sheet is always the picture of what is compiled in. A frame is 10 x 5 cells, drawn in the
cell style of the compositor (ring, gap, core) on the reference grid; the number under a frame is its index, the one of the
poster frame (shown while the setting DEMO is off) is underlined. Standard library only.

    python3 tools/frames/clip_sheet.py firmware/games/tetris/tetris.c docs/img/clip_tetris.png
"""
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from frames_to_png import write_png  # noqa: E402

COLS, ROWS = 10, 5
PITCH = 8  # dots per cell, the reference grid
PER_LINE = 6

DIGITS = {  # 3 x 5 digits for the frame numbers
    "0": ["###", "#.#", "#.#", "#.#", "###"], "1": [".#.", "##.", ".#.", ".#.", "###"],
    "2": ["##.", "..#", ".#.", "#..", "###"], "3": ["##.", "..#", ".#.", "..#", "##."],
    "4": ["#.#", "#.#", "###", "..#", "..#"], "5": ["###", "#..", "##.", "..#", "##."],
    "6": [".##", "#..", "###", "#.#", "###"], "7": ["###", "..#", ".#.", ".#.", ".#."],
    "8": ["###", "#.#", "###", "#.#", "###"], "9": ["###", "#.#", "###", "..#", "##."],
}


def parse_clip(source):
    """Returns (frames, frame_ticks, poster): frames are strings of 50 characters."""
    text = Path(source).read_text()
    m = re.search(r"static const char (\w+)_CLIP_FRAMES\[\]\[BK_CLIP_CELLS \+ 1\] = \{(.*?)\n\};", text, re.S)
    if not m:
        raise SystemExit(f"{source}: no *_CLIP_FRAMES array")
    name, body = m.group(1), m.group(2)
    body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)
    frames = []
    for chunk in re.split(r",\s*(?=\")|,\s*$", body.strip().rstrip(",")):
        literals = re.findall(r'"([.#]*)"', chunk)
        if literals:
            frames.append("".join(literals))
    t = re.search(rf"static const bk_clip_t {name}_CLIP = \{{[^,]*,\s*(\d+),\s*(\d+),", text)
    if not t:
        raise SystemExit(f"{source}: no {name}_CLIP definition")
    return frames, int(t.group(1)), int(t.group(2))


def draw_cell(canvas, x, y):
    for dy in range(7):
        for dx in range(7):
            ring = dx in (0, 6) or dy in (0, 6)
            core = 2 <= dx <= 4 and 2 <= dy <= 4
            if ring or core:
                canvas[y + dy][x + dx] = 1


def draw_text(canvas, x, y, text, underline=False):
    for i, ch in enumerate(text):
        for r, row in enumerate(DIGITS[ch]):
            for c, v in enumerate(row):
                if v == "#":
                    canvas[y + r][x + i * 4 + c] = 1
    if underline:
        for c in range(len(text) * 4 - 1):
            canvas[y + 6][x + c] = 1


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: clip_sheet.py game.c out.png")
    frames, ticks, poster = parse_clip(sys.argv[1])
    for i, f in enumerate(frames):
        if len(f) != COLS * ROWS:
            raise SystemExit(f"frame {i} has {len(f)} cells, expected {COLS * ROWS}")
    block_w = COLS * PITCH + PITCH
    block_h = ROWS * PITCH + 2 * PITCH
    lines = (len(frames) + PER_LINE - 1) // PER_LINE
    w, h = PER_LINE * block_w + PITCH, lines * block_h + PITCH
    canvas = [[0] * w for _ in range(h)]
    for i, f in enumerate(frames):
        bx = PITCH + (i % PER_LINE) * block_w
        by = PITCH + (i // PER_LINE) * block_h
        for y in range(ROWS):  # the thin frame of the 10 x 5 window
            for x in range(COLS):
                if f[y * COLS + x] == "#":
                    draw_cell(canvas, bx + x * PITCH, by + y * PITCH)
        for x in range(COLS * PITCH - 1):
            canvas[by + ROWS * PITCH + 1][bx + x] = 1 if x % 2 == 0 else 0  # a dotted rule under the picture
        draw_text(canvas, bx, by + ROWS * PITCH + 4, str(i), underline=(i == poster))
    write_png(sys.argv[2], w, h, canvas, 2)
    print(f"{sys.argv[1]}: {len(frames)} frames, {ticks} ticks per frame ({60 / ticks:g} pictures/s), "
          f"loop {len(frames) * ticks / 60:.1f} s, poster {poster} -> {sys.argv[2]}")


if __name__ == "__main__":
    main()
