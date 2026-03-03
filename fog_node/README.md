# Fog Host – Quick Setup

This project runs:

- Mosquitto MQTT broker (TLS enabled)
- Rust fog application (event subscriber)

Each developer runs everything locally.

---

## 1. Install Mosquitto

### Ubuntu / WSL

```bash
sudo apt update
sudo apt install mosquitto mosquitto-clients
```

### macOS

```bash
brew install mosquitto
```

## Setup project

From the fog_node/ directory:

```bash
cp .env.example .env
cp mosquitto.conf.example mosquitto-dev.conf
./scripts/gen-certs.sh
```
This creates local certificates in certs/

## Start the broker

Open Terminal 1:
```bash
./scripts/start-mosquitto.sh
```

## Start Fog

Open Terminal 2:
```bash
cargo run
```

# Test with manual Publish
```bash
mosquitto_pub \
  --cafile certs/ca.crt \
  --cert certs/fog.crt \
  --key certs/fog.key \
  -h 127.0.0.1 \
  -p 8883 \
  -t "md/v1/device/test/events" \
  -m '{"device_id":"test","mesh_node_id":"mesh1","seq":1,"sent_at":"2024-01-01T00:00:00Z","incident":{"type":"Login","worker_id":"123"}}'
  ```
  If everything works, the fog app will log the received event.

  
