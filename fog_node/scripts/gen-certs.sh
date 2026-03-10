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

echo "ca_cert.hpp generated successfully."