#!/usr/bin/env bash
# Launch the verified demo with a separate player settings profile.
set -euo pipefail
DEMO_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname -- "$DEMO_DIR")"
UAT_BINARY="${OPENSPACETTD_BINARY:-$PROJECT_DIR/build/openttd}"
UAT_PROFILE="$PROJECT_DIR/build/connected-uat-player"
UAT_SAVE="$DEMO_DIR/OpenSpaceTTD-Connected-Economy-UAT-v2.0.sav"
if [[ ! -x "$UAT_BINARY" || ! -f "$UAT_SAVE" ]]; then
    echo "Build OpenSpaceTTD and keep the connected UAT save in demo/." >&2
    exit 1
fi
mkdir -p "$UAT_PROFILE"
if [[ ! -f "$UAT_PROFILE/connected.cfg" ]]; then
    cp "$DEMO_DIR/connected_economy.cfg" "$UAT_PROFILE/connected.cfg"
fi
exec "$UAT_BINARY" -c "$UAT_PROFILE/connected.cfg" -g "$UAT_SAVE" "$@"
