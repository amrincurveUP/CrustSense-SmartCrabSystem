#!/bin/bash
# Install LaunchAgent so CurveUp backend starts at login and restarts if it crashes
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
PLIST="$HOME/Library/LaunchAgents/com.curveup.backend.plist"
LABEL="com.curveup.backend"

mkdir -p "$HOME/Library/LaunchAgents" "$ROOT/logs" "$ROOT/data"

# Pin to Node 16 — better-sqlite3 in this project is built for NODE_MODULE_VERSION 93.
# Do NOT auto-pick newest nvm (v22 breaks the native addon).
NODE="$HOME/.nvm/versions/node/v16.20.2/bin/node"
if [ ! -x "$NODE" ]; then
  # Fallback: first node that can load better-sqlite3
  NODE=""
  for candidate in \
    "$HOME/.nvm/versions/node"/v16*/bin/node \
    "$(command -v node 2>/dev/null || true)"
  do
    [ -x "$candidate" ] || continue
    if "$candidate" -e "require('$ROOT/node_modules/better-sqlite3')" >/dev/null 2>&1; then
      NODE="$candidate"
      break
    fi
  done
fi
if [ -z "${NODE:-}" ] || [ ! -x "$NODE" ]; then
  echo "No compatible Node found for better-sqlite3. Install Node 16 or run: npm rebuild better-sqlite3"
  exit 1
fi

NODE_DIR="$(dirname "$NODE")"
PATH_EXTRA="${NODE_DIR}:/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin"

# Stop ad-hoc nohup copies so LaunchAgent owns the port
pkill -f "$ROOT/src/index.js" 2>/dev/null || true
sleep 1

cat > "$PLIST" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>Label</key>
  <string>${LABEL}</string>
  <key>ProgramArguments</key>
  <array>
    <string>${NODE}</string>
    <string>${ROOT}/src/index.js</string>
  </array>
  <key>WorkingDirectory</key>
  <string>${ROOT}</string>
  <key>EnvironmentVariables</key>
  <dict>
    <key>MQTT_OPTIONAL</key>
    <string>0</string>
    <key>PORT</key>
    <string>3000</string>
    <key>DB_PATH</key>
    <string>${ROOT}/data/aquaculture.db</string>
    <key>PATH</key>
    <string>${PATH_EXTRA}</string>
  </dict>
  <key>RunAtLoad</key>
  <true/>
  <key>KeepAlive</key>
  <true/>
  <key>ThrottleInterval</key>
  <integer>5</integer>
  <key>StandardOutPath</key>
  <string>${ROOT}/logs/backend.launchd.log</string>
  <key>StandardErrorPath</key>
  <string>${ROOT}/logs/backend.launchd.err</string>
</dict>
</plist>
EOF

UID_NUM="$(id -u)"
launchctl bootout "gui/${UID_NUM}/${LABEL}" 2>/dev/null || true
launchctl unload "$PLIST" 2>/dev/null || true
launchctl bootstrap "gui/${UID_NUM}" "$PLIST"
launchctl enable "gui/${UID_NUM}/${LABEL}" 2>/dev/null || true
launchctl kickstart -k "gui/${UID_NUM}/${LABEL}"

echo "Installed and started: $PLIST"
echo "Node: $NODE ($($NODE -v))"
echo "Prefer URL: http://crabit.local:3000/"
echo "Localhost:  http://127.0.0.1:3000/"
echo "Stop later: launchctl bootout gui/${UID_NUM}/${LABEL}"
