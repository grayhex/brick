#!/usr/bin/env python3
"""Check a finished sysbuild directory against the Brick nRF52840 platform contract.

    verify_build.py <build-dir> --ncs <ncs-workspace> [--key <dev key.pem>] [--md report.md]

This verifies what a BUILD can prove: the resolved partition map is identical in
MCUboot and in the application, the Kconfig facts (swap mode, signature type, serial
recovery over CDC ACM, boot button, external flash), the signed image (signature checked
with the key, size headroom), and key hygiene (the bootloader does not carry the public
Nordic sample key). It does NOT prove anything about real hardware: flashing, USB
enumeration, swap, revert and power loss are tested on the DK (docs/NRF52840_BRINGUP.md).
"""
import argparse
import hashlib
import re
import subprocess
import sys
from pathlib import Path

APP = "nrf52840"  # sysbuild image name = application directory name
ERASE = 4096      # nRF52840 internal page and MX25R64 sector used for the slots
LABELS = ("boot_partition", "slot0_partition", "slot1_partition", "storage_partition")

failures = []
rows = []


def check(ok, what, detail=""):
    rows.append((ok, what, detail))
    if not ok:
        failures.append(what)


def partitions(dts_text):
    """label -> (offset, size) from a zephyr.dts, with the parent device name."""
    found = {}
    for m in re.finditer(r"(\w+_partition): partition@([0-9a-f]+) \{(.*?)\n\s*\};", dts_text, re.S):
        label, off, body = m.group(1), int(m.group(2), 16), m.group(3)
        reg = re.search(r"reg = < (0x[0-9a-f]+) (0x[0-9a-f]+) >", body)
        if reg:
            found[label] = (int(reg.group(1), 16), int(reg.group(2), 16))
    return found


def kconfig(path):
    cfg = {}
    for line in Path(path).read_text().splitlines():
        m = re.match(r"(CONFIG_\w+)=(.*)", line)
        if m:
            cfg[m.group(1)] = m.group(2).strip('"')
    return cfg


