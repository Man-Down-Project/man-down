#!/usr/bin/env bash
set -e

IP=${1:? "Usage: $0 <PI_IP>"}
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

cat > openssl_v3.conf <<EOF
[req]
distinguished_name = req_distinguished_name
prompt = no
[req_distinguished_name]
CN = Fog-CA
[v3_ca]
basicConstraints = critical,CA:TRUE
keyUsage = critical, digitalSignature, cRLSign, keyCertSign
[v3_req]
basicConstraints = CA:FALSE
keyUsage = nonRepudiation, digitalSignature, keyEncipherment
subjectAltName = IP:$IP, IP:127.0.0.1, DNS:localhost
EOF

# Modern CA
openssl genrsa -out fog-ca.key 2048
openssl req -x509 -new -nodes -key fog-ca.key -sha256 -days $DAYS \
    -out fog-ca.crt -config openssl_v3.conf -extensions v3_ca -subj "/CN=Fog-CA"

# Modern Server Cert (Listener 8884)
openssl genrsa -out fog-server.key 2048
openssl req -new -key fog-server.key -subj "/CN=$IP" -out fog-server.csr
openssl x509 -req -in fog-server.csr -CA fog-ca.crt -CAkey fog-ca.key -CAcreateserial \
    -out fog-server.crt -days $DAYS -sha256 -extfile openssl_v3.conf -extensions v3_req

# Modern Client Cert (For Rust app)
openssl genrsa -out rust-fog.key 2048
openssl req -new -key rust-fog.key -subj "/CN=fog-node-dev" -out rust-fog.csr
openssl x509 -req -in rust-fog.csr -CA fog-ca.crt -CAkey fog-ca.key -CAcreateserial \
    -out rust-fog.crt -days $DAYS -sha256 -extfile openssl_v3.conf -extensions v3_req

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