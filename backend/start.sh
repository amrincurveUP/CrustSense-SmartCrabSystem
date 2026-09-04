#!/bin/bash
# CurveUp Smart Aquaculture — ensure backend is always running
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
LOG_DIR="$ROOT/logs"
LABEL="com.curveup.backend"
PLIST="$HOME/Library/LaunchAgents/${LABEL}.plist"
mkdir -p "$LOG_DIR" "$ROOT/data"

export MQTT_OPTIONAL="${MQTT_OPTIONAL:-0}"
export PORT="${PORT:-3000}"
export DB_PATH="${DB_PATH:-$ROOT/data/aquaculture.db}"

# Prefer LaunchAgent (survives Terminal close + login reboot)
if [ ! -f "$PLIST" ]; then
  echo "Installing LaunchAgent (auto-start + KeepAlive)..."
  "$ROOT/install-launchagent.sh"
else
  # Refresh plist node path if needed, then restart
  "$ROOT/install-launchagent.sh" >/dev/null
fi

sleep 1

# Resolve URLs for the user
LAN_IP="$(ipconfig getifaddr en0 2>/dev/null || true)"
echo "Backend should be running via LaunchAgent (${LABEL})"
echo "Prefer:     http://crabit.local:${PORT}/   ← stable (use this)"
echo "Localhost:  http://127.0.0.1:${PORT}/"
if [ -n "${LAN_IP}" ]; then
  echo "LAN IP:     http://${LAN_IP}:${PORT}/       (changes with WiFi — avoid)"
fi
echo "Health:     curl -s http://127.0.0.1:${PORT}/health"
echo "Logs:       $LOG_DIR/backend.launchd.log"
echo "Stop:       launchctl bootout gui/\$(id -u)/${LABEL}"
