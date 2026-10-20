#!/usr/bin/env python3
"""brick-dfu: update or recover a Brick over its USB-C cable (MCUmgr / SMP over USB CDC ACM).

    brick_dfu.py ports                  list serial ports that look like a Brick
    brick_dfu.py status                 bootloader, images, which one runs and is confirmed
    brick_dfu.py update  signed.bin     upload to slot 1, ask MCUboot to try it, reset, report
    brick_dfu.py upload  signed.bin     upload only (no reset)
    brick_dfu.py test    [HASH]         mark the uploaded image (or HASH) to be tried on next reset
    brick_dfu.py reset                  reboot the device
    brick_dfu.py recover signed.bin     device held in MCUboot serial recovery: write slot 0, reset

STATUS: written against the SMP protocol and checked only against Zephyr's MCUmgr server
running on native_sim (platform/nrf52840/tests/native_sim_smp.sh). It has NOT been run
against a real Brick or nRF52840 DK yet (docs/NRF52840_BRINGUP.md).

The tool never confirms an image: confirmation belongs to the new application after its
self-test (docs/FIRMWARE_UPDATE_NRF52840.md). Only signed files produced by the build
(zephyr.signed.bin) are accepted; MCUboot verifies the signature again before it boots.
"""
from __future__ import annotations

import argparse
import asyncio
import sys
import time
from pathlib import Path

from smpclient import SMPClient
from smpclient.generics import error, success
from smpclient.mcuboot import IMAGE_TLV, ImageInfo
from smpclient.requests.image_management import ImageStatesRead, ImageStatesWrite
from smpclient.requests.os_management import BootloaderInformationRead, ResetWrite
from smpclient.transport.serial import Auto, BufferParams, SMPSerialTransport
from serial.tools import list_ports

# DEV identity (Zephyr project VID). Replace by the production VID/PID when it exists.
DEV_VIDS = {0x2FE3, 0x1915}
SETTLE_S = 1.0  # pause around a reset so the old USB device is really gone
NAME_HINT = "brick"
RECOVERY_HINT = "recovery"


class Fail(Exception):
    """A problem the user can act on; printed without a traceback."""


# --------------------------------------------------------------------------- ports


def find_ports() -> list[tuple[str, str]]:
    """(device, 'app'|'recovery') for every port that looks like a Brick."""
    found = []
    for p in list_ports.comports():
        product = (p.product or "").lower()
        if NAME_HINT in product:
            found.append((p.device, "recovery" if RECOVERY_HINT in product else "app"))
    return found


def pick_port(requested: str | None, want: str | None = None) -> str:
    if requested and requested != "auto":
        return requested
    ports = [d for d, mode in find_ports() if want in (None, mode)]
    if len(ports) == 1:
        return ports[0]
    if not ports:
        what = {"app": "application", "recovery": "recovery"}.get(want or "", "")
        raise Fail(f"no Brick {what} serial port found; connect the USB-C cable "
                   f"(try `ports`, or pass --port)".replace("  ", " "))
    raise Fail("several Brick ports found, pick one with --port: " + ", ".join(ports))


def wait_for_port(requested: str | None, timeout: float, want: str | None = None) -> str:
    """After a reset the device re-enumerates; its name may change (COM numbers)."""
    deadline = time.monotonic() + timeout
    last: Fail | None = None
    while time.monotonic() < deadline:
        try:
            return pick_port(requested, want)
        except Fail as e:
            last = e
            time.sleep(0.5)
    raise Fail(f"device did not come back within {timeout:.0f} s ({last})")


# --------------------------------------------------------------------------- image file


def read_image(path: Path):
    """Return (bytes, ImageInfo). Refuses anything that is not an MCUboot-signed image."""
    data = path.read_bytes()
    try:
        info = ImageInfo.load_file(str(path))
    except Exception as e:  # smpclient raises several types for malformed files
        raise Fail(f"{path}: not an MCUboot image ({e}); use zephyr.signed.bin") from e
    kinds = {t.header.type for t in info.tlvs}
    if IMAGE_TLV.SHA256 not in kinds:
        raise Fail(f"{path}: no SHA256 TLV")
    if not ({IMAGE_TLV.ECDSA_SIG, IMAGE_TLV.RSA2048_PSS, IMAGE_TLV.RSA3072_PSS,
             IMAGE_TLV.ED25519} & kinds):
        raise Fail(f"{path}: image is not signed; refusing (use zephyr.signed.bin from the build)")
    return data, info


