#!/usr/bin/env bash
# Wake/start FlexSense app and listen on serial output (no firmware flash).
#
# Usage:
#   ./wake-and-listen.sh
#   ./wake-and-listen.sh /dev/serial/by-id/usb-Myriota_...-if00
#   UPDATER_PORT=/dev/ttyACM0 ./wake-and-listen.sh
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PY="${SCRIPT_DIR}/scripts/updater.py"

detect_port() {
  # Prefer stable by-id path for FlexSense command interface.
  local p
  for p in /dev/serial/by-id/*FlexSense*if00 /dev/serial/by-id/*FlexSense*if03; do
    [[ -e "$p" ]] && { printf '%s\n' "$p"; return 0; }
  done
  for p in /dev/ttyACM0 /dev/ttyUSB0; do
    [[ -e "$p" ]] && { printf '%s\n' "$p"; return 0; }
  done
  return 1
}

PORT="${UPDATER_PORT:-${1:-}}"
if [[ -z "${PORT}" ]]; then
  PORT="$(detect_port || true)"
fi

if [[ -n "${PORT}" ]]; then
  echo "Using serial port: ${PORT}"
  exec python3 "$PY" -s -l -p "$PORT" -w
fi

echo "No serial port explicitly set; falling back to updater auto-detect."
exec python3 "$PY" -s -l
