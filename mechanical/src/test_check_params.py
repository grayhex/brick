"""check_params.py must be able to fail: each test breaks params.json on purpose and expects the matching check to go red.

Run: python3 mechanical/src/test_check_params.py (standard library only)
"""
import copy
import sys
import unittest

import check_params
from params import load


def failed(params):
    return [name for name, ok, _ in check_params.run(params) if not ok]


class ParamsChecksCanFail(unittest.TestCase):
    def setUp(self):
        self.params = load()

    def test_shipped_values_pass(self):
        self.assertEqual(failed(self.params), [])

    def test_keys_too_close(self):
        p = copy.deepcopy(self.params)
        p["direction_keys"]["centers"]["LEFT"] = [17.0, 130.5]  # almost touches UP and DOWN
        self.assertTrue(any("open gap between direction keys" in n for n in failed(p)))

    def test_action_key_collides_with_the_right_key(self):
        p = copy.deepcopy(self.params)
        p["action_key"]["center"] = [45.0, 130.5]
        self.assertTrue(any("action key and the direction keys" in n for n in failed(p)))

    def test_function_key_not_under_the_window(self):
        p = copy.deepcopy(self.params)
        p["function_keys"]["centers"]["MODE"] = [25, 80]
        self.assertTrue(any("MODE is under the LCD window" in n for n in failed(p)))

    def test_window_leaves_no_wall(self):
        p = copy.deepcopy(self.params)
        p["lcd_opening"]["x"] = 0.5
        self.assertTrue(any("LCD window lies inside the body" in n for n in failed(p)))

    def test_speaker_hole_on_the_rear(self):
        p = copy.deepcopy(self.params)
        p["speaker_grille"]["rear_or_bottom_holes"] = 1
        self.assertTrue(any("no speaker holes on the rear or the bottom" in n for n in failed(p)))

    def test_second_opening_on_the_bottom(self):
        p = copy.deepcopy(self.params)
        p["usb_c"]["openings_on_bottom_edge"] = 2
        self.assertTrue(any("USB-C is centred" in n for n in failed(p)))

    def test_wrong_envelope(self):
        p = copy.deepcopy(self.params)
        p["body"]["height"] = 170
        self.assertTrue(any("body envelope" in n for n in failed(p)))


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]], verbosity=2)
