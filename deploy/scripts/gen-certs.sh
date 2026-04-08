#!/usr/bin/env bash
set -e

IP=$(hostname -I | awk '{print $1}')
echo "Detected broker IP: $IP"

mkdir -p certs
cd certs

echo "Generating CA..."
openssl req -x509 -new -nodes -days 365 \
  -subj "/CN=Fog-CA" \
  -keyout ca.key -out ca.crt

echo "Generating server certificate..."
openssl req -newkey rsa:2048 -nodes \
  -subj "/CN=$IP" \
  -keyout server.key -out server.csr

openssl x509 -req -in server.csr \
  -CA ca.crt -CAkey ca.key -CAcreateserial \
  -out server.crt -days 365


echo "Generating fog client certificate..."
openssl req -newkey rsa:2048 -nodes \
  -subj "/CN=fog-client" \
  -keyout fog.key -out fog.csr

openssl x509 -req -in fog.csr \
  -CA ca.crt -CAkey ca.key -CAcreateserial \
  -out fog.crt -days 365

rm *.csr *.srl 

echo "Certificates generated successfully."

echo "Generating node CA header..."

OUTPUT="../../mesh_node/certs/ca_cert.hpp"

mkdir -p ../../mesh_node/certs

echo "#pragma once" > $OUTPUT
echo "" >> $OUTPUT
echo "static const char ca_cert[] = R\"EOF(" >> $OUTPUT

cat ca.crt >> $OUTPUT

echo ")EOF\";" >> $OUTPUT

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