def image_hash(info) -> bytes:
    return bytes(info.get_tlv(IMAGE_TLV.SHA256).value)


def image_version(info) -> str:
    v = info.header.ver  # shown like the device does: major.minor.revision[+build]
    return f"{v.major}.{v.minor}.{v.revision}" + (f"+{v.build_num}" if v.build_num else "")


# --------------------------------------------------------------------------- SMP helpers


def make_client(port: str, line_buffers: int | None) -> SMPClient:
    strategy = BufferParams(line_length=128, line_buffers=line_buffers) if line_buffers else Auto()
    return SMPClient(SMPSerialTransport(strategy), port, timeout_s=5.0)


def ok(resp, what: str):
    if success(resp):
        return resp
    if error(resp):
        raise Fail(f"{what}: device returned an error: {resp}")
    raise Fail(f"{what}: unexpected response {resp!r}")


async def read_images(c: SMPClient):
    return ok(await c.request(ImageStatesRead()), "image state").images


def flags(i) -> str:
    names = [n for n in ("active", "confirmed", "pending", "permanent", "bootable") if getattr(i, n)]
    return ",".join(names) or "-"


def describe(images) -> str:
    rows = []
    for i in sorted(images, key=lambda x: ((x.image or 0), x.slot)):
        h = bytes(i.hash).hex()[:16] if i.hash else "(no hash)"
        rows.append(f"  image {i.image or 0} slot {i.slot}  version {i.version:<12} hash {h}  flags {flags(i)}")
    return "\n".join(rows) or "  (no images)"


async def bootloader_line(c: SMPClient) -> str:
    try:
        r = await c.request(BootloaderInformationRead(query="mode"))
    except Exception:
        return "bootloader: (not reported)"
    if success(r):
        mode = getattr(getattr(r, "response", None), "mode", None)
        return f"bootloader: {r.bootloader}" + (f", mode {getattr(mode, 'name', mode)}" if mode is not None else "")
    return "bootloader: (not reported)"


# --------------------------------------------------------------------------- commands


async def cmd_status(a) -> int:
    port = pick_port(a.port)
    async with make_client(port, a.line_buffers) as c:
        print(f"port {port}")
        print(await bootloader_line(c))
        print(describe(await read_images(c)))
    return 0


async def do_upload(c: SMPClient, data: bytes, slot: int, quiet: bool) -> None:
    last = -1
    started = time.monotonic()
    async for offset in c.upload(data, slot=slot, upgrade=False):
        pct = offset * 100 // len(data)
        if not quiet and pct != last and (pct % 5 == 0 or pct == 100):
            last = pct
            print(f"\r  uploading {pct:3d}%  {offset}/{len(data)} B", end="", flush=True)
    if not quiet:
        print(f"\r  uploaded {len(data)} B in {time.monotonic() - started:.1f} s            ")


async def cmd_upload(a) -> int:
    data, info = read_image(Path(a.file))
    port = pick_port(a.port, "app")
    print(f"image {a.file}: version {image_version(info)}, {len(data)} B, hash {image_hash(info).hex()[:16]}")
    async with make_client(port, a.line_buffers) as c:
        await do_upload(c, data, 1, a.quiet)
        print(describe(await read_images(c)))
    return 0


async def mark_for_test(c: SMPClient, h: bytes | None) -> bytes:
    images = await read_images(c)
    if h is None:
        pending = [i for i in images if i.slot == 1 and i.hash]
        if len(pending) != 1:
            raise Fail("no unique image in slot 1; give the hash, or run `upload` first")
        h = bytes(pending[0].hash)
    elif not any(i.hash and bytes(i.hash) == h for i in images):
        raise Fail(f"no image with hash {h.hex()} on the device")
    resp = ok(await c.request(ImageStatesWrite(hash=h, confirm=False)), "test request")
    del resp
    return h


