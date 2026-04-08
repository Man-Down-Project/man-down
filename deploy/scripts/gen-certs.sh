#!/bin/bash

# Configuration
PI_USER="beebee" # blablalbla
PI_IP="192.168.0.29"
PI_PASS="blablabla" 
DEST="/home/$PI_USER/man_down"
SCRIPTS="$DEST/scripts"
BINARY_PATH="./fog"
DB_ENCRYPTION_KEY="key"

# Helper function to run commands with password
run_ssh() {
    sshpass -p "$PI_PASS" ssh -o StrictHostKeyChecking=no "$PI_USER@$PI_IP" "$1"
}

run_rsync() {
    sshpass -p "$PI_PASS" rsync -avz -e "ssh -o StrictHostKeyChecking=no" "$1" "$2"
}

echo "🚀 Starting Deployment..."

echo "📝 Generating local config files..."

# 1. Generate Auth Files
cat > aclfile <<EOF
user fog_user
topic read mesh/node/#
topic write mesh/provisioning/#
topic write edge/provisioning/#

user mesh_user
topic read mesh/provisioning/#
topic read edge/provisioning/#
topic write mesh/node/#
EOF

rm -f "./passwordfile"
mosquitto_passwd -b -c "./passwordfile" fog_user dev
mosquitto_passwd -b "./passwordfile" mesh_user dev

# 2. Generate Systemd Service
cat > man_down.service <<EOF
[Unit]
Description=Man Down Application
After=network.target bluetooth.service

[Service]
Type=simple
User=$PI_USER
WorkingDirectory=$DEST
ExecStart=$DEST/fog
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target 
EOF

# 3. Generate Mosquitto Config
cat > mosquitto-dev.conf <<EOF
persistence true
persistence_location /var/lib/mosquitto/
persistence_file mosquitto.db
autosave_interval 300

listener 8883
protocol mqtt
cafile /etc/mosquitto/certs/ca.crt
certfile /etc/mosquitto/certs/server.crt
keyfile /etc/mosquitto/certs/server.key
allow_anonymous true

listener 8884
protocol mqtt
cafile /etc/mosquitto/certs/fog-ca.crt
certfile /etc/mosquitto/certs/fog-server.crt
keyfile /etc/mosquitto/certs/fog-server.key
require_certificate true
use_identity_as_username true

password_file /etc/mosquitto/passwordfile
acl_file /etc/mosquitto/aclfile
EOF

# 4. Generate .env
cat > .env <<EOF
MQTT_HOST=127.0.0.1
MQTT_PORT=8884
MQTT_CLIENT_ID=fog-node-dev
MQTT_TOPIC=mesh/node/#
MQTT_USE_TLS=true
MQTT_CA_PATH=$DEST/certs/fog-ca.crt
MQTT_CERT_PATH=$DEST/certs/rust-fog.crt
MQTT_KEY_PATH=$DEST/certs/rust-fog.key
DB_KEY=$DB_ENCRYPTION_KEY
DB_PATH=$DEST/data/fog.db
EOF

# 5. Create the gen-certs.sh file locally (to be sent to /scripts)
# We use 'EOF' quoted to ensure variables like $IP are evaluated on the PI, not here.
cat > gen-certs.sh <<'EOF'
#!/usr/bin/env bash
set -e

# This runs on the Pi
IP=$(hostname -I | awk '{print $1}')
echo "Detected broker IP: $IP"

# Navigate to the certs directory relative to the script location
# Assuming script is in /home/user/man_down/scripts
BASE_DIR=$(dirname "$(readlink -f "$0")")/..
mkdir -p "$BASE_DIR/certs"
cd "$BASE_DIR/certs"

echo "Generating Certificates in $(pwd)..."

cat > openssl_v3.conf <<V3EOF
[req]
distinguished_name = req_distinguished_name
req_extensions = v3_req
x509_extensions = v3_ca
[req_distinguished_name]
commonName = Common Name
[v3_req]
basicConstraints = CA:FALSE
subjectAltName = IP:$IP,IP:127.0.0.1,DNS:localhost
[v3_ca]
basicConstraints = CA:TRUE
subjectAltName = IP:$IP,IP:127.0.0.1,DNS:localhost
V3EOF

