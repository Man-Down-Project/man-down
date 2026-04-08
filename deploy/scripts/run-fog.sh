#!/usr/bin/env bash
set -e

BASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

cd "$BASE_DIR"

echo "Starting fog..."
RUST_LOG=info cargo run