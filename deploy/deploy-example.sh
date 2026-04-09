#!/bin/bash

# Configuration for your own setup
PI_USER="zero"        # <--change to device name
PI_IP="192.168.X.X"   # <--add ip adress to device
PI_PASS="dev"
DB_ENCRYPTION_KEY="key"      
WIFI_SSID="wifi name"
WIFI_PASS="wifi pass"
MQTT_PORT="8883" 
#--------------------------------------------

LOCAL_HEADER_DIR="../mesh_node/certs"           
DEST="/home/$PI_USER/man_down"
SCRIPTS="$DEST/scripts"

# CROSS COMPILATION CONFIG
TARGET="armv7-unknown-linux-gnueabihf"
BINARY_NAME="fog"
LOCAL_BINARY_PATH="./target/$TARGET/release/$BINARY_NAME"

# Helper function to run commands with password
run_ssh() {
    sshpass -p "$PI_PASS" ssh -o StrictHostKeyChecking=no "$PI_USER@$PI_IP" "$1"
}

run_rsync() {
    sshpass -p "$PI_PASS" rsync -avz -e "ssh -o StrictHostKeyChecking=no" "$1" "$2"
}

echo "🛠️  Step 0: Local Cross-Compilation..."
cross build --release --target $TARGET

if [ $? -ne 0 ]; then
    echo "❌ Build failed! Aborting deployment."
    exit 1
fi

echo "🚀 Starting Deployment..."
echo "📝 Generating local config files..."

# 1. Generate Auth Files
cat > aclfile <<EOF
# ===== FOG (Rust APP) =====
user fog-node-dev
topic read mesh/node/#
topic write mesh/provisioning/#
topic write edge/provisioning/#

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

# FIX: Added fog-node-dev to password file so Certificate Identity is authorized
rm -f "./passwordfile"
mosquitto_passwd -b -c "./passwordfile" fog_user dev
mosquitto_passwd -b "./passwordfile" mesh_user dev
mosquitto_passwd -b "./passwordfile" fog-node-dev dev

# 2. Generate Systemd Service
# FIX: Hardcoded absolute paths for EnvironmentFile and WorkingDirectory
cat > man_down.service <<EOF
[Unit]
Description=Man Down Application
After=network.target bluetooth.service mosquitto.service

[Service]
Type=simple
User=$PI_USER
Group=$PI_USER
SupplementaryGroups=bluetooth dialout
WorkingDirectory=$DEST
EnvironmentFile=$DEST/.env
ExecStart=$DEST/fog
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target 
EOF

# 3. Generate Mosquitto Config
cat > mosquitto-dev.conf <<EOF
# --- Persistence ---
persistence true
persistence_location /var/lib/mosquitto/
persistence_file mosquitto.db
autosave_interval 300

# --- Listeners ---
listener 8883
protocol mqtt
cafile /etc/mosquitto/certs/ca.crt
certfile /etc/mosquitto/certs/server.crt
keyfile /etc/mosquitto/certs/server.key
require_certificate false
use_identity_as_username false
allow_anonymous false

listener 8884
protocol mqtt
cafile /etc/mosquitto/certs/fog-ca.crt
certfile /etc/mosquitto/certs/fog-server.crt
keyfile /etc/mosquitto/certs/fog-server.key
require_certificate true
use_identity_as_username true
allow_anonymous false 

password_file /etc/mosquitto/passwordfile
acl_file /etc/mosquitto/aclfile
EOF

# 4. Generate .env
cat > .env <<EOF
MQTT_HOST=$PI_IP
MQTT_PORT=8884
MQTT_CLIENT_ID=fog-node-dev
MQTT_TOPICS=mesh/node/#
MQTT_USE_TLS=true
MQTT_USERNAME=fog_user
MQTT_PASSWORD=dev
MQTT_CA_PATH=$DEST/certs/fog-ca.crt
MQTT_CERT_PATH=$DEST/certs/rust-fog.crt
MQTT_KEY_PATH=$DEST/certs/rust-fog.key
MQTT_KEEP_ALIVE_SECS=60
MQTT_RECONNECT_DELAY_SECS=3
DB_KEY=$DB_ENCRYPTION_KEY
DB_PATH=$DEST/data/fog.db
EOF

# FIX: Ensure Linux line endings for .env
sed -i 's/\r$//' .env

