#!/usr/bin/env python3
"""Builds the R0.2 mock-up assembly from mechanical/params.json, validates it and exports STEP/STL.

    python3 mechanical/src/build.py [--thickness 20|22] [--out mechanical/build] [--report mechanical/validation.md]

Validation covers the generator, not a physical fit: solids are valid, the envelope is 80 x 175 x T, parts do not intersect,
the openings that must exist are open and the walls that must be closed are closed (sampled). Exit status 1 on any failure.
"""
import argparse
import hashlib
import itertools
import platform
import sys
from pathlib import Path

import cadquery as cq

import check_params
from case import front_shell, key_positions, rear_shell
from display import display_placeholder
from geom import point, rounded_box
from keys import NAMES, key_caps
from params import PARAMS_PATH, load, thickness

EPS_VOLUME = 1e-6  # mm^3
EPS_LENGTH = 1e-6  # mm


def build_parts(params, t):
    parts = {"front_shell": front_shell(params, t), "rear_shell": rear_shell(params, t)}
    parts.update(key_caps(params))
    parts["display_placeholder"] = display_placeholder(params)
    return parts


class Report:
    def __init__(self):
        self.rows = []

    def check(self, name, ok, detail=""):
        self.rows.append((name, bool(ok), detail))
        print(f"{'PASS' if ok else 'FAIL'}  {name}{': ' + detail if detail else ''}")

    @property
    def failed(self):
        return [r for r in self.rows if not r[1]]


def inside(solid, x, y, z):
    return solid.val().isInside(point(x, y, z), 1e-6)


