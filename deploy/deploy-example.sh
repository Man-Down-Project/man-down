#!/bin/bash

# ==========================================
#    USER CONFIGURATION
# ==========================================
PI_USER="pi"
PI_IP="192.168.X.X"
PI_PASS="dev"
DB_ENCRYPTION_KEY="key"
WIFI_SSID="wifi namn"
WIFI_PASS="wifi pass"
MQTT_PORT="8883"
DEVICE_ID="2"
MQTT_PASS="dev"


DEPLOY_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$DEPLOY_DIR/.." && pwd)"
FOG_DIR="$ROOT_DIR/fog"
GEN_CERTS_SCRIPT="$DEPLOY_DIR/scripts/gen-certs.sh"

DEST="/home/$PI_USER/man_down"
SCRIPTS_REMOTE="$DEST/scripts"

echo "🛠️  Step 0: Generating Certificates & C++ Headers..."
if [ ! -f "$GEN_CERTS_SCRIPT" ]; then
    echo "❌ ERROR: Could not find cert script at $GEN_CERTS_SCRIPT"
    exit 1
fi

chmod +x "$GEN_CERTS_SCRIPT"
bash "$GEN_CERTS_SCRIPT" "$PI_IP" "$WIFI_SSID" "$WIFI_PASS" "$DEVICE_ID" "$MQTT_PASS" "$MQTT_PORT"

echo "🛠️  Step 1: Local Cross-Compilation (Rust)..."
cd "$FOG_DIR"

cross build --release --target armv7-unknown-linux-gnueabihf

if [ $? -ne 0 ]; then
    echo "❌ Build failed! Aborting deployment."
    exit 1
fi

echo "📝 Step 2: Generating deployment config files..."

# Generate Auth Files
cat > aclfile <<EOF
user fog_user
topic read mesh/node/#
topic write mesh/provisioning/#
topic write edge/provisioning/#

user mesh_user
topic read mesh/provisioning/#
topic read edge/provisioning/#
topic write mesh/node/#

user mesh_node_$DEVICE_ID
topic read mesh/provisioning/#
topic read edge/provisioning/#
topic write mesh/node/#
EOF

rm -f "./passwordfile"
mosquitto_passwd -b -c "./passwordfile" fog_user $MQTT_PASS
mosquitto_passwd -b "./passwordfile" mesh_user $MQTT_PASS
mosquitto_passwd -b "./passwordfile" mesh_node_$DEVICE_ID $MQTT_PASS

# Generate Systemd Service
cat > man_down.service <<EOF
[Unit]
Description=Man Down Application
After=network.target bluetooth.service mosquitto.service
Requires=bluetooth.service
ExecStartPre=/bin/sleep 2

[Service]
Type=simple
Environment=RUST_LOG=debug
User=$PI_USER
Group=$PI_USER
AmbientCapabilities=CAP_NET_ADMIN CAP_NET_RAW CAP_SYS_RAWIO
Capabilities=CAP_NET_ADMIN CAP_NET_RAW CAP_SYS_RAWIO
SupplementaryGroups=bluetooth spi gpio dialout lp plugdev
WorkingDirectory=$DEST
EnvironmentFile=$DEST/.env
ExecStart=$DEST/fog
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target 
EOF

# Generate Mosquitto Config
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
require_certificate false
allow_anonymous false

listener 8884
protocol mqtt
cafile /etc/mosquitto/certs/fog-ca.crt
certfile /etc/mosquitto/certs/fog-server.crt
keyfile /etc/mosquitto/certs/fog-server.key
require_certificate true
allow_anonymous false 

password_file /etc/mosquitto/passwordfile
acl_file /etc/mosquitto/aclfile
EOF

# Generate .env
cat > .env <<EOF
MQTT_HOST=$PI_IP
MQTT_PORT=8884
MQTT_CLIENT_ID=fog-node-dev
MQTT_TOPICS=mesh/node/#
MQTT_USE_TLS=true
MQTT_USERNAME=fog_user
MQTT_PASSWORD=$MQTT_PASS
MQTT_CA_PATH=$DEST/certs/fog-ca.crt
MQTT_CERT_PATH=$DEST/certs/rust-fog.crt
MQTT_KEY_PATH=$DEST/certs/rust-fog.key
DB_KEY=$DB_ENCRYPTION_KEY
DB_PATH=$DEST/data/fog.db
EOF

sed -i 's/\r$//' .env

echo "📦 Step 3: Transferring files to $PI_IP..."

run_ssh() { sshpass -p "$PI_PASS" ssh -o StrictHostKeyChecking=no "$PI_USER@$PI_IP" "$1"; }
run_rsync() { sshpass -p "$PI_PASS" rsync -avz -e "ssh -o StrictHostKeyChecking=no" "$1" "$2"; }

run_ssh "mkdir -p $DEST/data $DEST/state $DEST/certs $SCRIPTS_REMOTE"

# Transfer everything
run_rsync "$DEPLOY_DIR/scripts/" "$PI_USER@$PI_IP:$SCRIPTS_REMOTE/"
run_rsync "$FOG_DIR/target/armv7-unknown-linux-gnueabihf/release/fog" "$PI_USER@$PI_IP:$DEST/fog" 
run_rsync "$FOG_DIR/.env" "$PI_USER@$PI_IP:$DEST/"
run_rsync "$FOG_DIR/man_down.service" "$PI_USER@$PI_IP:$DEST/"
run_rsync "$FOG_DIR/mosquitto-dev.conf" "$PI_USER@$PI_IP:$DEST/"
run_rsync "$FOG_DIR/aclfile" "$PI_USER@$PI_IP:$DEST/"
run_rsync "$FOG_DIR/passwordfile" "$PI_USER@$PI_IP:$DEST/"
run_rsync "$DEPLOY_DIR/certs/" "$PI_USER@$PI_IP:$DEST/certs/"

echo "⚙️  Step 4: Remote Configuration & Service Start..."
run_ssh "
    sudo systemctl stop mosquitto || true
    
    # Configure Mosquitto
    sudo mkdir -p /etc/mosquitto/certs
    sudo cp $DEST/certs/* /etc/mosquitto/certs/
    sudo cp $DEST/aclfile /etc/mosquitto/aclfile
    sudo cp $DEST/passwordfile /etc/mosquitto/passwordfile
    sudo mv $DEST/mosquitto-dev.conf /etc/mosquitto/mosquitto.conf
    sudo chown -R mosquitto:mosquitto /etc/mosquitto/
    
    # Setup Service
    sudo mv $DEST/man_down.service /etc/systemd/system/
    sudo systemctl daemon-reload
    
    # Restart Services
    sudo systemctl restart mosquitto
    sudo systemctl enable man_down
    sudo systemctl restart man_down
"

SETUP_OUTPUT="$ROOT_DIR/mesh_node/src/personal_setup.hpp"
cat > "$SETUP_OUTPUT" <<EOF
#pragma once
//wifi config
#define WIFI_SSID "$WIFI_SSID"
#define WIFI_PASS "$WIFI_PASS"

//MQTT config
#define MQTT_BROKER "$PI_IP"
#define MQTT_PORT $MQTT_PORT
EOF

echo "✅ DONE! Deployment complete."

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

echo "✅ DONE! Deployment complete."