# Standard SSL
openssl req -x509 -new -nodes -days 365 -subj "/CN=Fog-CA" -keyout ca.key -out ca.crt
openssl req -newkey rsa:2048 -nodes -subj "/CN=$IP" -keyout server.key -out server.csr
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out server.crt -days 365

# Mutual TLS for Rust
openssl genrsa -out fog-ca.key 2048
openssl req -x509 -new -nodes -key fog-ca.key -sha256 -days 365 -subj "/CN=Fog-CA-MTLS" -out fog-ca.crt -config openssl_v3.conf -extensions v3_ca
openssl genrsa -out fog-server.key 2048
openssl req -new -key fog-server.key -subj "/CN=$IP" -out fog-server.csr
openssl x509 -req -in fog-server.csr -CA fog-ca.crt -CAkey fog-ca.key -CAcreateserial -out fog-server.crt -days 365 -sha256 -extfile openssl_v3.conf -extensions v3_req
openssl genrsa -out rust-fog.key 2048
openssl req -new -key rust-fog.key -subj "/CN=fog-node-dev" -out rust-fog.csr
openssl x509 -req -in rust-fog.csr -CA fog-ca.crt -CAkey fog-ca.key -CAcreateserial -out rust-fog.crt -days 365 -sha256 -extfile openssl_v3.conf -extensions v3_req

rm *.csr *.srl openssl_v3.conf
echo "Certs generated successfully."
EOF

echo "📦 Transferring files..."
run_ssh "mkdir -p $DEST/data $DEST/state $DEST/certs $SCRIPTS"
run_rsync "$BINARY_PATH" "$PI_USER@$PI_IP:$DEST/"
run_rsync ".env" "$PI_USER@$PI_IP:$DEST/"
run_rsync "./man_down.service" "$PI_USER@$PI_IP:$DEST/"
run_rsync "./mosquitto-dev.conf" "$PI_USER@$PI_IP:$DEST/"
run_rsync "./aclfile" "$PI_USER@$PI_IP:$DEST/"
run_rsync "./passwordfile" "$PI_USER@$PI_IP:$DEST/"
run_rsync "./gen-certs.sh" "$PI_USER@$PI_IP:$SCRIPTS/"

