#!/usr/bin/env python3
"""Measure the first Brick enclosure mockup (binary STL parts) and compare it with the R0.2 working values.

Input is a directory or a .zip with files named `untitled-part-<name>.stl` (the buildcad.ai export, see
mechanical/inbox/buildcad-r0.1/README.md). Everything is derived from the triangles: no values are typed in from a
screenshot. The parts lie on a build plate, so the tool maps the front shell to the R0.2 frame of
docs/MECHANICAL_CONCEPT.md (origin: top-left outer corner of the face, X right, Y down, mm) under this assumption,
which is checked (the LCD window must land in the upper half):

    device_x = Y - Y_min          (looking at the outer face, device-right is +Y)
    device_y = X_max - X          (device-top is the +X end)

The analysis is specific to this mockup's structure (hollow shells with a half-lap parting line, through-holes for the
keys). It is a review aid, not a CAD kernel: a different export needs the part names and thresholds revisited.

Usage:
    python3 -I tools/cad_review/stl_review.py <dir|zip> [--json out.json] [--png-dir DIR] [--verify baseline.json]

Dependencies: numpy, Pillow.
"""
import argparse
import io
import json
import math
import sys
import zipfile
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

PART_PREFIX = "untitled-part-"
SCALE = 10  # px per mm for the plan-view masks (0.1 mm resolution)

# R0.2 working values, docs/MECHANICAL_CONCEPT.md section 2. PROPOSED, not approved production tolerances.
R02 = {
    "body": {"w": 80.0, "h": 175.0, "t": 20.0, "t_alt": 22.0, "radius": (8.0, 11.0)},
    "window": {"x": 13.0, "y": 9.0, "w": 54.0, "h": 82.0},
    "function": {"centers": [(25.0, 101.0), (40.0, 101.0), (55.0, 101.0)], "d": 6.5},
    "direction": {"centers": {"up": (24.0, 119.0), "left": (12.5, 130.5), "right": (35.5, 130.5), "down": (24.0, 142.0)},
                  "d": (8.5, 9.0), "min_gap": 5.0},
    "action": {"center": (59.0, 130.5), "d": (21.0, 22.0)},
    "speaker": {"center": (40.0, 163.0), "w": 22.0, "h": 9.0},
    "usb_x": 40.0,
    "lcd": {"glass": (47.02, 76.0), "active": (42.672, 68.072)},
}


def load_parts(src):
    p = Path(src)
    raw = {}
    if p.is_dir():
        for f in sorted(p.glob(PART_PREFIX + "*.stl")):
            raw[f.name] = f.read_bytes()
    else:
        with zipfile.ZipFile(p) as z:
            for name in sorted(z.namelist()):
                base = name.rsplit("/", 1)[-1]
                if base.startswith(PART_PREFIX) and base.endswith(".stl"):
                    raw[base] = z.read(name)
    parts = {}
    for fname, d in raw.items():
        n = int(np.frombuffer(d, "<u4", 1, 80)[0])
        if 84 + 50 * n != len(d):
            raise SystemExit(f"{fname}: not a binary STL ({n} triangles, {len(d)} bytes)")
        rec = np.frombuffer(d, np.dtype([("n", "<f4", 3), ("v", "<f4", (3, 3)), ("a", "<u2")]), n, 84)
        parts[fname[len(PART_PREFIX):-4]] = rec["v"].astype(np.float64)
    if not parts:
        raise SystemExit("no untitled-part-*.stl found in " + str(src))
    return parts


def bbox(t):
    v = t.reshape(-1, 3)
    return v.min(0), v.max(0)


def volume_mm3(t):
    a, b, c = t[:, 0], t[:, 1], t[:, 2]
    return float(np.einsum("ij,ij->i", a, np.cross(b, c)).sum() / 6.0)


def open_edges(t):
    """Number of edges not shared by exactly two triangles (vertices welded at 1 um)."""
    _, ids = np.unique(np.round(t.reshape(-1, 3), 3), axis=0, return_inverse=True)
    ids = ids.reshape(-1, 3)
    e = np.concatenate([ids[:, [0, 1]], ids[:, [1, 2]], ids[:, [2, 0]]])
    e.sort(axis=1)
    _, cnt = np.unique(e, axis=0, return_counts=True)
    return int((cnt != 2).sum())


