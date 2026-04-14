#!/bin/bash

# Configuration for your own setup
PI_USER="pi"        # <--change to device name
PI_IP="192.168.X.X"    # <--add ip adress to device
PI_PASS="dev"
DB_ENCRYPTION_KEY="key" # <--change to proper encryption key
WIFI_SSID="wifi name"
WIFI_PASS="wifi pass"
MQTT_PORT="8883"        # the listener port for arduino 
DEVICE_ID="2"          # <--- for the esp mesh node
MQTT_PASS="dev"
#--------------------------------------------

LOCAL_HEADER_DIR="../mesh_node/certs"
LOCAL_HEADER_DIR_ESP="../mesh_esp_version/config"
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
chmod +x ./scripts/gen-certs.sh
echo "🔐 Generating certificates locally..."
mkdir -p "$LOCAL_HEADER_DIR"
mkdir -p "$LOCAL_HEADER_DIR_ESP"
./scripts/gen-certs.sh "$PI_IP" "$WIFI_SSID" "$WIFI_PASS" "$DEVICE_ID" "$MQTT_PASS"

cross build --release --target $TARGET

if [ $? -ne 0 ]; then
    echo "❌ Build failed! Aborting deployment."
    exit 1
fi

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

user mesh_node_$DEVICE_ID
topic read mesh/provisioning/#
topic read edge/provisioning/#
topic write mesh/node/#
EOF

rm -f "./passwordfile"
mosquitto_passwd -b -c "./passwordfile" fog_user $MQTT_PASS
mosquitto_passwd -b "./passwordfile" mesh_user $MQTT_PASS
mosquitto_passwd -b "./passwordfile" mesh_node_$DEVICE_ID $MQTT_PASS


# 2. Generate Systemd Service
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
TimeoutStopSec=5
KillSignal=SIGTERM
KillMode=process

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
require_certificate false
use_identity_as_username false
allow_anonymous false

listener 8884
protocol mqtt
cafile /etc/mosquitto/certs/fog-ca.crt
certfile /etc/mosquitto/certs/fog-server.crt
keyfile /etc/mosquitto/certs/fog-server.key
require_certificate true
use_identity_as_username false
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
MQTT_PASSWORD=$MQTT_PASS
MQTT_CA_PATH=$DEST/certs/fog-ca.crt
MQTT_CERT_PATH=$DEST/certs/rust-fog.crt
MQTT_KEY_PATH=$DEST/certs/rust-fog.key
MQTT_KEEP_ALIVE_SECS=60
MQTT_RECONNECT_DELAY_SECS=3
DB_KEY=$DB_ENCRYPTION_KEY
DB_PATH=$DEST/data/fog.db
EOF

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
run_ssh "
    sudo apt update && sudo apt install -y mosquitto mosquitto-clients bluez bluez-tools sqlcipher libssl-dev libdbus-1-dev ca-certificates
    
    # Enable hardware and add user to groups
    sudo raspi-config nonint do_spi 0
    sudo usermod -aG bluetooth,spi,gpio,lp,plugdev $PI_USER
    sudo rfkill unblock bluetooth

    # Create DBus policy for Bluetooth
    sudo bash -c \"cat > /etc/dbus-1/system.d/bluetooth-$PI_USER.conf <<DBUS
<!DOCTYPE busconfig PUBLIC '-//freedesktop//DTD D-BUS Bus Configuration 1.0//EN'
 'http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd'>
<busconfig>
  <policy user='$PI_USER'>
    <allow send_destination='org.bluez'/>
    <allow send_interface='org.bluez.GattCharacteristic1'/>
    <allow send_interface='org.bluez.GattService1'/>
    <allow send_interface='org.bluez.Device1'/>
    <allow send_interface='org.freedesktop.DBus.ObjectManager'/>
    <allow send_interface='org.freedesktop.DBus.Properties'/>
  </policy>
</busconfig>
DBUS\"
    sudo systemctl reload dbus

    cd $DEST && chmod +x $DEST/fog
    
    # Clean old state
    sudo systemctl stop mosquitto
    sudo rm -f /var/lib/mosquitto/mosquitto.db
    rm -f $DEST/data/fog.db $DEST/state/hmac.json

    sudo mkdir -p /etc/mosquitto/certs /var/lib/mosquitto
    
    if [ -d \"$DEST/certs\" ] && [ \"\$(ls -A $DEST/certs)\" ]; then
        sudo cp $DEST/certs/* /etc/mosquitto/certs/
    fi
    
    sudo cp $DEST/aclfile /etc/mosquitto/aclfile
    sudo cp $DEST/passwordfile /etc/mosquitto/passwordfile
    sudo chown -R mosquitto:mosquitto /etc/mosquitto/ /var/lib/mosquitto/
    sudo mv $DEST/mosquitto-dev.conf /etc/mosquitto/mosquitto.conf
    sudo chown -R $PI_USER:$PI_USER $DEST

    # Fix Bluetooth Service plugins (Aggressive exclusion to stop D-Bus errors)
    sudo sed -i 's|ExecStart=.*|ExecStart=/usr/libexec/bluetooth/bluetoothd --noplugin=sap,avrcp,a2dp,vcp,mcp,bap|' /lib/systemd/system/bluetooth.service
    
    sudo mv $DEST/man_down.service /etc/systemd/system/
    sudo systemctl daemon-reload
    
    # Modern Bluetooth Power-on
    sudo systemctl restart bluetooth
    sleep 2
    sudo bluetoothctl power on
    sudo bluetoothctl discoverable on
    sudo bluetoothctl pairable on
    
    sudo systemctl restart mosquitto
    
    # Set capabilities for hardware access
    sudo setcap 'cap_net_raw,cap_net_admin,cap_sys_rawio+eip' $DEST/fog
    sudo systemctl enable man_down
    sudo systemctl restart man_down
"

echo "✅ DONE! Man Down is deployed and running."
echo "🔄 Syncing generated C++ headers back to laptop..."

REMOTE_HEADER="/home/$PI_USER/mesh_node/certs/ca_cert.hpp"

sshpass -p "$PI_PASS" scp -o StrictHostKeyChecking=no "$PI_USER@$PI_IP:$REMOTE_HEADER" "$LOCAL_HEADER_DIR/ca_cert.hpp"

echo "✅ Local C++ headers updated from Pi."

SETUP_OUTPUT="../mesh_node/src/personal_setup.hpp"
cat > "$SETUP_OUTPUT" <<EOF
#pragma once
//wifi config
#define WIFI_SSID "$WIFI_SSID"
#define WIFI_PASS "$WIFI_PASS"

//MQTT config
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