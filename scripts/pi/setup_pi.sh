#!/usr/bin/env bash
set -euo pipefail

APP_DIR="$(cd "$(dirname "$0")" && pwd)"
APP_USER="$(whoami)"
SERVICE_PATH="/etc/systemd/system/fog_node.service"

echo "Setting up fog_node from: $APP_DIR"
echo "Running as user: $APP_USER"

mkdir -p "$APP_DIR/data" "$APP_DIR/logs"

chmod +x "$APP_DIR/fog_node"
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
sudo systemctl enable fog_node.service
sudo systemctl restart fog_node.service

echo
echo "fog_node service installed."
echo "Check status with:"
echo "  sudo systemctl status fog_node.service --no-pager"
echo
echo "View logs with:"
echo "  journalctl -u fog_node.service -f"
