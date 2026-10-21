# Sourced by the other scripts. Locates the NCS v3.4.1 workspace and the toolchain.
#
#   NCS_DIR       west workspace root that contains nrf/, zephyr/, bootloader/  (required)
#   NCS_VENV      python venv with west and NCS requirements (default: $NCS_DIR/../ncs-venv)
#   BRICK_TOOLCHAIN
#       nrfutil  (default) - the official NCS toolchain bundle, activated by the caller
#                            ('nrfutil sdk-manager toolchain launch ...') BEFORE running these scripts
#       gnuarmemb          - GNU Arm Embedded from the host (GNUARMEMB_TOOLCHAIN_PATH), used by CI and
#                            by the sandbox that produced the numbers in docs/NRF52840_BRINGUP.md
#                            (NOT the official bundle: sizes can differ by a few hundred bytes)

BRICK_PLAT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export BRICK_PLAT_DIR

: "${NCS_DIR:?set NCS_DIR to the west workspace root of nRF Connect SDK v3.4.1 (see scripts/setup_ncs.sh)}"
[ -d "$NCS_DIR/nrf" ] || { echo "NCS_DIR=$NCS_DIR has no nrf/" >&2; return 1 2>/dev/null || exit 1; }

NCS_VENV="${NCS_VENV:-$(dirname "$NCS_DIR")/ncs-venv}"
if [ -f "$NCS_VENV/bin/activate" ]; then
    # shellcheck disable=SC1091
    . "$NCS_VENV/bin/activate"
fi

if [ "${BRICK_TOOLCHAIN:-nrfutil}" = "gnuarmemb" ]; then
    export ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb
    export GNUARMEMB_TOOLCHAIN_PATH="${GNUARMEMB_TOOLCHAIN_PATH:-/usr}"
fi
export ZEPHYR_BASE="$NCS_DIR/zephyr"

BRICK_DEV_KEY_DIR="${BRICK_DEV_KEY_DIR:-${XDG_CONFIG_HOME:-$HOME/.config}/brick/dev-keys}"
BRICK_DEV_KEY="${BRICK_DEV_KEY:-$BRICK_DEV_KEY_DIR/brick-dev-ecdsa-p256.pem}"
export BRICK_DEV_KEY