echo "📦 Transferring files..."
run_ssh "mkdir -p $DEST/data $DEST/state $DEST/certs $SCRIPTS"
run_rsync "./scripts/" "$PI_USER@$PI_IP:$SCRIPTS/"
run_rsync "$LOCAL_BINARY_PATH" "$PI_USER@$PI_IP:$DEST/$BINARY_NAME"
run_rsync ".env" "$PI_USER@$PI_IP:$DEST/"
run_rsync "./man_down.service" "$PI_USER@$PI_IP:$DEST/"
run_rsync "./mosquitto-dev.conf" "$PI_USER@$PI_IP:$DEST/"
run_rsync "./aclfile" "$PI_USER@$PI_IP:$DEST/"
run_rsync "./passwordfile" "$PI_USER@$PI_IP:$DEST/"

echo "⚙️ Running remote configuration..."
# FIX: Escaped variables inside the SSH block to ensure they resolve on the Pi
run_ssh "
    sudo apt update && sudo apt install -y mosquitto mosquitto-clients bluez sqlcipher libssl-dev ca-certificates
    sudo usermod -aG bluetooth $PI_USER
    
    cd $DEST && chmod +x $DEST/fog
    rm -f $DEST/data/fog.db $DEST/state/hmac.json

    sudo sh $SCRIPTS/gen-certs.sh
    sudo mkdir -p /etc/mosquitto/certs /var/lib/mosquitto
    
    if [ -d \"$DEST/certs\" ] && [ \"\$(ls -A $DEST/certs)\" ]; then
        sudo cp $DEST/certs/* /etc/mosquitto/certs/
    fi

    sudo mv $DEST/aclfile /etc/mosquitto/aclfile
    sudo cp $DEST/passwordfile /etc/mosquitto/passwordfile
    
    sudo chown -R mosquitto:mosquitto /etc/mosquitto/
    sudo chown -R mosquitto:mosquitto /var/lib/mosquitto/
    sudo chmod 700 /etc/mosquitto/certs
    
    sudo find /etc/mosquitto/certs/ -name '*.crt' -exec chmod 644 {} +
    sudo find /etc/mosquitto/certs/ -name '*.key' -exec chmod 600 {} +
    sudo chmod 600 /etc/mosquitto/passwordfile
    
    sudo mv $DEST/mosquitto-dev.conf /etc/mosquitto/mosquitto.conf

    sudo chown -R $PI_USER:$PI_USER $DEST/certs
    find $DEST/certs/ -name '*.crt' -exec chmod 644 {} +
    find $DEST/certs/ -name '*.key' -exec chmod 600 {} +
    
    sudo mv $DEST/man_down.service /etc/systemd/system/
    sudo systemctl daemon-reload
    sudo systemctl enable mosquitto
    sudo systemctl restart mosquitto
    sudo systemctl enable man_down
    sudo systemctl restart man_down

    sudo setcap 'cap_net_raw,cap_net_admin+eip' $DEST/fog
"

echo "✅ DONE! Man Down is deployed and running."
echo "🔄 Syncing generated C++ headers back to laptop..."

REMOTE_HEADER="/home/$PI_USER/mesh_node/certs/ca_cert.hpp"
mkdir -p "$LOCAL_HEADER_DIR"
sshpass -p "$PI_PASS" scp -o StrictHostKeyChecking=no "$PI_USER@$PI_IP:$REMOTE_HEADER" "$LOCAL_HEADER_DIR/ca_cert.hpp"

echo "✅ Local C++ headers updated from Pi."

SETUP_OUTPUT="../mesh_node/src/personal_setup.hpp"
cat > "$SETUP_OUTPUT" <<EOF
#pragma once
#define WIFI_SSID "$WIFI_SSID"
#define WIFI_PASS "$WIFI_PASS"
#define MQTT_BROKER "$PI_IP"
#define MQTT_PORT $MQTT_PORT
EOF

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
                      ▓▓▒▒▒▒░░░░░░▓▓▒▒▓▓░░░░░░░░▓▓                    
                      ▓▓▒▒▒▒░░░░░░░░▒▒▒▒░░░░░░░░▓▓                    
                      ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓                    
                      ▓▓▒▒▒▒░░░░░░▓▓▓▓▒▒▒▒▒▒░░░░▓▓                    
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