def run(*cmd):
    return subprocess.run(cmd, check=True, capture_output=True, text=True).stdout


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("build")
    ap.add_argument("--ncs", required=True)
    ap.add_argument("--key")
    ap.add_argument("--md")
    a = ap.parse_args()
    b, ncs = Path(a.build), Path(a.ncs)
    imgtool = [sys.executable, str(ncs / "bootloader/mcuboot/scripts/imgtool.py")]

    # ---- partition map: identical in both images, sane, equal slots --------------------
    app_p = partitions((b / APP / "zephyr/zephyr.dts").read_text())
    boot_p = partitions((b / "mcuboot/zephyr/zephyr.dts").read_text())
    for l in LABELS:
        check(l in app_p and l in boot_p, f"partition {l} present in app and MCUboot")
    check(all(app_p.get(l) == boot_p.get(l) for l in LABELS), "partition map identical in app and MCUboot",
          ", ".join(f"{l}={app_p.get(l)}" for l in LABELS))
    if all(l in app_p for l in LABELS):
        s0, s1 = app_p["slot0_partition"], app_p["slot1_partition"]
        check(s0[1] == s1[1], "slot0 and slot1 have equal size", f"{s0[1]} / {s1[1]} bytes")
        check(s0[1] % ERASE == 0 and s1[1] % ERASE == 0, "slot sizes are a multiple of the 4 KiB erase size")
        internal = sorted((v, k) for k, v in app_p.items() if k != "slot1_partition")
        end = 0
        ok = True
        for (off, size), name in internal:
            ok &= off >= end
            end = off + size
        check(ok and end <= 0x100000, "internal partitions do not overlap and fit 1 MiB", hex(end))
        check(s1[0] + s1[1] <= 8 * 1024 * 1024, "slot1 fits the 8 MiB QSPI NOR", hex(s1[0] + s1[1]))
        sectors = s0[1] // ERASE
    else:
        sectors = 0

    # ---- MCUboot configuration -------------------------------------------------------
    mc = kconfig(b / "mcuboot/zephyr/.config")
    for sym, want in (("CONFIG_BOOT_SWAP_USING_MOVE", "y"), ("CONFIG_BOOT_SIGNATURE_TYPE_ECDSA_P256", "y"),
                      ("CONFIG_MCUBOOT_SERIAL", "y"), ("CONFIG_BOOT_SERIAL_CDC_ACM", "y"),
                      ("CONFIG_BOOT_SERIAL_ENTRANCE_GPIO", "y"), ("CONFIG_BOOT_SERIAL_NO_APPLICATION", "y"),
                      ("CONFIG_NORDIC_QSPI_NOR", "y"), ("CONFIG_BOOT_VALIDATE_SLOT0", "y"),
                      # the application starts the watchdog; it survives soft resets, so MCUboot must feed it
                      ("CONFIG_BOOT_WATCHDOG_FEED", "y"), ("CONFIG_BOOT_WATCHDOG_FEED_NRFX_WDT", "y")):
        check(mc.get(sym) == want, f"MCUboot {sym}={want}", f"is {mc.get(sym)}")
    for sym in ("CONFIG_BOOT_UPGRADE_ONLY", "CONFIG_BOOT_SERIAL_UART", "CONFIG_BOOT_DIRECT_XIP"):
        check(mc.get(sym) in (None, "n"), f"MCUboot {sym} is off", f"is {mc.get(sym)}")
    max_sec = int(mc.get("CONFIG_BOOT_MAX_IMG_SECTORS", "0"))
    check(max_sec >= sectors > 0, "BOOT_MAX_IMG_SECTORS covers a whole slot", f"{max_sec} >= {sectors}")
    mdts = (b / "mcuboot/zephyr/zephyr.dts").read_text()
    check(mdts.count('compatible = "zephyr,cdc-acm-uart"') == 1, "MCUboot devicetree has one CDC ACM UART node")
    check("mcuboot-button0 = &button0" in mdts, "boot button alias mcuboot-button0 = button0 (DK SW1)")

    # ---- application configuration ---------------------------------------------------
    ac = kconfig(b / APP / "zephyr/.config")
    for sym in ("CONFIG_BOOTLOADER_MCUBOOT", "CONFIG_MCUMGR_GRP_IMG", "CONFIG_MCUMGR_GRP_OS",
                "CONFIG_MCUMGR_TRANSPORT_UART", "CONFIG_NORDIC_QSPI_NOR", "CONFIG_WATCHDOG"):
        check(ac.get(sym) == "y", f"app {sym}=y", f"is {ac.get(sym)}")
    for sym in ("CONFIG_MCUMGR_GRP_SHELL", "CONFIG_MCUMGR_GRP_FS", "CONFIG_MCUMGR_GRP_STAT",
                "CONFIG_MCUMGR_GRP_OS_TASKSTAT", "CONFIG_MCUMGR_GRP_OS_ECHO", "CONFIG_SHELL",
                "CONFIG_MCUMGR_GRP_SETTINGS", "CONFIG_MCUMGR_GRP_ENUM"):
        check(ac.get(sym) in (None, "n"), f"app {sym} is off (minimal MCUmgr groups)", f"is {ac.get(sym)}")
    adts = (b / APP / "zephyr/zephyr.dts").read_text()
    check("zephyr,uart-mcumgr = &cdc_acm_uart0" in adts, "app MCUmgr transport is the USB CDC ACM UART")

    # ---- signed image ----------------------------------------------------------------
    signed = b / APP / "zephyr/zephyr.signed.bin"
    info = run(*imgtool, "dumpinfo", str(signed))
    hdr = int(re.search(r"hdr_size:\s+0x([0-9a-f]+)", info).group(1), 16)
    img = int(re.search(r"img_size:\s+0x([0-9a-f]+)", info).group(1), 16)
    ver = re.search(r"version:\s+(\S+)", info).group(1)
    check("ECDSASIG" in info and "SHA256" in info, "signed image carries SHA256 and ECDSA signature TLVs")
    size = signed.stat().st_size
    if sectors:
        limit = (sectors - 1) * ERASE - 0x1000  # one sector for swap-using-move, ~trailer reserve
        check(size <= limit, "signed image fits slot0 (swap-using-move: minus one sector, minus trailer)",
              f"{size} B of {limit} B usable ({100*size/limit:.1f}%), header 0x{hdr:x}, payload {img} B, version {ver}")
    if a.key:
        out = subprocess.run([*imgtool, "verify", "-k", a.key, str(signed)], capture_output=True, text=True)
        check(out.returncode == 0 and "valid" in (out.stdout + out.stderr).lower(),
              "imgtool verifies the signature with the DEV key", (out.stdout + out.stderr).strip().splitlines()[-1])
        # the check must be able to fail: Nordic's public sample key must NOT verify our image
        neg = subprocess.run([*imgtool, "verify", "-k", str(ncs / "bootloader/mcuboot/root-ec-p256.pem"),
                              str(signed)], capture_output=True, text=True)
        check(neg.returncode != 0, "imgtool REJECTS the image under Nordic's sample key (negative control)")
        # key hygiene: bootloader embeds the dev public key, not Nordic's sample key
        def key_bytes(pem):
            c = run(*imgtool, "getpub", "-k", pem, "-l", "c")
            return re.findall(r"0x[0-9a-f]{2}", c)
        mcu_c = (b / "mcuboot/zephyr/autogen-pubkey.c").read_text()
        embedded = re.findall(r"0x[0-9a-f]{2}", mcu_c)
        dev = key_bytes(a.key)
        nordic = key_bytes(str(ncs / "bootloader/mcuboot/root-ec-p256.pem"))
        check(embedded == dev, "MCUboot embeds the DEV public key", f"{len(embedded)} key bytes")
        check(embedded != nordic, "MCUboot does NOT embed Nordic's public sample key (root-ec-p256.pem)")
    # no private key material in the build tree
    leaked = [p for p in b.rglob("*") if p.is_file() and p.suffix in (".pem", ".key")]
    check(not leaked, "no .pem/.key files inside the build tree", ", ".join(map(str, leaked)))

    # ---- hex placement ---------------------------------------------------------------
    try:
        from intelhex import IntelHex
        def span(path):
            ih = IntelHex(str(path))
            return ih.minaddr(), ih.maxaddr()
        lo, hi = span(b / "mcuboot/zephyr/zephyr.hex")
        check(0 <= lo and hi < app_p["boot_partition"][0] + app_p["boot_partition"][1],
              "MCUboot image lies inside boot_partition", f"0x{lo:x}..0x{hi:x} ({hi-lo+1} B of {app_p['boot_partition'][1]} B)")
        lo, hi = span(b / APP / "zephyr/zephyr.signed.hex")
        s0 = app_p["slot0_partition"]
        check(lo >= s0[0] and hi < s0[0] + s0[1], "signed app lies inside slot0_partition", f"0x{lo:x}..0x{hi:x}")
    except ImportError:
        check(False, "intelhex module available for hex placement check")

    # ---- report ----------------------------------------------------------------------
    sizes = []
    for name, elf in (("MCUboot", b / "mcuboot/zephyr/zephyr.elf"), ("application", b / APP / "zephyr/zephyr.elf")):
        out = run("arm-none-eabi-size", str(elf)).splitlines()[1].split()
        text, data, bss = int(out[0]), int(out[1]), int(out[2])
        sizes.append((name, text, data, bss))
    lines = ["# Brick nRF52840 build check", "", f"Build: `{b}`", ""]
    lines += ["| result | check | detail |", "|---|---|---|"]
    lines += [f"| {'PASS' if ok else '**FAIL**'} | {w} | {d} |" for ok, w, d in rows]
    lines += ["", "| image | FLASH (text+data) | RAM (data+bss) |", "|---|---|---|"]
    lines += [f"| {n} | {t + d} B | {d + bs} B |" for n, t, d, bs in sizes]
    report = "\n".join(lines)
    print(report)
    if a.md:
        Path(a.md).write_text(report + "\n")
    print(f"\n{len(rows) - len(failures)}/{len(rows)} checks passed", file=sys.stderr)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
