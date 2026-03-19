# Fog Node

The fog node is the **central processing unit** of the system, responsible for:

- Receiving events via MQTT  
- Validating and classifying incidents  
- Triggering alerts  
- Persisting safety-critical data (encrypted)  

The fog layer runs entirely **locally** and does not depend on cloud infrastructure.

---

## Components

This project includes:

- **MQTT Broker** – Mosquitto (TLS enabled)  
- **Backend Application** – Rust (event subscriber + processor)  
- **Database** – SQLite with SQLCipher encryption  

---

## Quick Setup

Each developer runs the full fog stack locally.

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

## Project Setup

From the fog_node directory:

```bash
cp .env.example .env
cp mosquitto.conf.example mosquitto-dev.conf
./scripts/gen-certs.sh
```

This generates local TLS certificates in:

certs/
  ca.crt
  ca.key
  server.crt
  server.key
  fog.crt
  fog.key


---

### **Block 2 – Run + Test**

```md
## TLS Configuration

- Mutual TLS authentication is used  
- All communication is encrypted  
- Only trusted clients (signed by CA) are allowed  

---

## Run the System

### Start MQTT Broker

Open **Terminal 1**:

```bash
./scripts/start-mosquitto.sh
```
Broker runs on:
localhost:8883 (TLS)

## Start Fog Backend

Open terminal 2:
```bash
cargo run
```
Expected output:
```bash
MQTT: connecting host=localhost port=8883
MQTT: connected
MQTT: subscribed
```

### Test the System

Open Terminal 3 and publish a test event:
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

  Expected output:
  ```bash
  Processing device_id=test seq=1 incident=Login
  Login worker_id=123
  ```


---

### **Block 3 – Processing + Storage + Notes**

## Event Processing

The fog node handles both:

- JSON events (development/testing)  
- Binary edge events (production format)  

Processing steps:

1. Receive MQTT message  
2. Parse payload  
3. Convert to structured envelope  
4. Validate integrity and sequence  
5. Classify incident  
6. Store and/or trigger alert  

---

## Data Storage

- **Database:** SQLite  
- **Encryption:** SQLCipher (full-database encryption)  

### Stored Data

- Device ID  
- Worker ID  
- Event type  
- Timestamp  
- Battery level (if applicable)  
- Validation metadata  

> Only safety-critical events are persisted.

---

##  Logging

```bash
RUST_LOG=info cargo run
```

---

## Notes

- The system runs fully locally  
- No cloud services are required  
- TLS certificates must be generated before startup  
- Edge devices send binary-encoded events in production  

---

## Summary

The fog node acts as:

- Validation authority  
- Incident processor  
- Secure data store  
- Alert coordinator  

It ensures that safety-critical events are handled quickly, securely, and locally.