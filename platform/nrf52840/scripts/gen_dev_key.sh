#!/usr/bin/env bash
# Generate a DEVELOPMENT MCUboot signing key (ECDSA P-256) if it does not exist yet.
#
#   NCS_DIR=... scripts/gen_dev_key.sh [path]
#
# The key lives OUTSIDE the repository (default ~/.config/brick/dev-keys/) and is never committed.
# A device whose bootloader contains this public key accepts images signed with it: do not
# use such a device or key for release. The production key is created offline, stored
# outside GitHub, and its public part is built into the production MCUboot.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
# shellcheck disable=SC1091
. "$here/env.sh"

key="${1:-$BRICK_DEV_KEY}"
if [ -s "$key" ]; then
    echo "dev key exists: $key"
    exit 0
fi
mkdir -p "$(dirname "$key")"
chmod 700 "$(dirname "$key")"
umask 077
python "$NCS_DIR/bootloader/mcuboot/scripts/imgtool.py" keygen -k "$key" -t ecdsa-p256
echo "created DEV key: $key"
