#!/usr/bin/env bash
set -euo pipefail

pkill -f "idf_monitor.py|idf.py.*monitor" 2>/dev/null || true
sleep 0.5

cd "$(dirname "$0")"
# shellcheck disable=SC1091
source ./idf-env.sh

PORT="${ESP_PORT:-$(ls /dev/cu.usbmodem* 2>/dev/null | head -1)}"
if [[ -z "${PORT}" ]]; then
  echo "No ESP32 USB port found."
  exit 1
fi

echo "Port: ${PORT}"
idf.py -p "${PORT}" build flash monitor
