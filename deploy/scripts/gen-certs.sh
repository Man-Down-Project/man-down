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

openssl req -x509 -new -nodes -days 365 -subj "/CN=Legacy-CA" -keyout ca.key -out ca.crt
openssl req -newkey rsa:2048 -nodes -subj "/CN=$IP" -keyout server.key -out server.csr
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out server.crt -days 365

echo "ca_cert.hpp generated successfully."# Configuration - Change these to match your environment
CA_CN="Fog-CA"
SERVER_CN=$IP # Your Broker IP
CLIENT_CN="fog-node-dev"
DAYS=365

# Create a temporary OpenSSL config for v3 extensions
cat > openssl_v3.conf <<EOF
[req]
distinguished_name = req_distinguished_name
req_extensions = v3_req
x509_extensions = v3_ca

[req_distinguished_name]
commonName = Common Name

[v3_req]
basicConstraints = CA:FALSE
keyUsage = nonRepudiation, digitalSignature, keyEncipherment
subjectAltName = @alt_names

[v3_ca]
basicConstraints = CA:TRUE
keyUsage = digitalSignature, cRLSign, keyCertSign
subjectAltName = @alt_names

[alt_names]
IP.1 = $IP
IP.2 = 127.0.0.1
DNS.1 = localhost
EOF

echo "1. Generating Root CA..."
openssl genrsa -out fog-ca.key 2048
openssl req -x509 -new -nodes -key fog-ca.key -sha256 -days $DAYS \
    -subj "/CN=$CA_CN" -out fog-ca.crt -config openssl_v3.conf -extensions v3_ca

echo "2. Generating Server Certificate (for Mosquitto)..."
openssl genrsa -out fog-server.key 2048
openssl req -new -key fog-server.key -subj "/CN=$SERVER_CN" -out fog-server.csr
openssl x509 -req -in fog-server.csr -CA fog-ca.crt -CAkey fog-ca.key -CAcreateserial \
    -out fog-server.crt -days $DAYS -sha256 -extfile openssl_v3.conf -extensions v3_req

echo "3. Generating Client Certificate (for Rust/Arduino)..."
openssl genrsa -out rust-fog.key 2048
openssl req -new -key rust-fog.key -subj "/CN=$CLIENT_CN" -out rust-fog.csr
openssl x509 -req -in rust-fog.csr -CA fog-ca.crt -CAkey fog-ca.key -CAcreateserial \
    -out rust-fog.crt -days $DAYS -sha256 -extfile openssl_v3.conf -extensions v3_req

# Cleanup
rm *.csr openssl_v3.conf fog-ca.srl
echo "Done! Hope the certs work well for you..."

# 9. Read contents into variables (Avoids shell issues inside the file-write)
CA_CONTENT=$(cat ca.crt)

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