async def cmd_test(a) -> int:
    port = pick_port(a.port, "app")
    h = bytes.fromhex(a.hash) if a.hash else None
    async with make_client(port, a.line_buffers) as c:
        h = await mark_for_test(c, h)
        print(f"image {h.hex()[:16]} will be tried at the next reset")
        print(describe(await read_images(c)))
    return 0


async def cmd_reset(a) -> int:
    port = pick_port(a.port)
    async with make_client(port, a.line_buffers) as c:
        await c.request(ResetWrite())
    print("reset requested")
    return 0


async def cmd_update(a) -> int:
    data, info = read_image(Path(a.file))
    want_hash = image_hash(info)
    port = pick_port(a.port, "app")
    print(f"image {a.file}: version {image_version(info)}, {len(data)} B, hash {want_hash.hex()[:16]}")
    async with make_client(port, a.line_buffers) as c:
        before = await read_images(c)
        print("before:\n" + describe(before))
        if any(i.hash and bytes(i.hash) == want_hash and i.active for i in before):
            print("this image is already running; nothing to do")
            return 0
        await do_upload(c, data, 1, a.quiet)
        await mark_for_test(c, want_hash)
        print("marked for test; resetting (MCUboot swaps the slots, this takes a while)")
        try:
            await c.request(ResetWrite(), timeout_s=2.0)
        except Exception:
            pass  # the device may drop the link before it answers

    await asyncio.sleep(SETTLE_S)
    port = wait_for_port(a.port, a.wait, "app")
    await asyncio.sleep(SETTLE_S)
    async with make_client(port, a.line_buffers) as c:
        after = await read_images(c)
    print("after:\n" + describe(after))
    running = [i for i in after if i.active]
    if not running or not running[0].hash or bytes(running[0].hash) != want_hash:
        print("RESULT: the new image is NOT running (MCUboot rejected or reverted it)", file=sys.stderr)
        return 2
    if not running[0].confirmed:
        print("RESULT: new image runs but is NOT confirmed; it reverts at the next reset", file=sys.stderr)
        return 3
    print("RESULT: new image runs and is confirmed")
    return 0


async def cmd_recover(a) -> int:
    data, info = read_image(Path(a.file))
    port = pick_port(a.port, "recovery")
    print(f"recovery: writing {a.file} (version {image_version(info)}, {len(data)} B) into slot 0 of {port}")
    async with make_client(port, a.line_buffers) as c:
        await do_upload(c, data, 0, a.quiet)
        print(describe(await read_images(c)))
        try:
            await c.request(ResetWrite(), timeout_s=2.0)
        except Exception:
            pass
    print("reset requested; MCUboot verifies the signature before it starts the image")
    return 0


def cmd_ports(_a) -> int:
    ports = find_ports()
    for dev, mode in ports:
        print(f"{dev}\t{mode}")
    if not ports:
        print("no Brick serial port found", file=sys.stderr)
    return 0 if ports else 1


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(prog="brick-dfu", description=__doc__.split("\n\n")[0])
    ap.add_argument("--port", default="auto", help="serial port (default: auto-detect a Brick)")
    ap.add_argument("--line-buffers", type=int, default=None,
                    help="tune the SMP serial fragmentation (default: ask the device)")
    ap.add_argument("--wait", type=float, default=90.0, help="seconds to wait for the device after reset")
    ap.add_argument("--quiet", action="store_true", help="no progress output")
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("ports")
    sub.add_parser("status")
    sub.add_parser("reset")
    for name in ("update", "upload", "recover"):
        sub.add_parser(name).add_argument("file", help="zephyr.signed.bin from the build")
    sub.add_parser("test").add_argument("hash", nargs="?", help="image hash (hex); default: image in slot 1")
    a = ap.parse_args(argv)
    try:
        if a.cmd == "ports":
            return cmd_ports(a)
        return asyncio.run({"status": cmd_status, "update": cmd_update, "upload": cmd_upload,
                            "test": cmd_test, "reset": cmd_reset, "recover": cmd_recover}[a.cmd](a))
    except Fail as e:
        print(f"error: {e}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        return 130


if __name__ == "__main__":
    sys.exit(main())
