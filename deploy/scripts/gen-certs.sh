#!/usr/bin/env bash
set -e

# 1. Capture arguments with defaults to prevent empty subject lines
IP=${1:? "Usage: $0 <PI_IP> <WIFI_SSID> <WIFI_PASS> <DEVICE_ID> <MQTT_PASS> <MQTT_PORT>"}
WIFI_SSID=${2:-"Unknown_SSID"}
WIFI_PASS=${3:-"no_password"}
DEVICE_ID=${4:-"default_id"}
MQTT_PASS=${5:-"dev"}
MQTT_PORT=${6:-"port"}

# 2. Get ABSOLUTE paths (Stops the "Arduino works but ESP fails" pathing issue)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FOG_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ROOT_DIR="$(cd "$FOG_DIR/.." && pwd)"
CERTS_DIR="$FOG_DIR/certs"

# Define targets using absolute paths
ARDUINO_H="$ROOT_DIR/mesh_node/certs/ca_cert.hpp"
ESP_H="$ROOT_DIR/mesh_esp_version/src/config/user_config.h"

echo "📂 Project Root: $ROOT_DIR"
echo "🔐 Target Cert Folder: $CERTS_DIR"

mkdir -p "$CERTS_DIR"
cd "$CERTS_DIR"

# 3. Generate Basic/Legacy Certs (The ones you said work)
echo "Generating CA and Server certs..."
openssl req -x509 -new -nodes -days 365 -subj "/CN=Legacy-CA" -keyout ca.key -out ca.crt
openssl req -newkey rsa:2048 -nodes -subj "/CN=$IP" -keyout server.key -out server.csr
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out server.crt -days 365

# 4. Create the v3 Config (Needed for the ESP Mesh Certs)
cat > openssl_v3.conf << EOF
[req]
distinguished_name = req_distinguished_name
prompt = no
[req_distinguished_name]
CN = Fog-CA
[v3_ca]
basicConstraints = critical, CA:TRUE
keyUsage = critical, digitalSignature, cRLSign, keyCertSign
[v3_server]
basicConstraints = CA:FALSE
keyUsage = digitalSignature, keyEncipherment
extendedKeyUsage = serverAuth
subjectAltName = IP:$IP, IP:127.0.0.1, DNS:localhost
[v3_client]
basicConstraints = CA:FALSE
keyUsage = digitalSignature
extendedKeyUsage = clientAuth
EOF

# 5. Generate Modern CA
openssl genrsa -out fog-ca.key 2048
openssl req -x509 -new -nodes -key fog-ca.key -sha256 -days 365 -out fog-ca.crt -config openssl_v3.conf -extensions v3_ca -subj "/CN=Fog-CA"

# 6. Generate the Mesh Client Cert (The one that was failing)
echo "Generating Mesh Client Cert for ID: $DEVICE_ID"
openssl genrsa -out mesh-client.key 2048
openssl req -new -key mesh-client.key -subj "/CN=mesh-node-$DEVICE_ID" -out mesh-client.csr
openssl x509 -req -in mesh-client.csr -CA fog-ca.crt -CAkey fog-ca.key -CAcreateserial -out mesh-client.crt -days 365 -sha256 -extfile openssl_v3.conf -extensions v3_client

# 7. Generate Rust Fog Cert
openssl genrsa -out rust-fog.key 2048
openssl req -new -key rust-fog.key -subj "/CN=fog-node-dev" -out rust-fog.csr
openssl x509 -req -in rust-fog.csr -CA fog-ca.crt -CAkey fog-ca.key -CAcreateserial -out rust-fog.crt -days 365 -sha256 -extfile openssl_v3.conf -extensions v3_client

# 8. PRE-WRITE CHECK: Verify files exist
if [ ! -f "mesh-client.crt" ]; then
    echo "❌ CRITICAL ERROR: mesh-client.crt was not created. Stopping."
    exit 1
fi

# 9. Read contents into variables (Avoids shell issues inside the file-write)
CA_CONTENT=$(cat ca.crt)
MESH_CRT_CONTENT=$(cat mesh-client.crt)
MESH_KEY_CONTENT=$(cat mesh-client.key)
FOG_CA_CONTENT=$(cat fog-ca.crt)

# 10. Write Arduino Header
echo "📝 Writing Arduino Header..."
mkdir -p "$(dirname "$ARDUINO_H")"
cat > "$ARDUINO_H" <<EOF
#pragma once
static const char ca_cert[] = R"EOF(
$CA_CONTENT
)EOF";
EOF

# 11. Write ESP32 Header
echo "📝 Writing ESP32 Header to $ESP_H"
mkdir -p "$(dirname "$ESP_H")"
cat > "$ESP_H" <<EOF
#pragma once

// ===== WiFi =====
#define WIFI_SSID "$WIFI_SSID"
#define WIFI_PASS "$WIFI_PASS"
#define DEVICE_ID $DEVICE_ID

// ===== MQTT =====
#define BROKER_IP "mqtts://$IP:8883"
#define MQTT_BROKER "$IP"
#define MQTT_PORT "$MQTT_PORT"
#define MQTT_USERNAME "mesh_node_$DEVICE_ID"
#define MQTT_PASSWORD "$MQTT_PASS"
#define PUB_TOPIC "mesh/node/$DEVICE_ID/edge"

// ===== TLS =====
static const char ca_cert[] = R"EOF(
$CA_CONTENT
)EOF";
EOF

rm -f *.csr *.srl openssl_v3.conf
echo "✅ SUCCESS: All certs and headers created."