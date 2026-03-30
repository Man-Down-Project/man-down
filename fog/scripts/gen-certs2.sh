#!/bin/bash

# Configuration - Change these to match your environment
CA_CN="Fog-CA"
SERVER_CN="192.168.0.106" # Your Broker IP
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
IP.1 = 192.168.0.16
IP.2 = 127.0.0.1
DNS.1 = localhost
EOF

echo "1. Generating Root CA..."
openssl genrsa -out ca.key 2048
openssl req -x509 -new -nodes -key ca.key -sha256 -days $DAYS \
    -subj "/CN=$CA_CN" -out ca.crt -config openssl_v3.conf -extensions v3_ca

echo "2. Generating Server Certificate (for Mosquitto)..."
openssl genrsa -out server.key 2048
openssl req -new -key server.key -subj "/CN=$SERVER_CN" -out server.csr
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial \
    -out server.crt -days $DAYS -sha256 -extfile openssl_v3.conf -extensions v3_req

echo "3. Generating Client Certificate (for Rust/Arduino)..."
openssl genrsa -out fog.key 2048
openssl req -new -key fog.key -subj "/CN=$CLIENT_CN" -out fog.csr
openssl x509 -req -in fog.csr -CA ca.crt -CAkey ca.key -CAcreateserial \
    -out fog.crt -days $DAYS -sha256 -extfile openssl_v3.conf -extensions v3_req

# Cleanup
rm *.csr openssl_v3.conf ca.srl
echo "Done! Move these to your certs/ directory."