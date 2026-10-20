#!/usr/bin/env bash
# Build MCUboot + the Brick platform application for the nRF52840 DK with west sysbuild.
#
#   NCS_DIR=<workspace> scripts/build.sh [--build-dir DIR] [--variant V] [--sign-version X.Y.Z+B]
#
# variant:  release   normal image; confirms after self-test (default)
#           noconfirm TEST: never confirms        -> MCUboot must revert at the next reset
#           crash     TEST: faults before confirm -> MCUboot must revert
#           hang      TEST: hangs before confirm  -> watchdog resets, MCUboot must revert
#
# Output (in <build-dir>): brick-dk-initial.hex (MCUboot + signed app, for the first flash),
# nrf52840/zephyr/zephyr.signed.bin (for DFU), mcuboot/zephyr/zephyr.elf.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
# shellcheck disable=SC1091
. "$here/env.sh"

build_dir=""; variant="release"; sign_version=""
while [ $# -gt 0 ]; do
    case "$1" in
        --build-dir) build_dir="$2"; shift 2 ;;
        --variant) variant="$2"; shift 2 ;;
        --sign-version) sign_version="$2"; shift 2 ;;
        *) echo "unknown argument: $1" >&2; exit 2 ;;
    esac
done
build_dir="${build_dir:-$BRICK_PLAT_DIR/build/$variant}"

case "$variant" in
    release) extra="" ;;
    noconfirm|crash|hang) extra="$BRICK_PLAT_DIR/variants/$variant.conf" ;;
    *) echo "unknown variant: $variant" >&2; exit 2 ;;
esac

"$here/gen_dev_key.sh"

# sysbuild names the main image after the application directory (here: nrf52840).
app_image="$(basename "$BRICK_PLAT_DIR")"
args=(-DSB_CONFIG_BOOT_SIGNATURE_KEY_FILE="\"$BRICK_DEV_KEY\"")
[ -n "$extra" ] && args+=("-D${app_image}_EXTRA_CONF_FILE=$extra")
[ -n "$sign_version" ] && args+=("-D${app_image}_CONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION=\"$sign_version\"")

cd "$NCS_DIR"
west build --sysbuild -p auto -b nrf52840dk/nrf52840 "$BRICK_PLAT_DIR" -d "$build_dir" -- "${args[@]}"

# One hex for the first flash of a bare DK: MCUboot + signed application (internal flash only;
# slot1 in the external QSPI NOR stays empty until the first DFU).
python "$NCS_DIR/zephyr/scripts/build/mergehex.py" -o "$build_dir/brick-dk-initial.hex" \
    "$build_dir/mcuboot/zephyr/zephyr.hex" "$build_dir/$app_image/zephyr/zephyr.signed.hex"
echo "built: $build_dir"
echo "first flash:  nrfutil device program --firmware $build_dir/brick-dk-initial.hex --options chip_erase_mode=ERASE_ALL"
echo "DFU image:    $build_dir/$app_image/zephyr/zephyr.signed.bin"
