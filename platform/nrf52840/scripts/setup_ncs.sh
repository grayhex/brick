#!/usr/bin/env bash
# Create a reproducible nRF Connect SDK v3.4.1 workspace OUTSIDE the Brick repository.
#
#   scripts/setup_ncs.sh <workspace-parent-dir>
#
# Result:  <dir>/ncs          west workspace pinned to tag v3.4.1 (sdk-nrf)
#          <dir>/ncs-venv     python 3.12 venv with west and the NCS/Zephyr/MCUboot requirements
#
# The compiler is NOT installed here: use the official toolchain bundle
# (nrfutil sdk-manager) or set BRICK_TOOLCHAIN=gnuarmemb with GNU Arm Embedded (see env.sh).
set -euo pipefail

NCS_TAG="v3.4.1"
parent="${1:?usage: setup_ncs.sh <workspace-parent-dir>}"
mkdir -p "$parent"
parent="$(cd "$parent" && pwd)"
python="${PYTHON:-python3.12}"

if [ ! -d "$parent/ncs-venv" ]; then
    "$python" -m venv "$parent/ncs-venv"
fi
# shellcheck disable=SC1091
. "$parent/ncs-venv/bin/activate"
python -m pip install --upgrade pip
python -m pip install "west==1.5.0"

if [ ! -d "$parent/ncs/.west" ]; then
    west init -m https://github.com/nrfconnect/sdk-nrf --mr "$NCS_TAG" "$parent/ncs"
fi
cd "$parent/ncs"
west update --narrow -o=--depth=1
# Pinned tag check: refuse to continue on anything but the named release.
actual="$(git -C nrf describe --tags --exact-match 2>/dev/null || true)"
if [ "$actual" != "$NCS_TAG" ]; then
    echo "nrf is at '$actual', expected $NCS_TAG" >&2
    exit 1
fi
python -m pip install -r zephyr/scripts/requirements.txt \
                      -r nrf/scripts/requirements.txt \
                      -r bootloader/mcuboot/scripts/requirements.txt
echo "NCS $NCS_TAG ready: NCS_DIR=$parent/ncs"
