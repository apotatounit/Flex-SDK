#!/usr/bin/env bash
# Open serial port and read output only (no flash, no start).
#
# Usage:
#   ./listen-serial.sh                    # auto-detect port
#   ./listen-serial.sh /dev/cu.usbmodem1101
#   UPDATER_PORT=/dev/cu.usbmodem1101 ./listen-serial.sh
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PY="${SCRIPT_DIR}/scripts/updater.py"

set -- -l
if [[ -n "${UPDATER_PORT:-}" ]]; then
  set -- "$@" -p "$UPDATER_PORT" -w
elif [[ -n "${1:-}" && "$1" == /dev/* ]]; then
  set -- "$@" -p "$1" -w
fi
exec python3 "$PY" "$@"
