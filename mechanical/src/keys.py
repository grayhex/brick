"""The eight key caps as separate solids: three function keys, four direction keys, one action key."""
from case import key_positions
from geom import cylinder

NAMES = ("function_key_mode", "function_key_select", "function_key_start", "direction_key_up", "direction_key_left",
         "direction_key_right", "direction_key_down", "action_key")


def key_caps(params):
    """name -> solid. A cap passes through its hole with the clearance left around it and has a flange behind the front wall."""
    wall = params["body"]["wall_prototype"]
    asm = params["prototype_assumptions"]
    caps = {}
    for name, (cx, cy, d) in zip(NAMES, key_positions(params)):
        post = cylinder(cx, cy, d / 2.0, -asm["key_protrusion"], wall + asm["key_flange_thickness"])
        flange = cylinder(cx, cy, d / 2.0 + asm["key_flange_extra_radius"], wall, wall + asm["key_flange_thickness"])
        caps[name] = post.union(flange)
    return caps
