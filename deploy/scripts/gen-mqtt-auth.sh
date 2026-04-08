#!/usr/bin/env bash
set -e

BASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "Using fog base directory: $BASE_DIR"

mkdir -p "$BASE_DIR/mosquitto-data"

cat > "$BASE_DIR/aclfile" <<EOF
# ===== FOG =====
user fog_user
topic read mesh/node/#
topic write mesh/provisioning/#
topic write edge/provisioning/#

# ===== MESH =====
user mesh_user
topic read mesh/provisioning/#
topic read edge/provisioning/#
topic write mesh/node/#
EOF

echo "Created aclfile"

rm -f "$BASE_DIR/passwordfile"

echo "Creating users..."
mosquitto_passwd -b -c "$BASE_DIR/passwordfile" fog_user dev
mosquitto_passwd -b "$BASE_DIR/passwordfile" mesh_user dev

echo "Created passwordfile"
echo "Done."