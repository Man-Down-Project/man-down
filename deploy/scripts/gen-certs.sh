#!/usr/bin/env bash
set -e

IP=${1:? "Usage: $0 <PI_IP> <WIFI_SSID> <WIFI_PASS> <DEVICE_ID>"}
WIFI_SSID=${2:-""}
WIFI_PASS=${3:-""}
DEVICE_ID=${4:-""}
MQTT_PASS=${5:-""}
echo "Detected broker IP: $IP"

mkdir -p certs
cd certs

DAYS=365

# ==========================================
# 1. LEGACY CERTS (For Arduino/C++ Node)
# ==========================================
echo "Generating Legacy CA and Server Certs..."
# CA for Arduino
openssl req -x509 -new -nodes -days $DAYS \
  -subj "/CN=Legacy-CA" \
  -keyout ca.key -out ca.crt

# Server cert for Arduino (Listener 8883)
openssl req -newkey rsa:2048 -nodes \
  -subj "/CN=$IP" \
  -keyout server.key -out server.csr

openssl x509 -req -in server.csr \
  -CA ca.crt -CAkey ca.key -CAcreateserial \
  -out server.crt -days $DAYS

# ==========================================
# 2. MODERN CERTS (For Rust/Modern TLS)
# ==========================================
echo "Generating Modern v3 CERTS (with SAN)..."

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
# cat > openssl_v3.conf <<EOF
# [req]
# distinguished_name = req_distinguished_name
# prompt = no
# [req_distinguished_name]
# CN = Fog-CA
# [v3_ca]
# basicConstraints = critical,CA:TRUE
# keyUsage = critical, digitalSignature, cRLSign, keyCertSign
# [v3_req]
# basicConstraints = CA:FALSE
# keyUsage = nonRepudiation, digitalSignature, keyEncipherment
# subjectAltName = IP:$IP, IP:127.0.0.1, DNS:localhost
# EOF

# Modern CA
openssl genrsa -out fog-ca.key 2048
openssl req -x509 -new -nodes -key fog-ca.key -sha256 -days $DAYS \
    -out fog-ca.crt -config openssl_v3.conf -extensions v3_ca -subj "/CN=Fog-CA"

# Modern Server Cert (Listener 8884)
openssl genrsa -out fog-server.key 2048
openssl req -new -key fog-server.key -subj "/CN=$IP" -out fog-server.csr
openssl x509 -req -in fog-server.csr -CA fog-ca.crt -CAkey fog-ca.key -CAcreateserial \
    -out fog-server.crt -days $DAYS -sha256 -extfile openssl_v3.conf -extensions v3_server

# Modern Client Cert (For Rust app)
openssl genrsa -out rust-fog.key 2048
openssl req -new -key rust-fog.key -subj "/CN=fog-node-dev" -out rust-fog.csr
openssl x509 -req -in rust-fog.csr -CA fog-ca.crt -CAkey fog-ca.key -CAcreateserial \
    -out rust-fog.crt -days $DAYS -sha256 -extfile openssl_v3.conf -extensions v3_client

# Modern Client Cert (For Rust app)
openssl genrsa -out mesh-client.key 2048
openssl req -new -key mesh-client.key -subj "/CN=mesh-node-$DEVICE_ID" -out mesh-client.csr
openssl x509 -req -in mesh-client.csr -CA fog-ca.crt -CAkey fog-ca.key -CAcreateserial \
    -out mesh-client.crt -days $DAYS -sha256 -extfile openssl_v3.conf -extensions v3_client

# ==========================================
# 3. HEADER GENERATION
# ==========================================
echo "Generating node CA header..."
OUTPUT="../../mesh_node/certs/ca_cert.hpp"
mkdir -p "../../mesh_node/certs"

echo "#pragma once" > "$OUTPUT"
echo "static const char ca_cert[] = R\"EOF(" >> "$OUTPUT"
cat ca.crt >> "$OUTPUT" # Uses the Legacy CA
echo ")EOF\";" >> "$OUTPUT"

rm *.csr *.srl openssl_v3.conf
echo "All certificates (Legacy & Modern) generated."

SECURE_OUTPUT="../mesh_node/certs/device_config.hpp"
SECURE_OUTPUT2="../mesh_esp_version/src/config/user_config.h"

echo "#pragma once" > "$SECURE_OUTPUT2"

echo "// ===== WiFi =====" >> "$SECURE_OUTPUT2"
echo "#define WIFI_SSID \"$WIFI_SSID\"" >> "$SECURE_OUTPUT2"
echo "#define WIFI_PASS \"$WIFI_PASS\"" >> "$SECURE_OUTPUT2"
echo "#define DEVICE_ID $DEVICE_ID" >> "$SECURE_OUTPUT2"
echo "" >> "$SECURE_OUTPUT2"
echo "// ===== MQTT =====" >> "$SECURE_OUTPUT2"
echo "#define MQTT_BROKER \"$IP\"" >> "$SECURE_OUTPUT2"
echo "#define MQTT_PORT 8884" >> "$SECURE_OUTPUT2"
echo "#define MQTT_USERNAME \"mesh_node_$DEVICE_ID\"" >> "$SECURE_OUTPUT2"
echo "#define MQTT_PASSWORD \"$MQTT_PASS\"" >> "$SECURE_OUTPUT2"

echo "" >> "$SECURE_OUTPUT2"
echo "// ===== TLS =====" >> "$SECURE_OUTPUT2"

echo "static const char client_cert[] = R\"EOF(" >> "$SECURE_OUTPUT2"
cat mesh-client.crt >> "$SECURE_OUTPUT2"
printf "\n)EOF\";\n" >> "$SECURE_OUTPUT2"

echo "static const char client_key[] = R\"EOF(" >> "$SECURE_OUTPUT2"
cat mesh-client.key >> "$SECURE_OUTPUT2"
printf "\n)EOF\";\n" >> "$SECURE_OUTPUT2"

echo "static const char ca_cert[] = R\"EOF(" >> "$SECURE_OUTPUT2"
cat fog-ca.crt >> "$SECURE_OUTPUT2"
printf "\n)EOF\";\n" >> "$SECURE_OUTPUT2"