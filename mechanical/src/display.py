"""Placeholder of the LCD stack behind the window. The panel is a CANDIDATE (Sharp LS032B7DD02 + Azumo frontlight): this
solid is only its outline and the stack thickness to be checked, not the real component, FPC or frontlight."""
from geom import box


def display_placeholder(params):
    wall = params["body"]["wall_prototype"]
    lcd = params["lcd_opening"]
    cand = params["lcd_candidate"]
    cx = lcd["x"] + lcd["width"] / 2.0
    cy = lcd["y"] + lcd["height"] / 2.0
    ow, oh = cand["outline"]
    stack = params["prototype_assumptions"]["lcd_stack_thickness_placeholder"]
    return box(cx - ow / 2.0, cy - oh / 2.0, wall, cx + ow / 2.0, cy + oh / 2.0, wall + stack)
