"""Primitives in the specification frame of mechanical/params.json.

The frame is X to the right, Y downwards, Z into the thickness, origin at the outer top-left corner of the front face. CadQuery
is Y-up; the mapping (x, y, z) -> (x, -y, -z) is a half turn about the X axis, a proper rotation, so nothing is mirrored.
"""
import cadquery as cq


def box(x0, y0, z0, x1, y1, z1):
    """Axis-aligned box between two opposite corners given in the specification frame."""
    xmin, xmax = sorted((x0, x1))
    ymin, ymax = sorted((y0, y1))
    zmin, zmax = sorted((z0, z1))
    return cq.Workplane("XY").box(xmax - xmin, ymax - ymin, zmax - zmin, centered=False).translate((xmin, -ymax, -zmax))


def rounded_box(x0, y0, z0, x1, y1, z1, radius):
    """A box whose four edges along Z are rounded."""
    return box(x0, y0, z0, x1, y1, z1).edges("|Z").fillet(radius)


def cylinder(cx, cy, radius, z0, z1):
    """A cylinder along Z from z0 to z1 with its axis through (cx, cy)."""
    zmin, zmax = sorted((z0, z1))
    return cq.Workplane("XY").circle(radius).extrude(zmax - zmin).translate((cx, -cy, -zmax))


def point(x, y, z):
    """A point of the specification frame as a CadQuery vector."""
    return cq.Vector(x, -y, -z)
