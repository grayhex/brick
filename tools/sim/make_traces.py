#!/usr/bin/env python3
"""Writes the simulator trace fixtures (tests/fixtures/sim/*.bkt) and their expected results.

A trace is a BKT1 file (platform/sim/sim.h): config + one button mask per 60 Hz tick. The scripts below are readable
sessions; the expected ticks / pictures shown / pictures waiting / hash of the last picture come from the NATIVE
simulator (brick_sim_term --replay F --fast), and every other build (WebAssembly, ARM later) must reproduce them.

    python3 tools/sim/make_traces.py build/platform/brick_sim_term tests/fixtures/sim
"""
import re
import struct
import subprocess
import sys
from pathlib import Path

BTN = {"UP": 1, "DOWN": 2, "LEFT": 4, "RIGHT": 8, "ACTION": 16, "MODE": 32, "SELECT": 64, "START": 128, "": 0}


def mask(names):
    m = 0
    for n in names.split("+"):
        m |= BTN[n]
    return m


def expand(script):
    """'70:- 3:START 3:- 50:DOWN' -> list of masks, one per tick."""
    out = []
    for part in script.split():
        ticks, names = part.split(":")
        out += [0 if names == "-" else mask(names)] * int(ticks)
    return out


def tap(name):
    return f"3:{name} 3:-"


TETRIS = " ".join([
    "70:-", tap("START"), "90:-",
    tap("LEFT"), tap("LEFT"), tap("LEFT"), tap("ACTION"), "50:DOWN", tap("RIGHT"), "60:-",
    "200:DOWN", "30:LEFT", tap("ACTION"), tap("ACTION"), "40:DOWN", "120:-",
    "12:RIGHT", "6:-", "12:RIGHT", "300:DOWN", "150:-",       # fills the well: stacking and eventually game over
    "300:DOWN", "200:-", tap("START"), "30:-", tap("MODE"), "30:-", tap("MODE"), "100:-",
    tap("SELECT"), "60:-",
])

MENU = " ".join([
    "80:-", tap("RIGHT"), tap("LEFT"), tap("SELECT"), tap("UP"), tap("UP"), tap("DOWN"), tap("SELECT"), tap("UP"),
    tap("UP"), tap("UP"), "20:-",
    tap("MODE"), "10:-",                                                  # settings, the cursor on SOUND
    tap("DOWN"), tap("ACTION"), tap("DOWN"), tap("ACTION"),               # LIGHT one step up, BLE on
    tap("DOWN"), tap("ACTION"), "100:-", tap("ACTION"), "100:-",          # DEMO off for a while, then on again
    tap("DOWN"), tap("RIGHT"), "20:-",                                    # AUTO OFF one step up
    tap("DOWN"), tap("DOWN"), tap("ACTION"), "30:-",                      # past RECORDS to BUTTONS: the test page
    "20:MODE+SELECT+START", "10:-",                                       # the three function keys leave it
    tap("DOWN"), tap("ACTION"), "30:-", tap("MODE"), "30:-",              # ABOUT and back to the list
    tap("MODE"), "50:-", tap("START"), "60:-", tap("MODE"), "30:-", tap("MODE"), "60:-",  # menu, into a game and out again
])

SNAKE = " ".join([
    "70:-", tap("RIGHT"), "20:-", tap("START"), "70:-",             # game 2 in the menu, then play
    tap("UP"), "25:-", tap("RIGHT"), "30:-", tap("DOWN"), "30:-", tap("RIGHT"), "30:-",
    tap("LEFT"), "40:-", tap("ACTION"), "30:-", "6:UP", "6:-", "6:LEFT", "6:-",
    "500:-",                                                         # runs into a wall: lives, clear scene
    tap("START"), "20:-", tap("START"), "20:-",                      # pause, resume
    "700:-", tap("MODE"), "30:-", tap("MODE"), "40:-",               # more losses, then out to the menu
])

SNAKE_LCD = " ".join([
    "70:-", tap("RIGHT"), "20:-", tap("START"), "70:-",             # game 2 in the menu, then play on the 336 x 536 panel at 15 Hz
    tap("UP"), "30:-", tap("RIGHT"), "30:-", tap("DOWN"), "30:-", tap("LEFT"), "30:-",
    "800:-", tap("SELECT"), "20:-", "400:-",
])

RACING_LCD = " ".join([
    "70:-", tap("RIGHT"), tap("RIGHT"), "20:-", tap("START"), "130:-",   # game 3 in the menu, play on the 336 x 536 panel at 15 Hz
    tap("LEFT"), "40:-", "20:UP", "20:-", tap("RIGHT"), "40:-", "12:ACTION", "30:-",
    tap("LEFT"), "60:-", tap("RIGHT"), "400:-",                          # crashes: explosions, lost lives, clear scenes
    tap("START"), "20:-", tap("START"), "500:-",                         # pause, resume
    tap("MODE"), "30:-", tap("MODE"), "40:-",                            # pause, then out to the menu
])

TANK_LCD = " ".join([
    "70:-", tap("RIGHT"), tap("RIGHT"), tap("RIGHT"), "20:-", tap("START"), "130:-",   # game 4 in the menu, play on the 336 x 536 panel at 15 Hz
    "12:LEFT", "20:-", "30:ACTION", "20:UP", "10:-", "30:ACTION", "12:RIGHT", "40:-",  # drive, turn and fire
    "30:ACTION", "6:DOWN", "30:ACTION", "500:-",                                        # the enemies answer: bullets, hits, explosions
    tap("START"), "20:-", tap("START"), "400:-",                                        # pause, resume
    tap("MODE"), "30:-", tap("MODE"), "40:-",                                           # pause, then out to the menu
])

# name, cell, width, height, max_fps, seed, script
TRACES = [
    ("tetris_ref8", 8, 0, 0, 0, 12345, TETRIS),
    ("tetris_lcd316_10hz", 0, 336, 536, 10, 777, TETRIS),
    ("menu_cell10_30hz", 10, 0, 0, 30, 4242, MENU),
    ("snake_ref8", 8, 0, 0, 0, 31, SNAKE),
    ("snake_lcd316_15hz", 0, 336, 536, 15, 5150, SNAKE_LCD),
    ("racing_lcd316_15hz", 0, 336, 536, 15, 777, RACING_LCD),
    ("tank_lcd316_15hz", 0, 336, 536, 15, 4711, TANK_LCD),
]


def write_trace(path, cell, w, h, fps, seed, masks):
    data = b"BKT1" + struct.pack("<HHBBII", w, h, cell, fps, seed, len(masks)) + bytes(masks)
    path.write_bytes(data)


def main():
    exe, out = sys.argv[1], Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    lines = []
    for name, cell, w, h, fps, seed, script in TRACES:
        f = out / f"{name}.bkt"
        write_trace(f, cell, w, h, fps, seed, expand(script))
        res = subprocess.run([exe, "--replay", str(f), "--fast"], capture_output=True, text=True, check=True).stdout
        m = re.fullmatch(r"ticks (\d+)\s+presents (\d+)\s+dropped (\d+)\s+picture hash ([0-9a-f]{8})\s*", res)
        if not m:
            raise SystemExit(f"unexpected output for {name}: {res!r}")
        lines.append(f"{name}.bkt {m.group(1)} {m.group(2)} {m.group(3)} {m.group(4)}")
    (out / "expected.txt").write_text("# file ticks presents dropped last-picture-hash (native simulator)\n" + "\n".join(lines) + "\n")
    print("\n".join(lines))


if __name__ == "__main__":
    main()
