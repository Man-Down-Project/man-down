#!/usr/bin/env bash
set -e

BASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "Resetting dev state..."

rm -f "$BASE_DIR/data/fog.db"
rm -f "$BASE_DIR/state/hmac.json"

echo "Done."