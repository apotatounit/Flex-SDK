#!/usr/bin/env bash
# Upload firmware, start the device, and open serial for reading in one command.
#
# Usage:
#   ./flash-and-listen.sh                              # auto-detect port, default binary
#   ./flash-and-listen.sh /dev/cu.usbmodem1101         # use this port
#   ./flash-and-listen.sh ./build/user_application.bin /dev/cu.usbmodem1101
#   UPDATER_PORT=/dev/cu.usbmodem1101 ./flash-and-listen.sh
#
set -euo pipefail

BIN=""
PORT="${UPDATER_PORT:-}"
for arg in "${1:-}" "${2:-}"; do
  [[ -z "$arg" ]] && continue
  if [[ "$arg" == /dev/* ]]; then
    PORT="$arg"
  elif [[ -f "$arg" ]]; then
    BIN="$arg"
  fi
done
BIN="${BIN:-./build/user_application.bin}"

if [[ ! -f "$BIN" ]]; then
  echo "Error: Binary not found: $BIN"
  echo "Build first (e.g. ./build-via-codespace.sh) or pass path."
  exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PY="${SCRIPT_DIR}/scripts/updater.py"

set -- -m "$BIN" -s -l
if [[ -n "$PORT" ]]; then
  set -- "$@" -p "$PORT" -w
fi
exec python3 "$PY" "$@"
