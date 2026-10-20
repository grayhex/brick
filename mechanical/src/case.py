"""Front and rear shell of the shared R0.2 case (one geometry for MONOLITH, CLEAR_SMOKE and CERAMIC).

Not modelled on purpose (undecided, see params.json "not_decided"): fasteners and bosses, battery and PCB space, side
controls, the LCD/FPC exit. The shells are a hollow mock-up for ergonomics, not a moulding.
"""
from geom import box, cylinder, rounded_box
from params import seam_z


def _usb_cut(params, t):
    usb = params["usb_c"]
    wall = params["body"]["wall_prototype"]
    w, h = params["body"]["width"], params["body"]["height"]
    cw, ch = usb["cutout_placeholder"]
    cx = usb["center_x"]
    return box(cx - cw / 2.0, h - wall - 1.0, t / 2.0 - ch / 2.0, cx + cw / 2.0, h + 1.0, t / 2.0 + ch / 2.0)


def front_shell(params, t):
    body = params["body"]
    w, h, wall, r = body["width"], body["height"], body["wall_prototype"], body["corner_radius"]
    seam = seam_z(params, t)
    asm = params["prototype_assumptions"]
    shell = rounded_box(0, 0, 0, w, h, seam, r).cut(rounded_box(wall, wall, wall, w - wall, h - wall, seam + 1.0, r - wall))

    lcd = params["lcd_opening"]
    shell = shell.cut(box(lcd["x"], lcd["y"], -1.0, lcd["x"] + lcd["width"], lcd["y"] + lcd["height"], wall + 0.5))

    hole = asm["hole_clearance"]
    for cx, cy, d in key_positions(params):
        shell = shell.cut(cylinder(cx, cy, d / 2.0 + hole, -1.0, wall + 0.5))

    sp = params["speaker_grille"]
    pitch, hole_d = asm["grille_pitch"], asm["grille_hole_diameter"]
    ex, ey = sp["envelope"]
    cols = int((ex - hole_d) // pitch) + 1
    rows = int((ey - hole_d) // pitch) + 1
    x_start = sp["center"][0] - (cols - 1) * pitch / 2.0
    y_start = sp["center"][1] - (rows - 1) * pitch / 2.0
    for i in range(cols):
        for j in range(rows):
            shell = shell.cut(cylinder(x_start + i * pitch, y_start + j * pitch, hole_d / 2.0, -1.0, wall + 0.5))

    return shell.cut(_usb_cut(params, t))


def rear_shell(params, t):
    body = params["body"]
    w, h, wall, r = body["width"], body["height"], body["wall_prototype"], body["corner_radius"]
    seam = seam_z(params, t)
    shell = rounded_box(0, 0, seam, w, h, t, r).cut(rounded_box(wall, wall, seam - 1.0, w - wall, h - wall, t - wall, r - wall))
    return shell.cut(_usb_cut(params, t))


def key_positions(params):
    """(x, y, diameter) of the eight keys in a fixed order: three function, four direction, one action."""
    out = []
    fk = params["function_keys"]
    for name in ("MODE", "SELECT", "START"):
        out.append((*fk["centers"][name], fk["diameter"]))
    dk = params["direction_keys"]
    for name in ("UP", "LEFT", "RIGHT", "DOWN"):
        out.append((*dk["centers"][name], dk["diameter"]))
    ak = params["action_key"]
    out.append((*ak["center"], ak["diameter"]))
    return out
