#!/usr/bin/env bash
set -euo pipefail

APP_DIR="$(cd "$(dirname "$0")/../../deploy/pi" && pwd)"

echo "Starting fog_node from: $APP_DIR"

cd "$APP_DIR"

exec ./fog_node
