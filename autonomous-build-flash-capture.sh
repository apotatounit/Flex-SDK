#!/usr/bin/env bash
# Full autonomous cycle: build in Codespace, download, flash device, capture serial for N seconds.
# Run this in your host terminal (with gh auth, ssh-add, and venv) so the agent can iterate
# by re-running this and reading SERIAL_CAPTURE_FILE.
#
# Usage:
#   ./autonomous-build-flash-capture.sh [--push] [PORT] [CAPTURE_SECONDS]
#   SERIAL_CAPTURE_FILE=/path/to/out.txt ./autonomous-build-flash-capture.sh
#
set -euo pipefail

PUSH=""
PORT="${UPDATER_PORT:-/dev/cu.usbmodem1101}"
CAPTURE_SECONDS=90
SERIAL_CAPTURE_FILE="${SERIAL_CAPTURE_FILE:-./serial_capture.txt}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --push) PUSH="--push"; shift ;;
    /dev/*) PORT="$1"; shift ;;
    [0-9]*) CAPTURE_SECONDS="$1"; shift ;;
    *) echo "Unknown: $1"; shift ;;
  esac
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "==> Step 1: Build in Codespace and download..."
if [[ -n "$PUSH" ]]; then
  COMMIT_MSG="${COMMIT_MSG:-autonomous build and flash cycle}" ./build-via-codespace.sh --push
else
  ./build-via-codespace.sh
fi

echo ""
echo "==> Step 2: Flash device and capture serial for ${CAPTURE_SECONDS}s to $SERIAL_CAPTURE_FILE..."
python3 scripts/updater.py -m ./build/user_application.bin -s -p "$PORT" -w
python3 scripts/updater.py -l -p "$PORT" 2>&1 | tee "$SERIAL_CAPTURE_FILE" &
CAPTURE_PID=$!
sleep "$CAPTURE_SECONDS"
kill $CAPTURE_PID 2>/dev/null || true
wait $CAPTURE_PID 2>/dev/null || true

echo ""
echo "==> Done. Serial output saved to $SERIAL_CAPTURE_FILE"
echo "    Ask the agent to read and evaluate it, then iterate if needed."
