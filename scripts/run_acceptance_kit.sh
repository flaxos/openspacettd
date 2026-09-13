#!/usr/bin/env bash
# OpenSpaceTTD Sprint 29 - Federation Acceptance Kit Runner
# Convenience wrapper script to execute the multi-server acceptance suite.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

echo "==========================================================================="
echo " OpenSpaceTTD Federation Acceptance Kit Runner"
echo "==========================================================================="
echo "Executing Python acceptance test suite..."

python3 "${REPO_ROOT}/scripts/test_sprint29_acceptance_kit.py" "$@"

echo "==========================================================================="
echo " Federation Acceptance Kit Run Completed Successfully!"
echo "==========================================================================="
