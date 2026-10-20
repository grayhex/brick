#!/usr/bin/env python3
"""Checks of mechanical/params.json that need no CAD kernel (standard library only).

What is checked: the envelope, that every opening lies inside the body with a wall of material around it, that the three
function keys sit under the LCD window, the open gap between the four direction keys and between them and the action key,
and the front-only speaker grille. A failure means the numbers contradict each other or a CONFIRMED requirement; success
means only that the numbers are consistent, NOT that the case fits a hand or a component.

Usage: python3 mechanical/src/check_params.py [params.json] [--markdown FILE]
"""
import json
import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
DEFAULT = HERE.parent / "params.json"


def circle_box(center, d):
    r = d / 2.0
    return (center[0] - r, center[1] - r, center[0] + r, center[1] + r)


def gap(a, da, b, db):
    """Open gap between two circles: distance of centres minus both radii."""
    return math.hypot(a[0] - b[0], a[1] - b[1]) - da / 2.0 - db / 2.0


def run(params):
    results = []

    def check(name, ok, detail):
        results.append((name, bool(ok), detail))

    body = params["body"]
    w, h = body["width"], body["height"]
    wall = body["wall_prototype"]
    check("body envelope is 80 x 175 mm", (w, h) == (80, 175), f"{w} x {h}")
    check("thickness options are 20 and 22 mm", body["thickness_options"] == [20, 22] and body["thickness_default"] in (20, 22),
          f"{body['thickness_options']}, default {body['thickness_default']}")
    check("corner radius within 8..11 mm", body["corner_radius_range"][0] <= body["corner_radius"] <= body["corner_radius_range"][1],
          f"{body['corner_radius']}")

    def inside_with_wall(box, label):
        x0, y0, x1, y1 = box
        ok = x0 >= wall and y0 >= wall and x1 <= w - wall and y1 <= h - wall
        check(f"{label} lies inside the body with a {wall} mm wall", ok, f"x {x0:.2f}..{x1:.2f}, y {y0:.2f}..{y1:.2f}")

    lcd = params["lcd_opening"]
    lcd_box = (lcd["x"], lcd["y"], lcd["x"] + lcd["width"], lcd["y"] + lcd["height"])
    inside_with_wall(lcd_box, "LCD window")
    cand = params["lcd_candidate"]
    check("LCD candidate outline fits behind the window", cand["outline"][0] <= lcd["width"] and cand["outline"][1] <= lcd["height"],
          f"outline {cand['outline'][0]} x {cand['outline'][1]} in window {lcd['width']} x {lcd['height']}")
    check("LCD candidate active area fits the window", cand["active_area"][0] <= lcd["width"] and cand["active_area"][1] <= lcd["height"],
          f"{cand['active_area'][0]} x {cand['active_area'][1]}")

    fk = params["function_keys"]
    names = ["MODE", "SELECT", "START"]
    fcent = [fk["centers"][n] for n in names]
    for n, c in zip(names, fcent):
        inside_with_wall(circle_box(c, fk["diameter"]), f"function key {n}")
        check(f"function key {n} is under the LCD window", c[1] - fk["diameter"] / 2.0 > lcd_box[3], f"top edge y={c[1] - fk['diameter'] / 2.0:.2f} > window bottom {lcd_box[3]}")
        check(f"function key {n} is inside the label zone", fk["label_zone_y"][0] <= c[1] <= fk["label_zone_y"][1], f"y={c[1]}")
    check("function keys are in one row", len({c[1] for c in fcent}) == 1, f"y={[c[1] for c in fcent]}")
    gaps = [gap(fcent[i], fk["diameter"], fcent[i + 1], fk["diameter"]) for i in range(2)]
    check("function keys do not touch", min(gaps) > 0, f"gaps {', '.join(f'{g:.2f}' for g in gaps)} mm")

    dk = params["direction_keys"]
    d = dk["diameter"]
    check("direction key diameter within its range", dk["diameter_range"][0] <= d <= dk["diameter_range"][1], f"{d}")
    dnames = ["UP", "LEFT", "RIGHT", "DOWN"]
    dc = {n: dk["centers"][n] for n in dnames}
    for n in dnames:
        inside_with_wall(circle_box(dc[n], d), f"direction key {n}")
        check(f"direction key {n} is below the function keys", dc[n][1] - d / 2.0 > max(c[1] for c in fcent) + fk["diameter"] / 2.0, f"y={dc[n][1]}")
    pairs = [(a, b) for i, a in enumerate(dnames) for b in dnames[i + 1:]]
    worst = min((gap(dc[a], d, dc[b], d), a, b) for a, b in pairs)
    check(f"open gap between direction keys >= {dk['min_open_gap']} mm", worst[0] >= dk["min_open_gap"], f"smallest {worst[0]:.2f} mm ({worst[1]}-{worst[2]})")
    check("direction keys are the four points of a symmetric diamond (no shared cross)", dc["LEFT"][1] == dc["RIGHT"][1] and dc["UP"][0] == dc["DOWN"][0] == (dc["LEFT"][0] + dc["RIGHT"][0]) / 2.0,
          f"{dc}")

    ak = params["action_key"]
    inside_with_wall(circle_box(ak["center"], ak["diameter"]), "action key")
    check("action key diameter within its range", ak["diameter_range"][0] <= ak["diameter"] <= ak["diameter_range"][1], f"{ak['diameter']}")
    check("action key is the largest key", ak["diameter"] > max(d, fk["diameter"]), f"{ak['diameter']} mm")
    ag = min(gap(ak["center"], ak["diameter"], dc[n], d) for n in dnames)
    check(f"open gap between the action key and the direction keys >= {dk['min_open_gap']} mm", ag >= dk["min_open_gap"], f"smallest {ag:.2f} mm")

    sp = params["speaker_grille"]
    sx, sy = sp["center"]
    sbox = (sx - sp["envelope"][0] / 2.0, sy - sp["envelope"][1] / 2.0, sx + sp["envelope"][0] / 2.0, sy + sp["envelope"][1] / 2.0)
    inside_with_wall(sbox, "speaker grille")
    check("speaker grille is below every key", sbox[1] > max(dc[n][1] + d / 2.0 for n in dnames) and sbox[1] > ak["center"][1] + ak["diameter"] / 2.0,
          f"top edge y={sbox[1]:.2f}")
    check("speaker grille is centred on the body width", abs(sx - w / 2.0) < 1e-9, f"x={sx}")
    check("no speaker holes on the rear or the bottom", sp["rear_or_bottom_holes"] == 0, f"{sp['rear_or_bottom_holes']}")

    usb = params["usb_c"]
    check("USB-C is centred on the bottom edge and is the only opening there", usb["edge"] == "bottom" and abs(usb["center_x"] - w / 2.0) < 1e-9 and usb["openings_on_bottom_edge"] == 1,
          f"x={usb['center_x']}, openings={usb['openings_on_bottom_edge']}")
    check("USB-C cut-out placeholder is narrower than the body", usb["cutout_placeholder"][0] < w - 2 * wall, f"{usb['cutout_placeholder']}")

    fin = params["finishes"]
    check("three finishes share one geometry", fin["geometry_is_shared"] and sorted(fin["names"]) == ["CERAMIC", "CLEAR_SMOKE", "MONOLITH"], f"{fin['names']}")
    return results


def markdown(results, params):
    lines = [f"Parameters `{params['m_rev']}` ({params['status']}).", "", "| Check | Result | Detail |", "| --- | --- | --- |"]
    for name, ok, detail in results:
        lines.append(f"| {name} | {'PASS' if ok else '**FAIL**'} | {detail} |")
    return "\n".join(lines) + "\n"


def main(argv):
    args = argv[1:]
    out = None
    if "--markdown" in args:
        i = args.index("--markdown")
        out = args[i + 1]
        del args[i:i + 2]
    path = Path(args[0]) if args else DEFAULT
    params = json.loads(path.read_text(encoding="utf-8"))
    results = run(params)
    failed = [r for r in results if not r[1]]
    for name, ok, detail in results:
        print(f"{'PASS' if ok else 'FAIL'}  {name}: {detail}")
    print(f"{len(results) - len(failed)} of {len(results)} checks passed")
    if out:
        Path(out).write_text(markdown(results, params), encoding="utf-8")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
