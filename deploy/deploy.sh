#!/bin/bash

# Configuration
PI_USER="beebee" # 👈 Add your Raspberry Pi Zero 2W username here
PI_IP="192.168.0.29" # 👈 Add your Raspberry Pi Zero 2W ip here
PI_PASS="blablabla"  # 👈 Add your password here
DEST="/home/$PI_USER/man_down"
SCRIPTS="$DEST/scripts"
BINARY_PATH="./fog"
DB_ENCRYPTION_KEY="key"

echo "🚀 Starting Deployment..."

echo "📝 Generating local config files..."

# Generate Systemd Service File
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

# Generate Mosquitto Config
cat > mosquitto-dev.conf <<EOF
listener 8883
protocol mqtt
cafile /etc/mosquitto/certs/ca.crt
certfile /etc/mosquitto/certs/server.crt
keyfile /etc/mosquitto/certs/server.key
require_certificate false
use_identity_as_username false
allow_anonymous true

listener 8884
protocol mqtt
cafile /etc/mosquitto/certs/fog-ca.crt
certfile /etc/mosquitto/certs/fog-server.crt
keyfile /etc/mosquitto/certs/fog-server.key
require_certificate true
use_identity_as_username true
allow_anonymous true 
EOF

# Generate .env file
cat > .env <<EOF
MQTT_HOST=127.0.0.1
MQTT_PORT=8884
MQTT_CLIENT_ID=fog-node-dev
MQTT_TOPIC=mesh/node/#

MQTT_USE_TLS=true

MQTT_CA_PATH=$DEST/certs/fog-ca.crt
MQTT_CERT_PATH=$DEST/certs/rust-fog.crt
MQTT_KEY_PATH=$DEST/certs/rust-fog.key

MQTT_KEEP_ALIVE_SECS=60
MQTT_RECONNECT_DELAY_SECS=3

DB_KEY=$DB_ENCRYPTION_KEY
DB_PATH=$DEST/data/fog.db
EOF

# Helper function to run commands with password
run_ssh() {
    sshpass -p "$PI_PASS" ssh -o StrictHostKeyChecking=no "$PI_USER@$PI_IP" "$1"
}

run_rsync() {
    sshpass -p "$PI_PASS" rsync -avz -e "ssh -o StrictHostKeyChecking=no" "$1" "$2"
}

# ... [Generate files] ...

echo "📦 Transferring files..."
run_ssh "mkdir -p $DEST/data $DEST/certs $SCRIPTS"
run_rsync "./scripts/" "$PI_USER@$PI_IP:$SCRIPTS/"
run_rsync "$BINARY_PATH" "$PI_USER@$PI_IP:$DEST/"
run_rsync ".env" "$PI_USER@$PI_IP:$DEST/"
run_rsync "./man_down.service" "$PI_USER@$PI_IP:$DEST/"
run_rsync "./mosquitto-dev.conf" "$PI_USER@$PI_IP:$DEST/"

echo "⚙️ Running remote configuration..."
run_ssh "
    cd $DEST && \
    sudo sh $SCRIPTS/gen-certs.sh && \
    sudo sh $SCRIPTS/gen-mqtt-auth.sh && \
    sudo sh $SCRIPTS/reset-dev-state.sh && \

	# B. Setup Mosquitto (System Copy)
    sudo mkdir -p /etc/mosquitto/certs && \
    sudo cp $DEST/certs/* /etc/mosquitto/certs/ && \
    sudo chown -R mosquitto:mosquitto /etc/mosquitto/certs && \
    sudo chmod 700 /etc/mosquitto/certs && \
    sudo chmod 644 /etc/mosquitto/certs/*.crt && \
    sudo chmod 600 /etc/mosquitto/certs/*.key && \
    sudo mv $DEST/mosquitto-dev.conf /etc/mosquitto/mosquitto.conf && \

    # C. Setup Rust App (Local Copy)
    sudo chown -R $PI_USER:$PI_USER $DEST/certs && \
    chmod 644 $DEST/certs/*.crt && \
    chmod 600 $DEST/certs/*.key && \
    
    # D. Service Management
    sudo mv $DEST/man_down.service /etc/systemd/system/ && \
    sudo systemctl daemon-reload && \
    sudo systemctl restart mosquitto && \
    sudo systemctl enable man_down && \
    sudo systemctl restart man_down && \

    # E. Set Capabilities for Bluetooth (Must be after binary transfer)
    sudo setcap 'cap_net_raw,cap_net_admin+eip' $DEST/fog
"
echo "✅ DONE! Man Down is deployed and running."
    
   