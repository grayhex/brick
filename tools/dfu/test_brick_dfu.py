"""Logic tests for brick_dfu.py with a simulated device.

Run:  python -m unittest discover -s tools/dfu   (needs tools/dfu/requirements.txt)

The device here is a small state machine behind a fake SMP client that returns real
smpclient/smp response objects. It proves the tool's decisions (refuse unsigned files, upload to
slot 1, ask for a TEST swap, never confirm, judge the outcome after the reset). It proves nothing
about the SMP wire protocol, USB CDC ACM, MCUboot, or real hardware.
"""
import argparse
import contextlib
import hashlib
import io
import struct
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import brick_dfu as dfu  # noqa: E402
from smp import image_management as imgm  # noqa: E402
from smp import os_management as osm  # noqa: E402
from smpclient.requests.image_management import ImageStatesRead, ImageStatesWrite  # noqa: E402
from smpclient.requests.os_management import ResetWrite  # noqa: E402

dfu.SETTLE_S = 0.0

TLV_SHA256, TLV_ECDSA = 0x10, 0x22


def make_image(version=(0, 2, 0), signed=True, body=b"firmware" * 64):
    """A minimal MCUboot image: 32-byte header, body, TLV area (SHA256 [+ ECDSA signature])."""
    hdr = struct.pack("<LLHHLL", 0x96F3B83D, 0x20000, 32, 0, len(body), 0x100)
    hdr += struct.pack("<BBHL", *version, 0) + b"\0" * 4
    digest = hashlib.sha256(hdr + body).digest()
    tlvs = struct.pack("<BxH", TLV_SHA256, 32) + digest
    if signed:
        tlvs += struct.pack("<BxH", TLV_ECDSA, 72) + b"\x30" * 72
    return hdr + body + struct.pack("<HH", 0x6907, 4 + len(tlvs)) + tlvs, digest


class Slot:
    def __init__(self, version, digest, active=False, confirmed=False, pending=False):
        self.version, self.hash = version, digest
        self.active, self.confirmed, self.pending = active, confirmed, pending


class Device:
    """Two slots; reset applies a pending swap the way MCUboot would (test, then app decides)."""

    def __init__(self, self_test_ok=True, bootloader_accepts=True):
        _, d = make_image((0, 1, 0))
        self.slot = {0: Slot("0.1.0", d, active=True, confirmed=True), 1: None}
        self.self_test_ok, self.bootloader_accepts = self_test_ok, bootloader_accepts
        self.requests = []

    def images(self):
        return [imgm.ImageState(slot=n, version=s.version, image=0, hash=s.hash, active=s.active,
                                confirmed=s.confirmed, pending=s.pending, bootable=True)
                for n, s in self.slot.items() if s]

    def reset(self):
        s1 = self.slot[1]
        if s1 and s1.pending and self.bootloader_accepts:
            self.slot[0], self.slot[1] = s1, self.slot[0]
            s1.pending, s1.active = False, True
            self.slot[1].active = False
            self.slot[1].confirmed = False
            s1.confirmed = self.self_test_ok  # the application confirms itself after its self-test
            if not self.self_test_ok:
                s1.confirmed = False
        elif s1:
            s1.pending = False


class FakeClient:
    def __init__(self, dev):
        self.dev = dev

    async def __aenter__(self):
        return self

    async def __aexit__(self, *exc):
        return False

    async def request(self, req, timeout_s=None):
        self.dev.requests.append(req)
        if isinstance(req, ImageStatesRead):
            return imgm.ImageStatesReadResponse(sequence=0, images=self.dev.images())
        if isinstance(req, ImageStatesWrite):
            assert not req.confirm, "the tool must never confirm an image"
            for s in self.dev.slot.values():
                if s and s.hash == req.hash:
                    s.pending = True
            return imgm.ImageStatesWriteResponse(sequence=0, images=self.dev.images())
        if isinstance(req, ResetWrite):
            self.dev.reset()
            return osm.ResetWriteResponse(sequence=0)
        raise AssertionError(f"unexpected request {req!r}")

    async def upload(self, image, slot=0, upgrade=False, **_):
        assert upgrade is False
        tmp = Path(tempfile.mkstemp()[1])
        tmp.write_bytes(image)
        info = dfu.ImageInfo.load_file(str(tmp))
        tmp.unlink()
        self.dev.slot[slot] = Slot(dfu.image_version(info), dfu.image_hash(info))
        for off in range(0, len(image), 64):
            yield min(off + 64, len(image))


class Base(unittest.TestCase):
    def setUp(self):
        self.dev = Device()
        self._saved = (dfu.make_client, dfu.pick_port, dfu.wait_for_port)
        dfu.make_client = lambda port, lb: FakeClient(self.dev)
        self.roles = []
        dfu.pick_port = lambda requested, want=None: self.roles.append(want) or "FAKE"
        dfu.wait_for_port = lambda requested, timeout, want=None: "FAKE"
        self.tmp = tempfile.TemporaryDirectory()

    def tearDown(self):
        dfu.make_client, dfu.pick_port, dfu.wait_for_port = self._saved
        self.tmp.cleanup()

    def write(self, name, data):
        p = Path(self.tmp.name) / name
        p.write_bytes(data)
        return str(p)

    def run_cli(self, *argv):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            rc = dfu.main(["--quiet", *argv])
        return rc, out.getvalue(), err.getvalue()

    def confirm_requests(self):
        return [r for r in self.dev.requests if isinstance(r, ImageStatesWrite) and r.confirm]


