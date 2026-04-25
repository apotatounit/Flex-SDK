#!/usr/bin/env bash
# Reboot/reset FlexSense via updater bootloader capture, then start app and listen.
#
# Usage:
#   ./reboot-and-listen.sh
#   ./reboot-and-listen.sh /dev/serial/by-id/usb-Myriota_...-if00
#   UPDATER_PORT=/dev/ttyACM0 ./reboot-and-listen.sh
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PY="${SCRIPT_DIR}/scripts/updater.py"

detect_port() {
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

if [[ -z "${PORT}" ]]; then
  echo "No serial port explicitly set; falling back to updater auto-detect."
  # Attempt reset path first, then run.
  python3 "$PY" -v || true
  exec python3 "$PY" -s -l
fi

echo "Using serial port: ${PORT}"
echo "Requesting reboot/reset via bootloader capture..."
# -v runs capture_bootloader() internally, which performs reset flow.
python3 "$PY" -v -p "$PORT" -w || true
exec python3 "$PY" -s -l -p "$PORT" -w
