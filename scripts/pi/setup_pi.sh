#!/usr/bin/env bash
set -euo pipefail

APP_DIR="$(cd "$(dirname "$0")" && pwd)"
APP_USER="$(whoami)"
SERVICE_PATH="/etc/systemd/system/fog.service"

echo "Setting up fog from: $APP_DIR"
echo "Running as user: $APP_USER"

mkdir -p "$APP_DIR/data" "$APP_DIR/logs"

chmod +x "$APP_DIR/fog"
chmod +x "$APP_DIR/start.sh"

sudo tee "$SERVICE_PATH" > /dev/null <<EOF
[Unit]
Description=Fog Node Service
After=network.target

[Service]
User=$APP_USER
WorkingDirectory=$APP_DIR
ExecStart=$APP_DIR/start.sh
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable fog.service
sudo systemctl restart fog.service

echo
echo "fog service installed."
echo "Check status with:"
echo "  sudo systemctl status fog.service --no-pager"
echo
echo "View logs with:"
echo "  journalctl -u fog.service -f"
