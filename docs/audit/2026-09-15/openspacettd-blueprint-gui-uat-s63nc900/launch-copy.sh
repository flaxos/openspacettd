#!/usr/bin/env bash
set -euo pipefail
case "${1:-crash}" in
  crash) uat_save=/tmp/openspacettd-blueprint-gui-uat-s63nc900/blueprint-crash-copy.sav ;;
  m1) uat_save=/tmp/openspacettd-blueprint-gui-uat-s63nc900/uat-v1.1-copy.sav ;;
  *) echo "Usage: launch-copy.sh [crash|m1]" >&2; exit 2 ;;
esac
cd /tmp/openspacettd-blueprint-gui-uat-s63nc900
exec /home/flax/games/openspacettd/build/openttd -c /tmp/openspacettd-blueprint-gui-uat-s63nc900/openttd.cfg -x -s null -m null -g "$uat_save"