echo "⚙️ Running remote configuration..."
run_ssh "
    # Install dependencies
    sudo apt update && sudo apt install -y mosquitto mosquitto-clients bluez
    
    # Setup permissions
    chmod +x $DEST/fog
    chmod +x $SCRIPTS/gen-certs.sh
    
    # Run the cert script from the scripts folder
    echo 'Executing certificate generation...'
    bash $SCRIPTS/gen-certs.sh
    
    # Configure Mosquitto
    sudo mkdir -p /etc/mosquitto/certs /var/lib/mosquitto
    sudo cp $DEST/certs/* /etc/mosquitto/certs/
    
    sudo mv $DEST/aclfile /etc/mosquitto/aclfile
    sudo mv $DEST/passwordfile /etc/mosquitto/passwordfile
    sudo mv $DEST/mosquitto-dev.conf /etc/mosquitto/mosquitto.conf
    
    sudo chown -R mosquitto:mosquitto /etc/mosquitto/ /var/lib/mosquitto/
    sudo chmod 700 /etc/mosquitto/certs
    
    # Service Management
    sudo mv $DEST/man_down.service /etc/systemd/system/
    sudo systemctl daemon-reload
    sudo systemctl restart mosquitto
    sudo systemctl enable man_down
    sudo systemctl restart man_down
    sudo setcap 'cap_net_raw,cap_net_admin+eip' $DEST/fog
"

# Clean up local temp file
rm gen-certs.sh

echo "✅ DONE! Man Down is deployed."

cat << "EOF"
                    ▓▓▓▓    ▓▓▓▓▓▓▒▒▓▓▓▓▓▓▓▓▓▓                        
                    ▓▓░░▓▓▓▓░░░░▒▒░░░░░░░░░░░░▓▓▓▓▓▓                  
                ▒▒▒▒▓▓▒▒▒▒▒▒░░░░▒▒░░░░░░░░░░░░░░▒▒▓▓▒▒▒▒              
                ▓▓▒▒▒▒▒▒░░░░░░░░░░░░░░░░░░░░░░░░░░▒▒▒▒▓▓              
                  ▓▓▒▒▓▓░░░░░░▒▒░░░░▒▒▒▒░░░░░░▓▓░░░░▓▓                
                ▓▓░░▒▒░░▓▓▓▓░░░░▓▓▓▓░░░░▓▓▓▓░░░░▓▓░░░░▓▓              
                ▓▓░░░░▓▓░░░░▒▒▒▒░░░░▓▓▒▒░░░░▓▓▒▒░░▒▒░░▓▓              
                ▓▓░░▒▒▓▓░░░░░░░░░░░░░░░░░░░░░░░░░░░░▒▒▓▓              
                ▒▒░░░░▓▓░░░░░░░░▓▓▓▓▒▒░░░░░░▓▓▓▓▓▓░░▒▒                
                ▒▒▒▒░░▓▓░░░░░░░░░░  ░░░░░░░░░░░░░░░░▒▒▒▒              
      ▒▒▒▒▒▒▒▒▒▒░░░░▓▓▓▓░░░░░░░░████░░░░░░░░░░████░░▒▒░░▒▒▒▒▒▒▒▒▓▓    
    ▒▒░░░░░░░░▒▒░░░░▓▓░░░░░░░░░░████░░░░░░░░░░████░░▒▒░░▒▒░░░░░░░░▒▒  
    ▒▒░░░░░░░░▒▒░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░▒▒░░░░░░░░▒▒  
    ▒▒░░░░░░░░▒▒▒▒░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░▒▒▒▒░░░░░░░░▒▒  
    ▒▒░░▒▒▒▒▒▒    ▒▒▒▒░░░░░░░░▒▒░░░░░░░░░░░░░░░░░░░░▒▒    ▒▒▒▒▒▒░░▒▒  
    ▒▒░░░░░░▒▒    ▒▒░░░░░░░░░░░░▒▒▒▒▒▒▒▒░░░░░░░░░░░░▒▒    ▒▒░░░░░░▒▒  
    ▒▒░░░░░░▒▒    ▒▒░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░▒▒    ▒▒░░░░░░▒▒  
    ▒▒░░░░░░▒▒    ░░▒▒░░░░░░░░░░░░░░░░░░░░░░░░░░░░▒▒░░    ▒▒░░░░░░▒▒  
    ░░▒▒    ░░▒▒    ▒▒░░░░░░░░░░░░░░░░░░░░░░░░░░░░▒▒    ▒▒░░    ▒▒░░  
      ▓▓▒▒░░░░▓▓      ▒▒░░░░░░░░░░░░░░░░░░░░░░░░▓▓      ▓▓░░░░▒▒▓▓    
      ▓▓▒▒░░░░░░▓▓      ▒▒▒▒░░░░░░  ░░░░░░░░▒▒▒▒      ▓▓░░░░░░▒▒▓▓    
        ▓▓▒▒░░░░░░▓▓▓▓      ▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒      ▓▓▓▓░░░░░░▒▒▓▓      
        ▓▓▒▒░░░░░░░░░░▓▓▓▓▓▓▓▓      ▓▓▒▒    ▓▓▓▓▓▓░░░░░░░░░░▒▒▓▓      
          ▓▓▒▒░░░░░░░░░░░░░░▓▓    ▓▓▒▒▓▓▓▓  ▓▓░░░░░░░░░░░░▒▒▓▓        
            ▓▓▒▒▒▒░░░░░░░░░░░░▓▓  ▓▓▒▒▓▓▓▓  ▓▓░░░░░░░░▒▒▒▒▒▒          
              ▓▓▓▓▒▒░░░░░░░░░░░░▓▓  ░░░░░░▓▓░░░░░░░░▒▒▓▓▓▓            
                  ▓▓▒▒░░░░░░░░░░▓▓  ▒▒▒▒  ▓▓░░░░░░▒▒▓▓                
                    ▓▓▒▒▒▒░░░░░░▓▓  ▒▒▒▒  ▓▓░░░░░░▓▓                  
                      ▓▓▒▒▒▒░░░░░░▓▓▒▒▒▒▓▓░░░░░░▓▓                    
                      ▓▓▒▒▒▒░░░░░░▒▒▒▒▒▒▓▓░░░░░░▓▓                    
                      ▓▓▒▒▒▒░░░░░░▓▓▒▒▓▓▓▓░░░░░░▓▓                    
                      ▓▓▒▒▒▒░░░░░░░░▓▓▓▓░░░░░░░░▓▓                    
                      ▓▓▒▒▒▒░░░░░░░░▒▒▒▒░░░░░░░░▓▓                    
                      ▓▓▒▒▒▒░░░░░░▓▓▒▒▒▒░░░░░░░░▓▓                    
                      ▓▓▒▒▒▒░░░░░░░░▒▒▓▓░░░░░░░░▓▓                    
                      ▓▓▒▒▒▒░░░░░░░░▒▒▓▓░░░░░░░░▓▓                    
                      ▓▓▒▒▒▒░░░░░░░░▒▒▓▓░░░░░░░░▓▓                    
                      ▓▓▒▒▒▒░░░░░░▓▓▒▒▓▓░░░░░░░░▓▓                    
                      ▓▓▒▒▒▒░░░░░░░░▒▒▒▒░░░░░░░░▓▓                    
                      ▓▓▒▒▒▒░░░░░░░░▒▒▒▒░░░░░░░░▓▓                    
                      ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓                    
                      ▓▓▒▒▒▒░░░░░░▓▓▓▓▒▒▒▒▒▒░░░░▓▓                    
                      ▓▓▒▒▒▒░░░░▓▓  ░░▓▓▒▒▒▒░░░░▓▓                    
                      ▓▓▒▒▒▒░░░░▓▓  ░░▓▓▒▒▒▒░░░░▓▓                    
                      ▓▓▒▒▒▒░░░░▓▓  ░░▓▓▒▒▒▒░░░░▓▓                    
                      ▓▓▒▒▒▒░░░░▓▓  ░░▓▓▒▒▒▒░░░░▓▓                    
                      ▓▓▒▒▒▒░░░░▓▓  ░░▓▓▒▒▒▒░░░░▓▓                    
                      ▓▓▒▒▒▒░░░░▓▓  ░░▓▓▒▒▒▒░░░░▓▓                    
                      ▓▓▒▒▒▒░░░░▓▓  ░░▓▓▒▒▒▒░░░░▓▓                    
                      ▓▓▒▒▒▒░░░░▓▓  ░░▓▓▒▒▒▒░░░░▓▓                    
                      ▓▓▒▒▒▒░░░░▓▓  ░░▓▓▒▒▒▒░░░░▓▓                    
                      ▓▓▒▒▓▓▓▓▓▓▓▓  ░░▓▓▒▒▓▓▓▓▓▓▓▓                    
                      ▓▓▓▓▒▒▒▒░░░░▓▓░░▓▓▓▓▒▒▒▒░░░░▓▓                  
                      ▓▓▒▒▒▒▒▒▒▒░░▓▓░░▓▓▒▒▒▒▒▒▒▒░░▓▓                  
                      ▓▓▓▓▓▓▓▓▓▓▓▓▓▓░░▓▓▓▓▓▓▓▓▓▓▓▓▓▓                  
                                                                      
                                                                      
    ▓▓▓▓▓▓  ▓▓    ▓▓    ▓▓▓▓▓▓    ▓▓▓▓▒▒    ▓▓▓▓      ▓▓▓▓▓▓    ▓▓▓▓▓▓
  ░░▓▓        ▓▓    ▓▓  ▓▓        ▓▓        ▓▓    ▓▓  ▓▓        ▓▓      
    ░░▓▓▓▓    ▓▓    ▓▓  ▓▓        ▓▓        ▓▓▓▓▓▓▓▓    ▓▓▓▓      ▓▓▓▓  
          ▓▓  ▓▓    ▓▓  ▓▓        ▓▓        ▓▓              ▓▓        ▓▓
  ░░▓▓▓▓██      ▓▓▓▓▓▓    ▓▓▓▓██    ▓▓▓▓▒▒    ▓▓▓▓▓▓  ▓▓▓▓▓▓    ▓▓▓▓▓▓  
EOF