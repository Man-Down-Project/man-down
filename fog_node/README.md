# Fog Host – Quick Setup

This project runs:

- Mosquitto MQTT broker (TLS enabled)
- Rust fog application (event subscriber)

Each developer runs everything locally.

---

# 1. Install Mosquitto

## Ubuntu / WSL

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

### Example structure

certs/
  ca.crt
  ca.key
  server.crt
  server.key
  fog.crt
  fog.key

## Start the broker

Open Terminal 1:
```bash
./scripts/start-mosquitto.sh
```
The broker will start with TLS enabled on port:
8883

## Start Fog

Open Terminal 2:
```bash
cargo run
```
You should see logs similar to:
MQTT: connecting host=localhost port=8883
MQTT: connected
MQTT: subscribed

The fog node subscribes to:
md/v1/device/+/events

# Test with manual Publish
Open Terminal 3 and send a test-event:

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

Example output:
Processing device_id=test seq=1 incident=Login
Login worker_id=123
  
## Notes

The fog node accepts both:

JSON events

Binary edge events

Logging level can be controlled with:
```bash
RUST_LOG=info cargo run
```
or
```bash
RUST_LOG=debug cargo run
```