def face_normals(t):
    n = np.cross(t[:, 1] - t[:, 0], t[:, 2] - t[:, 0])
    ln = np.linalg.norm(n, axis=1, keepdims=True)
    return n / np.maximum(ln, 1e-12), ln[:, 0] / 2.0


def flat_faces(t, axis, level, sign=0, tol=0.02):
    """Triangles lying in the plane coordinate[axis] == level (normal along +-axis; sign 0 = either)."""
    nn, _ = face_normals(t)
    m = np.all(np.abs(t[:, :, axis] - level) < tol, axis=1) & (np.abs(nn[:, axis]) > 0.99)
    if sign:
        m &= np.sign(nn[:, axis]) == sign
    return t[m]


def flat_levels(t, axis, min_area):
    """{(level, sign): area} of planes with normal along the axis, summed over triangles."""
    nn, area = face_normals(t)
    m = np.abs(nn[:, axis]) > 0.99
    out = {}
    for lv, s, a in zip(np.round(t[m][:, 0, axis], 2), np.sign(nn[m, axis]).astype(int), area[m]):
        out[(float(lv), int(s))] = out.get((float(lv), int(s)), 0.0) + float(a)
    return {k: v for k, v in sorted(out.items()) if v >= min_area}


def plan_mask(t, lo, hi, scale=SCALE):
    w = int((hi[0] - lo[0]) * scale) + 3
    h = int((hi[1] - lo[1]) * scale) + 3
    im = Image.new("1", (w, h), 0)
    dr = ImageDraw.Draw(im)
    for tri in t[:, :, :2]:
        dr.polygon([((q[0] - lo[0]) * scale + 1, (q[1] - lo[1]) * scale + 1) for q in tri], fill=1)
    return np.array(im, dtype=bool)


