"""The validation must be able to fail: each test breaks the model on purpose and expects the matching check to go red.

Run: python3 mechanical/src/test_validation.py (needs CadQuery, see mechanical/requirements.txt)
"""
import copy
import sys
import unittest

import build
from case import key_positions
from geom import cylinder
from keys import NAMES
from params import load, thickness


def run(params, parts, t):
    rep = build.Report()
    import io
    import contextlib

    with contextlib.redirect_stdout(io.StringIO()):
        build.validate(params, t, parts, rep)
    return {name: ok for name, ok, _ in rep.rows}


def failed(results):
    return [name for name, ok in results.items() if not ok]


class ValidationCanFail(unittest.TestCase):
    def setUp(self):
        self.params = load()
        self.t = thickness(self.params)
        self.parts = build.build_parts(self.params, self.t)

    def test_clean_model_passes(self):
        self.assertEqual(failed(run(self.params, self.parts, self.t)), [])

    def test_hole_in_the_rear_wall_is_caught(self):
        w = self.params["body"]["width"]
        self.parts["rear_shell"] = self.parts["rear_shell"].cut(cylinder(w / 2.0, 40.0, 3.0, self.t - 3.0, self.t + 1.0))
        self.assertTrue(any("rear plate is closed" in n for n in failed(run(self.params, self.parts, self.t))))

    def test_extra_bottom_opening_is_caught(self):
        h, wall = self.params["body"]["height"], self.params["body"]["wall_prototype"]
        self.parts["front_shell"] = self.parts["front_shell"].cut(_bottom_hole(20.0, h, wall, self.t))
        self.assertTrue(any("side, top and bottom walls have no opening" in n for n in failed(run(self.params, self.parts, self.t))))

    def test_side_hole_is_caught(self):
        self.parts["front_shell"] = self.parts["front_shell"].cut(_side_hole(self.params, self.t))
        self.assertTrue(any("side, top and bottom walls have no opening" in n for n in failed(run(self.params, self.parts, self.t))))

    def test_extra_hole_in_the_front_plate_is_caught(self):
        self.parts["front_shell"] = self.parts["front_shell"].cut(cylinder(60.0, 160.0, 2.0, -1.0, 1.0))
        self.assertTrue(any("front plate has only" in n for n in failed(run(self.params, self.parts, self.t))))

    def test_oversized_key_overlaps_the_shell(self):
        cx, cy, d = key_positions(self.params)[3]
        self.parts[NAMES[3]] = cylinder(cx, cy, d / 2.0 + 1.0, -1.5, 3.0)
        self.assertTrue(any("no two parts intersect" in n for n in failed(run(self.params, self.parts, self.t))))

    def test_missing_key_hole_is_caught(self):
        cx, cy, d = key_positions(self.params)[7]
        self.parts["front_shell"] = self.parts["front_shell"].union(cylinder(cx, cy, d / 2.0 + 2.0, 0.0, 1.0))
        self.assertTrue(any("action_key sits in its hole" in n for n in failed(run(self.params, self.parts, self.t))))

    def test_wrong_envelope_is_caught(self):
        params = copy.deepcopy(self.params)
        parts = build.build_parts(params, self.t)
        parts["rear_shell"] = parts["rear_shell"].union(cylinder(40.0, 80.0, 3.0, self.t, self.t + 2.0))
        self.assertTrue(any("envelope is" in n for n in failed(run(params, parts, self.t))))


def _bottom_hole(x, h, wall, t):
    from geom import box

    return box(x - 1.5, h - wall - 0.5, 0.5, x + 1.5, h + 1.0, t / 4.0 + 1.0)


def _side_hole(params, t):
    from geom import box

    wall = params["body"]["wall_prototype"]
    return box(-1.0, 60.0, 0.5, wall + 0.5, 70.0, t / 4.0 + 1.0)


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]], verbosity=2)