class FileChecks(Base):
    def test_refuses_unsigned_image(self):
        f = self.write("u.bin", make_image(signed=False)[0])
        rc, _, err = self.run_cli("update", f)
        self.assertEqual(rc, 1)
        self.assertIn("not signed", err)
        self.assertEqual(self.dev.requests, [])

    def test_refuses_random_file(self):
        f = self.write("junk.bin", bytes(range(256)) * 16)
        rc, _, err = self.run_cli("upload", f)
        self.assertEqual(rc, 1)
        self.assertIn("not an MCUboot image", err)

    def test_version_formatting_matches_the_device(self):
        info = self._info(make_image((1, 2, 3))[0])
        self.assertEqual(dfu.image_version(info), "1.2.3")

    def _info(self, data):
        return dfu.ImageInfo.load_file(self.write("v.bin", data))


class Update(Base):
    def test_good_update_ends_confirmed_and_never_confirms_itself(self):
        f = self.write("new.bin", make_image()[0])
        rc, out, _ = self.run_cli("update", f)
        self.assertEqual(rc, 0, out)
        self.assertIn("new image runs and is confirmed", out)
        self.assertEqual(self.confirm_requests(), [])
        self.assertTrue(self.dev.slot[0].active and self.dev.slot[0].version == "0.2.0")

    def test_image_that_fails_its_self_test_is_reported_unconfirmed(self):
        self.dev.self_test_ok = False
        rc, _, err = self.run_cli("update", self.write("new.bin", make_image()[0]))
        self.assertEqual(rc, 3)
        self.assertIn("NOT confirmed", err)

    def test_bootloader_rejection_is_reported(self):
        self.dev.bootloader_accepts = False
        rc, _, err = self.run_cli("update", self.write("new.bin", make_image()[0]))
        self.assertEqual(rc, 2)
        self.assertIn("NOT running", err)
        self.assertEqual(self.dev.slot[0].version, "0.1.0")

    def test_already_running_image_is_not_uploaded_again(self):
        data, d = make_image((0, 1, 0))
        self.dev.slot[0].hash = d
        rc, out, _ = self.run_cli("update", self.write("same.bin", data))
        self.assertEqual(rc, 0)
        self.assertIn("already running", out)
        self.assertIsNone(self.dev.slot[1])

    def test_upload_goes_to_slot_1_only(self):
        rc, _, _ = self.run_cli("upload", self.write("new.bin", make_image()[0]))
        self.assertEqual(rc, 0)
        self.assertEqual(self.dev.slot[1].version, "0.2.0")
        self.assertFalse(self.dev.slot[1].pending)
        self.assertEqual(self.dev.slot[0].version, "0.1.0")


class TestAndRecover(Base):
    def test_test_without_uploaded_image_fails_cleanly(self):
        rc, _, err = self.run_cli("test")
        self.assertEqual(rc, 1)
        self.assertIn("no unique image in slot 1", err)

    def test_test_marks_slot_1_pending(self):
        self.run_cli("upload", self.write("new.bin", make_image()[0]))
        rc, out, _ = self.run_cli("test")
        self.assertEqual(rc, 0, out)
        self.assertTrue(self.dev.slot[1].pending)

    def test_each_command_looks_for_the_right_kind_of_port(self):
        f = self.write("new.bin", make_image()[0])
        self.run_cli("recover", f)
        self.assertEqual(self.roles, ["recovery"])
        self.roles.clear()
        for argv in (("update", f), ("upload", f), ("test",)):
            self.dev = Device()
            self.roles.clear()
            self.run_cli(*argv)
            self.assertEqual(self.roles[0], "app", argv)

    def test_recover_writes_slot_0(self):
        rc, out, _ = self.run_cli("recover", self.write("new.bin", make_image((0, 3, 0))[0]))
        self.assertEqual(rc, 0, out)
        self.assertEqual(self.dev.slot[0].version, "0.3.0")
        self.assertEqual(self.confirm_requests(), [])


class Ports(unittest.TestCase):
    def setUp(self):
        self._saved = dfu.find_ports

    def tearDown(self):
        dfu.find_ports = self._saved

    def test_no_port(self):
        dfu.find_ports = lambda: []
        with self.assertRaises(dfu.Fail):
            dfu.pick_port("auto")

    def test_single_port_and_role_filter(self):
        dfu.find_ports = lambda: [("/dev/ttyACM0", "app"), ("/dev/ttyACM1", "recovery")]
        self.assertEqual(dfu.pick_port("auto", "recovery"), "/dev/ttyACM1")
        self.assertEqual(dfu.pick_port("auto", "app"), "/dev/ttyACM0")

    def test_ambiguous_ports_need_a_choice(self):
        dfu.find_ports = lambda: [("A", "app"), ("B", "app")]
        with self.assertRaises(dfu.Fail):
            dfu.pick_port("auto", "app")
        self.assertEqual(dfu.pick_port("COM7"), "COM7")


if __name__ == "__main__":
    unittest.main()