def background_components(m):
    """4-connected components of the False pixels: [(area_px, x0, x1, y0, y1, touches_border)]."""
    h, w = m.shape
    bg = ~m
    runs, rows = [], []
    for y in range(h):
        d = np.diff(np.concatenate(([0], bg[y].astype(np.int8), [0])))
        s, e = np.nonzero(d == 1)[0], np.nonzero(d == -1)[0]
        rows.append(list(range(len(runs), len(runs) + len(s))))
        runs.extend((y, int(a), int(b)) for a, b in zip(s, e))
    parent = list(range(len(runs)))

    def find(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i

    for y in range(1, h):
        a, b = rows[y - 1], rows[y]
        i = j = 0
        while i < len(a) and j < len(b):
            ra, rb = runs[a[i]], runs[b[j]]
            if ra[1] < rb[2] and rb[1] < ra[2]:
                parent[find(a[i])] = find(b[j])
            if ra[2] < rb[2]:
                i += 1
            else:
                j += 1
    comp = {}
    for i, (y, a, b) in enumerate(runs):
        c = comp.setdefault(find(i), [0, a, b, y, y, False])
        c[0] += b - a
        c[1], c[2] = min(c[1], a), max(c[2], b)
        c[3], c[4] = min(c[3], y), max(c[4], y)
        if y in (0, h - 1) or a == 0 or b == w:
            c[5] = True
    return [tuple(c) for c in comp.values()]


def slice_x(t, c):
    """Intersection of the mesh with the plane X == c as segments in (Y, Z): array (m, 2, 2)."""
    d = t[:, :, 0] - c
    neg = d < 0
    cuts, pts = [], []
    for e0, e1 in ((0, 1), (1, 2), (2, 0)):
        cut = neg[:, e0] != neg[:, e1]
        k = np.where(cut, d[:, e0] / np.where(cut, d[:, e0] - d[:, e1], 1.0), 0.0)
        cuts.append(cut)
        pts.append((t[:, e0] + (t[:, e1] - t[:, e0]) * k[:, None])[:, 1:])
    cuts = np.stack(cuts, axis=1)
    segs = [[pts[e][i] for e in range(3) if cuts[i, e]] for i in np.nonzero(cuts.sum(1) == 2)[0]]
    return np.array(segs)


def merge_boxes(lo, hi, eps=0.05):
    """Union of overlapping axis-aligned boxes (one per triangle) into connected groups: [[lo, hi], ...]."""
    n = len(lo)
    parent = list(range(n))

    def find(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i

    for i in range(n):
        for j in np.nonzero(np.all((lo[i] <= hi + eps) & (hi[i] >= lo - eps), axis=1))[0]:
            parent[find(int(j))] = find(i)
    groups = {}
    for i in range(n):
        g = groups.setdefault(find(i), [lo[i].copy(), hi[i].copy()])
        g[0], g[1] = np.minimum(g[0], lo[i]), np.maximum(g[1], hi[i])
    return list(groups.values())


def hole_extent(lo, x0, x1, y0, y1):
    """Plate-frame extent (X0, X1, Y0, Y1) of a background component of a plan mask.

    Polygon fill covers the boundary pixels, so a hole measures one pixel (0.1 mm) too small; half a pixel is added on
    each side. Checked against vertex-exact sizes of this mockup (window 54.0 x 76.0, key hole 10.30, action 22.30);
    positions are good to about +-0.1 mm (sub-pixel quantisation).
    """
    return (lo[0] + (x0 - 1.5) / SCALE, lo[0] + (x1 - 0.5) / SCALE, lo[1] + (y0 - 1.5) / SCALE, lo[1] + (y1 + 0.5) / SCALE)


def to_device(x, y, org, length):
    return y - org[1], org[0] + length - x


class Review:
    def __init__(self, parts):
        self.parts = parts
        lo, hi = bbox(parts["front"])
        self.org = (lo[0], lo[1])
        self.length = hi[0] - lo[0]
        self.width = hi[1] - lo[1]

    def part_table(self):
        rows = []
        for name, t in self.parts.items():
            lo, hi = bbox(t)
            rows.append({"part": name, "triangles": len(t), "size_mm": [round(float(v), 2) for v in hi - lo],
                         "volume_cm3": round(volume_mm3(t) / 1000.0, 2), "open_edges": open_edges(t)})
        return rows

    def through_holes(self):
        t = self.parts["front"]
        lo, hi = bbox(t)
        mask = plan_mask(t, lo, hi)
        out = []
        for area, x0, x1, y0, y1, border in background_components(mask):
            if border or area < 20:
                continue
            bx0, bx1, by0, by1 = hole_extent(lo, x0, x1, y0, y1)
            dx0, dy1 = to_device(bx0, by0, self.org, self.length)  # device (x, y) of the corner (X min, Y min)
            dx1, dy0 = to_device(bx1, by1, self.org, self.length)
            px = (x1 - x0, y1 - y0 + 1)
            out.append({"x0": round(float(dx0), 1), "x1": round(float(dx1), 1), "y0": round(float(dy0), 1), "y1": round(float(dy1), 1),
                        "w": round(float(dx1 - dx0), 1), "h": round(float(dy1 - dy0), 1),
                        "area_mm2": round(float((area + px[0] + px[1] + 1) / SCALE ** 2), 1)})
        return out

    @staticmethod
    def classify(holes):
        res = {"window": None, "round": [], "slots": [], "slits": []}
        for h in sorted(holes, key=lambda h: -h["area_mm2"]):
            small = min(h["w"], h["h"])
            if h["area_mm2"] > 1000:
                res["window"] = h
            elif small < 2.0:
                res["slits"].append(h)
            elif abs(h["w"] - h["h"]) < 0.4:
                res["round"].append(h)
            else:
                res["slots"].append(h)
        return res

    def stack(self):
        f, r = self.parts["front"], self.parts["rear"]
        out = {}
        for name, t in (("front", f), ("rear", r)):
            lv = {z: a for (z, s), a in flat_levels(t, 2, 300.0).items() if s > 0}
            plate = max(lv, key=lv.get)  # the largest up-facing plane is the inner surface of the plate
            above = sorted(z for z in lv if z > plate)
            # wall shoulder and wall rim: the two highest large up-facing planes above the plate
            out[name] = {"plate_inner": plate, "shoulder": above[-2], "rim": above[-1],
                         "depth_incl_bosses": round(float(bbox(t)[1][2]), 2)}
        out["thickness_front_rim_on_rear_shoulder"] = round(out["front"]["rim"] + out["rear"]["shoulder"], 2)
        out["thickness_rear_rim_on_front_shoulder"] = round(out["rear"]["rim"] + out["front"]["shoulder"], 2)
        out["butt_joint_sum"] = round(out["front"]["depth_incl_bosses"] + out["rear"]["depth_incl_bosses"], 2)
        return out

    def rim_notches(self):
        """Short stretches where the front wall rim is lower than the main rim."""
        f = self.parts["front"]
        main = self.stack()["front"]["rim"]
        out = []
        for lv in sorted({z for (z, s) in flat_levels(f, 2, 5.0) if s > 0 and 9.5 < z < main - 0.2}):
            tri = flat_faces(f, 2, lv, +1)
            lo, hi = bbox(tri)
            if hi[0] - lo[0] > 40:
                continue
            out.append({"side": "left" if (lo[1] + hi[1]) / 2 < self.org[1] + self.width / 2 else "right",
                        "device_y": [round(float(self.org[0] + self.length - hi[0]), 1),
                                     round(float(self.org[0] + self.length - lo[0]), 1)],
                        "length": round(float(hi[0] - lo[0]), 1), "depth": round(main - lv, 2)})
        return out

    def bosses(self):
        """Screw bosses (tops of the tallest features) of both shells and the through-holes of the rear shell.

        Coordinates stay in the plate frame (X along the length, Y across); the rear shell is shifted by its Y offset
        so that both shells can be compared directly.
        """
        res = {}
        for name in ("front", "rear"):
            t = self.parts[name]
            top = float(bbox(t)[1][2])
            tris = flat_faces(t, 2, top, +1)[:, :, :2]
            boxes = merge_boxes(tris.min(1), tris.max(1))
            items = [{"x": round(float((b[0][0] + b[1][0]) / 2), 2), "y": round(float((b[0][1] + b[1][1]) / 2), 2),
                      "d": round(float((b[1] - b[0]).max()), 2)} for b in boxes]
            res[name] = {"top_z": round(top, 2), "items": sorted(items, key=lambda i: (i["x"], i["y"]))}
        r = self.parts["rear"]
        lo, hi = bbox(r)
        shift = lo[1] - self.org[1]
        holes = [hole_extent(lo, x0, x1, y0, y1) for area, x0, x1, y0, y1, border
                 in background_components(plan_mask(r, lo, hi)) if not border and area >= 20]
        res["rear_holes"] = sorted(({"x": round(float((a + b) / 2), 1), "y": round(float((c + d) / 2 - shift), 1),
                                     "d": round(float(max(b - a, d - c)), 1)} for a, b, c, d in holes),
                                   key=lambda i: (i["x"], i["y"]))
        return res

    def usb_cutout(self):
        """Opening in the end wall nearest to the bottom of the device, from the +-Z faces inside the wall."""
        f = self.parts["front"]
        lo, _ = bbox(f)
        nn, _ = face_normals(f)
        centre = self.org[1] + self.width / 2
        sel = ((np.abs(nn[:, 2]) > 0.99) & (f[:, :, 0].max(1) < lo[0] + 2.45) & (f[:, :, 2].min(1) > 1.9)
               & (np.abs(f[:, :, 1].mean(1) - centre) < 6))
        groups = []  # [facing, z_values, x0, x1, y0, y1]
        for i in sorted(np.nonzero(sel)[0], key=lambda i: f[i, 0, 2]):
            tri, z, sg = f[i], float(f[i, 0, 2]), int(np.sign(nn[i, 2]))
            for g in groups:
                if g[0] == sg and abs(g[1][-1] - z) < 0.1 and abs(g[2] - (tri[:, 0].min() - lo[0])) < 1.5:
                    g[1].append(z)
                    g[2], g[3] = min(g[2], tri[:, 0].min() - lo[0]), max(g[3], tri[:, 0].max() - lo[0])
                    g[4], g[5] = min(g[4], tri[:, 1].min() - self.org[1]), max(g[5], tri[:, 1].max() - self.org[1])
                    break
            else:
                groups.append([sg, [z], tri[:, 0].min() - lo[0], tri[:, 0].max() - lo[0],
                               tri[:, 1].min() - self.org[1], tri[:, 1].max() - self.org[1]])
        return [{"z": round(float(np.median(g[1])), 2), "facing": "+z" if g[0] > 0 else "-z",
                 "wall_x": [round(g[2], 1), round(g[3], 1)], "device_x": [round(g[4], 1), round(g[5], 1)]} for g in groups]

    def lens_seat(self, window):
        """Recess for the lens: up-facing ledge between the outer face and the inner plate surface, around the window."""
        f = self.parts["front"]
        plate = self.stack()["front"]["plate_inner"]
        nn, _ = face_normals(f)
        cx, cy = self.org[0] + self.length - (window["y0"] + window["y1"]) / 2, self.org[1] + (window["x0"] + window["x1"]) / 2
        near = (np.abs(f[:, :, 0].mean(1) - cx) < window["h"] / 2 + 6) & (np.abs(f[:, :, 1].mean(1) - cy) < window["w"] / 2 + 6)
        m = near & (nn[:, 2] > 0.99) & np.all((f[:, :, 2] > 0.2) & (f[:, :, 2] < plate - 0.2), axis=1)
        if not m.any():
            return None
        lo, hi = bbox(f[m])
        x0, y1 = to_device(lo[0], lo[1], self.org, self.length)
        x1, y0 = to_device(hi[0], hi[1], self.org, self.length)
        ledge = float(f[m][0, 0, 2])
        return {"ledge_z": round(ledge, 2), "recess_depth": round(plate - ledge, 2),
                "x": [round(x0, 1), round(x1, 1)], "y": [round(y0, 1), round(y1, 1)]}

    def caps(self):
        res = {}
        for name, t in self.parts.items():
            if name in ("front", "rear", "lens"):
                continue
            v = t.reshape(-1, 3)
            zs = np.unique(np.round(v[:, 2], 2))
            ext = []
            for z in zs:
                m = np.abs(v[:, 2] - z) < 0.006
                if m.sum() >= 100:
                    ext.append((float(z), float(np.ptp(v[m, 0])), float(np.ptp(v[m, 1]))))
            if not ext:
                continue
            # flange = widest section at the lowest z; stem = the first narrower plateau above it
            fl = ext[0]
            stem = next((e for e in ext if e[1] < fl[1] - 0.5 or e[2] < fl[2] - 0.5), fl)
            res[name] = {"flange_mm": [round(fl[1], 2), round(fl[2], 2)], "flange_top_z": round(stem[0], 2),
                         "stem_mm": [round(stem[1], 2), round(stem[2], 2)], "height_mm": round(float(v[:, 2].max()), 2)}
        return res


def build(rv):
    holes = rv.through_holes()
    cl = rv.classify(holes)
    r = {"parts": rv.part_table(), "front_origin_xy": [round(float(rv.org[0]), 2), round(float(rv.org[1]), 2)],
         "envelope_mm": [round(float(rv.width), 2), round(float(rv.length), 2)]}
    win = cl["window"]
    r["window_in_upper_half"] = bool(win and (win["y0"] + win["y1"]) / 2 < rv.length / 2)
    r["window"] = win
    r["round_holes"] = sorted(cl["round"], key=lambda h: (-(h["y0"] + h["y1"]) / 2 if h["w"] > 15 else 0, h["y0"], h["x0"]))
    r["slots"] = sorted(cl["slots"], key=lambda h: h["x0"])
    if cl["slits"]:
        xs0, xs1 = min(h["x0"] for h in cl["slits"]), max(h["x1"] for h in cl["slits"])
        ys0, ys1 = min(h["y0"] for h in cl["slits"]), max(h["y1"] for h in cl["slits"])
        r["speaker"] = {"slits": len(cl["slits"]), "slit_w": round(float(np.mean([h["w"] for h in cl["slits"]])), 2),
                        "x": [xs0, xs1], "y": [ys0, ys1], "open_area_mm2": round(sum(h["area_mm2"] for h in cl["slits"]), 1)}
    r["stack"] = rv.stack()
    r["rim_notches"] = rv.rim_notches()
    r["bosses"] = rv.bosses()
    r["usb"] = rv.usb_cutout()
    r["lens_seat"] = rv.lens_seat(win)
    lo, hi = bbox(rv.parts["lens"])
    r["lens_mm"] = [round(float(v), 2) for v in (hi - lo)[:2]] + [round(float((hi - lo)[2]), 2)]
    r["caps"] = rv.caps()
    r["corner_radius_mm"] = corner_radius(rv.parts["front"])
    return r


def corner_radius(t):
    lo, hi = bbox(t)
    m = plan_mask(t, lo, hi, 20)
    h, w = m.shape
    ds = []
    for sx, sy, x0, y0 in ((1, 1, 1, 1), (-1, 1, w - 2, 1), (1, -1, 1, h - 2), (-1, -1, w - 2, h - 2)):
        for k in range(0, 4000):
            if m[y0 + sy * k, x0 + sx * k]:
                ds.append(k * math.sqrt(2) / 20.0 / (math.sqrt(2) - 1))
                break
    return round(float(np.mean(ds)), 1)


def compare(r):
    """Rows of (item, mockup, R0.2 working value, delta text)."""
    rows = []
    w = r["window"]
    s = R02["window"]
    rows.append(("LCD window (x, y, w, h)", f'{w["x0"]}, {w["y0"]}, {w["w"]}, {w["h"]}',
                 f'{s["x"]}, {s["y"]}, {s["w"]}, {s["h"]}',
                 f'dx {w["x0"]-s["x"]:+.1f}, dy {w["y0"]-s["y"]:+.1f}, dw {w["w"]-s["w"]:+.1f}, dh {w["h"]-s["h"]:+.1f}'))
    g = R02["lcd"]["glass"]
    a = R02["lcd"]["active"]
    rows.append(("Window vs Sharp glass 47.02 x 76.00", f'{w["w"]} x {w["h"]}', "glass 47.02 x 76.00",
                 f'side margin {(w["w"]-g[0])/2:.1f} mm, glass length - window = {g[1]-w["h"]:+.1f} mm'))
    rows.append(("Window vs active 42.672 x 68.072", f'{w["w"]} x {w["h"]}', "active 42.672 x 68.072",
                 f'visible non-active border {(w["w"]-a[0])/2:.1f} mm each side, {(w["h"]-a[1])/2:.1f} mm top/bottom'))
    rnd = [h for h in r["round_holes"] if h["w"] < 15]
    big = [h for h in r["round_holes"] if h["w"] >= 15]
    cen = lambda h: ((h["x0"] + h["x1"]) / 2, (h["y0"] + h["y1"]) / 2)
    if len(rnd) == 4:
        cs = sorted((cen(h) for h in rnd), key=lambda c: (c[1], c[0]))
        up, left, right, down = cs[0], cs[1], cs[2], cs[3]
        names = (("up", up), ("left", left), ("right", right), ("down", down))
        for n, c in names:
            sp = R02["direction"]["centers"][n]
            rows.append((f"Direction {n} centre", f"({c[0]:.1f}, {c[1]:.1f})", f"({sp[0]}, {sp[1]})",
                         f"dx {c[0]-sp[0]:+.1f}, dy {c[1]-sp[1]:+.1f}"))
        dm = rnd[0]["w"]
        pitch = math.dist(left, right)
        diag = math.dist(up, left)
        rows.append(("Direction hole diameter", f"{dm}", "8.5-9 (spec Ø)", f"{dm-9.0:+.1f} vs Ø9"))
        rows.append(("Direction pitch (opposite keys)", f"{pitch:.1f}", "23.0", f"{pitch-23.0:+.1f}"))
        rows.append(("Direction diagonal neighbour gap (hole edges)", f"{diag-dm:.1f}", ">= 5.0 target", f"{diag-dm-5.0:+.1f}"))
    if big:
        c = cen(big[0])
        sp = R02["action"]["center"]
        rows.append(("Action centre / hole Ø", f"({c[0]:.1f}, {c[1]:.1f}) / Ø{big[0]['w']}", f"({sp[0]}, {sp[1]}) / Ø21-22",
                     f"dx {c[0]-sp[0]:+.1f}, dy {c[1]-sp[1]:+.1f}"))
    for i, h in enumerate(r["slots"]):
        c = cen(h)
        sp = R02["function"]["centers"][i] if i < 3 else (math.nan, math.nan)
        rows.append((f"Function key {i+1} centre / slot", f"({c[0]:.1f}, {c[1]:.1f}) / {h['w']} x {h['h']}",
                     f"({sp[0]}, {sp[1]}) / round Ø6.5", f"dx {c[0]-sp[0]:+.1f}, dy {c[1]-sp[1]:+.1f}"))
    if "speaker" in r:
        sk = r["speaker"]
        cx, cy = sum(sk["x"]) / 2, sum(sk["y"]) / 2
        sp = R02["speaker"]
        rows.append(("Speaker grille centre / size", f"({cx:.1f}, {cy:.1f}) / {sk['x'][1]-sk['x'][0]:.1f} x {sk['y'][1]-sk['y'][0]:.1f}",
                     f"({sp['center'][0]}, {sp['center'][1]}) / {sp['w']} x {sp['h']}",
                     f"dx {cx-sp['center'][0]:+.1f}, dy {cy-sp['center'][1]:+.1f}"))
    st = r["stack"]
    rows.append(("Assembled thickness (half-lap)", f'{st["thickness_front_rim_on_rear_shoulder"]} / {st["thickness_rear_rim_on_front_shoulder"]}',
                 "20 (22 fallback)", f'{st["thickness_front_rim_on_rear_shoulder"]-20:+.2f}'))
    rows.append(("Plan corner radius", f'~{r["corner_radius_mm"]}', "8-11 (concept)", f'{r["corner_radius_mm"]-11:+.1f} vs 11'))
    return rows


def render_front(rv, r, path):
    """Portrait front view in the R0.2 frame: mockup through-holes (black) over the R0.2 working values (red)."""
    sc = 6
    W, H = int(80 * sc) + 40, int(175 * sc) + 40
    im = Image.new("RGB", (W, H), (255, 255, 255))
    dr = ImageDraw.Draw(im)
    ox, oy = 20, 20
    P = lambda x, y: (ox + x * sc, oy + y * sc)
    dr.rounded_rectangle([P(0, 0), P(80, 175)], radius=int(r["corner_radius_mm"] * sc), fill=(232, 232, 232), outline=(90, 90, 90))
    holes = [r["window"]] + r["round_holes"] + r["slots"]
    for h in holes:
        dr.rounded_rectangle([P(h["x0"], h["y0"]), P(h["x1"], h["y1"])], radius=int(min(h["w"], h["h"]) / 2 * sc) if h is not r["window"] else int(2 * sc),
                             fill=(255, 255, 255), outline=(0, 0, 0), width=2)
    if "speaker" in r:
        sk = r["speaker"]
        dr.rectangle([P(sk["x"][0], sk["y"][0]), P(sk["x"][1], sk["y"][1])], outline=(0, 0, 0), width=2)
        n = sk["slits"]
        for i in range(n):
            x = sk["x"][0] + (sk["x"][1] - sk["x"][0]) * (i + 0.5) / n
            dr.line([P(x, sk["y"][0] + 0.5), P(x, sk["y"][1] - 0.5)], fill=(0, 0, 0), width=2)
    red = (214, 40, 40)
    s = R02["window"]
    dr.rectangle([P(s["x"], s["y"]), P(s["x"] + s["w"], s["y"] + s["h"])], outline=red, width=2)
    for n, c in R02["direction"]["centers"].items():
        d = 9.0 / 2
        dr.ellipse([P(c[0] - d, c[1] - d), P(c[0] + d, c[1] + d)], outline=red, width=2)
    c = R02["action"]["center"]
    dr.ellipse([P(c[0] - 11, c[1] - 11), P(c[0] + 11, c[1] + 11)], outline=red, width=2)
    for c in R02["function"]["centers"]:
        dr.ellipse([P(c[0] - 3.25, c[1] - 3.25), P(c[0] + 3.25, c[1] + 3.25)], outline=red, width=2)
    sp = R02["speaker"]
    dr.rectangle([P(sp["center"][0] - sp["w"] / 2, sp["center"][1] - sp["h"] / 2), P(sp["center"][0] + sp["w"] / 2, sp["center"][1] + sp["h"] / 2)],
                 outline=red, width=2)
    dr.text((ox, 4), "black: mockup through-holes   red: R0.2 working values (docs/MECHANICAL_CONCEPT.md)", fill=(0, 0, 0))
    im.save(path)


def render_joint(rv, r, path):
    """Wall section at a notch-free X, both shells in assembled position (mm, face at the top)."""
    f, rr = rv.parts["front"], rv.parts["rear"]
    x = rv.org[0] + rv.length * 0.5
    sf = slice_x(f, x)
    sr = slice_x(rr, x)
    T = r["stack"]["thickness_rear_rim_on_front_shoulder"]
    ry0 = float(bbox(rr)[0][1])
    sc = 40
    ylo, yhi = -1.0, 7.0
    W, H = int((yhi - ylo) * sc) + 60, int((T + 2) * sc) + 20
    im = Image.new("RGB", (W, H), (255, 255, 255))
    dr = ImageDraw.Draw(im)
    for k in range(0, int(T) + 2, 2):
        dr.line([(0, 10 + k * sc), (6, 10 + k * sc)], fill=(120, 120, 120))
        dr.text((8, 4 + k * sc), f"{k}", fill=(120, 120, 120))
    for s_, col, fn in ((sf, (20, 90, 200), lambda a: (a[0] - rv.org[1], a[1])),
                        (sr, (210, 60, 40), lambda a: (a[0] - ry0, T - a[1]))):
        for a, b in s_:
            pa, pb = fn(a), fn(b)
            if min(pa[0], pb[0]) > yhi + 0.2 or max(pa[0], pb[0]) < ylo - 0.2:
                continue
            dr.line([((pa[0] - ylo) * sc + 40, pa[1] * sc + 10), ((pb[0] - ylo) * sc + 40, pb[1] * sc + 10)], fill=col, width=2)
    dr.text((44, H - 14), f"blue: front shell, red: rear shell; thickness {T} mm (mm scale at left)", fill=(0, 0, 0))
    im.save(path)


def verify(r, baseline_path):
    base = json.loads(Path(baseline_path).read_text())
    if json.loads(json.dumps(r)) != base:
        for k in base:
            if base[k] != json.loads(json.dumps(r)).get(k):
                print(f"MISMATCH in '{k}'", file=sys.stderr)
        return 1
    print("measurements reproduce the baseline")
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("src")
    ap.add_argument("--json")
    ap.add_argument("--png-dir")
    ap.add_argument("--verify")
    a = ap.parse_args()
    rv = Review(load_parts(a.src))
    r = build(rv)
    if a.json:
        Path(a.json).write_text(json.dumps(r, indent=1, sort_keys=True) + "\n")
    if a.png_dir:
        Path(a.png_dir).mkdir(parents=True, exist_ok=True)
        render_front(rv, r, str(Path(a.png_dir) / "front_vs_r02.png"))
        render_joint(rv, r, str(Path(a.png_dir) / "joint_section.png"))
    print(f"{'part':11} {'tris':>6} {'size mm':>22} {'cm3':>7} open-edges")
    for p in r["parts"]:
        print(f"{p['part']:11} {p['triangles']:6d} {p['size_mm'][0]:7.2f}x{p['size_mm'][1]:6.2f}x{p['size_mm'][2]:5.2f} {p['volume_cm3']:7.2f} {p['open_edges']}")
    print(f"\nwindow lands in the upper half of the device: {r['window_in_upper_half']} (assumed orientation {'OK' if r['window_in_upper_half'] else 'CHECK'})")
    print("\n| item | mockup | R0.2 working value | delta |\n|---|---|---|---|")
    for row in compare(r):
        print("| " + " | ".join(row) + " |")
    print("\nstack:", json.dumps(r["stack"]))
    print("rim notches:", json.dumps(r["rim_notches"]))
    print("bosses:", json.dumps(r["bosses"]))
    print("usb cutout levels:", json.dumps(r["usb"]))
    print("lens seat:", json.dumps(r["lens_seat"]), "lens:", r["lens_mm"])
    print("caps:", json.dumps(r["caps"]))
    return verify(r, a.verify) if a.verify else 0


if __name__ == "__main__":
    sys.exit(main())