def validate(params, t, parts, rep):
    body = params["body"]
    w, h, wall = body["width"], body["height"], body["wall_prototype"]

    for name, part in parts.items():
        shape = part.val()
        rep.check(f"{name} is a valid single solid with volume", shape.isValid() and len(part.solids().vals()) == 1 and shape.Volume() > 0,
                  f"volume {shape.Volume():.1f} mm3")

    shells = cq.Compound.makeCompound([parts["front_shell"].val(), parts["rear_shell"].val()])
    bb = shells.BoundingBox()
    spec = tuple(round(v, 6) + 0.0 for v in (bb.xmin, -bb.ymax, -bb.zmax, bb.xmax, -bb.ymin, -bb.zmin))  # back to the specification frame
    rep.check(f"envelope is {w} x {h} x {t} mm", abs(spec[0]) < EPS_LENGTH and abs(spec[1]) < EPS_LENGTH and abs(spec[2]) < EPS_LENGTH
              and abs(spec[3] - w) < EPS_LENGTH and abs(spec[4] - h) < EPS_LENGTH and abs(spec[5] - t) < EPS_LENGTH,
              f"x {spec[0]:.3f}..{spec[3]:.3f}, y {spec[1]:.3f}..{spec[4]:.3f}, z {spec[2]:.3f}..{spec[5]:.3f}")

    worst = (0.0, None)
    for (an, a), (bn, b) in itertools.combinations(parts.items(), 2):
        try:
            vol = a.val().intersect(b.val()).Volume()
        except Exception:  # the kernel may return nothing for parts that only touch
            vol = 0.0
        if vol > worst[0]:
            worst = (vol, f"{an} / {bn}")
    rep.check("no two parts intersect", worst[0] < EPS_VOLUME, "largest overlap %.6f mm3%s" % (worst[0], f" ({worst[1]})" if worst[1] else ""))

    front, rear = parts["front_shell"], parts["rear_shell"]
    seam = t / 2.0

    # the openings that must exist
    cx_lcd = params["lcd_opening"]["x"] + params["lcd_opening"]["width"] / 2.0
    cy_lcd = params["lcd_opening"]["y"] + params["lcd_opening"]["height"] / 2.0
    rep.check("the LCD window is open", not inside(front, cx_lcd, cy_lcd, wall / 2.0), f"centre ({cx_lcd:.1f}, {cy_lcd:.1f})")

    for name, (cx, cy, d) in zip(NAMES, key_positions(params)):
        ok = (not inside(front, cx, cy, wall / 2.0)) and inside(parts[name], cx, cy, wall / 2.0)
        rep.check(f"{name} sits in its hole", ok, f"({cx}, {cy}), diameter {d}")

    usb = params["usb_c"]
    ok = not inside(front, usb["center_x"], h - wall / 2.0, t / 2.0 - 0.1) and not inside(rear, usb["center_x"], h - wall / 2.0, t / 2.0 + 0.1)
    rep.check("the USB-C cut-out is open on the bottom edge", ok, f"x={usb['center_x']}")

    sp = params["speaker_grille"]
    asm = params["prototype_assumptions"]
    pitch, hole_d = asm["grille_pitch"], asm["grille_hole_diameter"]
    cols = int((sp["envelope"][0] - hole_d) // pitch) + 1
    rows = int((sp["envelope"][1] - hole_d) // pitch) + 1
    x0 = sp["center"][0] - (cols - 1) * pitch / 2.0
    y0 = sp["center"][1] - (rows - 1) * pitch / 2.0
    holes_open = [not inside(front, x0 + i * pitch, y0 + j * pitch, wall / 2.0) for i in range(cols) for j in range(rows)]
    webs_closed = [inside(front, x0 + (i + 0.5) * pitch, y0 + (j + 0.5) * pitch, wall / 2.0) for i in range(cols - 1) for j in range(rows - 1)]
    rep.check("the front speaker grille is perforated and the webs between its holes are solid", all(holes_open) and all(webs_closed),
              f"{cols} x {rows} holes of {hole_d} mm, pitch {pitch} mm (placeholder pattern)")

    # the walls that must stay closed, by volume: any opening of any size changes the volume of the region it cuts
    import math

    r = body["corner_radius"]
    seam = t / 2.0
    tol = 1e-2  # mm3

    def region_volume(shell, region):
        return shell.val().intersect(region.val()).Volume()

    # front plate: only the LCD window, the eight key holes and the speaker-grille holes pass through it
    plate = rounded_box(0, 0, 0, w, h, wall, r)
    lcd = params["lcd_opening"]
    keys_area = sum(math.pi * (d / 2.0 + asm["hole_clearance"]) ** 2 for _, _, d in key_positions(params))
    grille_area = cols * rows * math.pi * (hole_d / 2.0) ** 2
    expected = plate.val().Volume() - wall * (lcd["width"] * lcd["height"] + keys_area + grille_area)
    actual = region_volume(front, plate)
    rep.check("the front plate has only the LCD window, eight key holes and the grille holes", abs(actual - expected) < tol,
              f"plate volume {actual:.2f} mm3, expected {expected:.2f} mm3")

    # rear plate: no hole at all (no speaker outlet, no vent)
    rear_plate = rounded_box(0, 0, t - wall, w, h, t, r)
    actual = region_volume(rear, rear_plate)
    rep.check("the rear plate is closed (no holes, no speaker outlet)", abs(actual - rear_plate.val().Volume()) < tol,
              f"plate volume {actual:.2f} mm3 of {rear_plate.val().Volume():.2f} mm3")

    # side, top and bottom walls: closed except the USB-C cut-out, which takes half of its volume from each shell
    cw, ch = usb["cutout_placeholder"]
    usb_half = cw * wall * ch / 2.0
    ring_front = rounded_box(0, 0, 0, w, h, seam, r).cut(rounded_box(wall, wall, -1.0, w - wall, h - wall, seam + 1.0, r - wall))
    ring_rear = rounded_box(0, 0, seam, w, h, t, r).cut(rounded_box(wall, wall, seam - 1.0, w - wall, h - wall, t + 1.0, r - wall))
    off = []
    for name, shell, ring in (("front", front, ring_front), ("rear", rear, ring_rear)):
        diff = ring.val().Volume() - usb_half - region_volume(shell, ring)
        if abs(diff) >= tol:
            off.append(f"{name} {diff:+.2f} mm3")
    rep.check("the side, top and bottom walls have no opening besides the USB-C cut-out", not off,
              "volume differs from the closed walls by " + ", ".join(off) if off else f"USB-C cut-out {cw} x {ch} mm only")


def write_report(path, params, t, rep, versions, params_hash):
    lines = [
        "# Проверка параметрической модели корпуса (генерируется)",
        "",
        "Файл создаёт `mechanical/src/build.py`; **не править вручную**. Он проверяет генератор и согласованность чисел, а **не посадку",
        "в руке, не дисплей и не детали**: все размеры из `params.json` — предложенные (PROPOSED), корпус не проверялся на физическом образце.",
        "",
        f"* Ревизия параметров: `{params['m_rev']}`, SHA-256 `params.json`: `{params_hash}`",
        f"* Толщина: {t} мм, шов посередине толщины (допущение для макета)",
        f"* Инструменты: {versions}",
        "",
        "| Проверка | Результат | Подробности |",
        "| --- | --- | --- |",
    ]
    for name, ok, detail in rep.rows:
        lines.append(f"| {name} | {'PASS' if ok else '**FAIL**'} | {detail} |")
    lines += [
        "",
        "## Не моделируется (решения не приняты)",
        "",
    ] + [f"* {item}" for item in params["not_decided"]]
    lines += ["", "Единственные источники истины остаются `params.json` и код в `mechanical/src/`; экспорт (STEP/STL) создаётся этой же командой и в git не хранится."]
    Path(path).write_text("\n".join(lines) + "\n", encoding="utf-8")


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--thickness", type=float, default=None, help="20 or 22 mm (default from params.json)")
    ap.add_argument("--out", default=str(PARAMS_PATH.parent / "build"), help="directory for STEP and STL files")
    ap.add_argument("--report", default=None, help="write a Markdown validation report here")
    ap.add_argument("--no-export", action="store_true")
    args = ap.parse_args(argv)

    params = load()
    t = thickness(params, args.thickness)

    rep = Report()
    pre = check_params.run(params)
    rep.check("parameter checks (check_params.py)", all(ok for _, ok, _ in pre), f"{sum(1 for _, ok, _ in pre if ok)} of {len(pre)} passed")

    parts = build_parts(params, t)
    validate(params, t, parts, rep)

    if not args.no_export:
        out = Path(args.out)
        out.mkdir(parents=True, exist_ok=True)
        compound = cq.Compound.makeCompound([p.val() for p in parts.values()])
        cq.exporters.export(cq.Workplane("XY").add(compound), str(out / f"brick-r02-t{int(t)}-assembly.step"))
        for name, part in parts.items():
            cq.exporters.export(part, str(out / f"{name}.step"))
            cq.exporters.export(part, str(out / f"{name}.stl"), tolerance=0.01, angularTolerance=0.1)
        print(f"exported {len(parts)} parts and the assembly to {out}")

    if args.report:
        versions = f"CadQuery {cq.__version__}, Python {platform.python_version()}"
        write_report(args.report, params, t, rep, versions, hashlib.sha256(PARAMS_PATH.read_bytes()).hexdigest())

    print(f"{len(rep.rows) - len(rep.failed)} of {len(rep.rows)} checks passed")
    return 1 if rep.failed else 0


if __name__ == "__main__":
    sys.exit(main())
