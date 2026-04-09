# Fog Node

The fog node is the **central processing unit** of the system, responsible for:

- Receiving events via MQTT  
- Validating and classifying incidents 
- Processing identity events (RFID login/logout) 
- Triggering BLE provisioning sessions
- Persisting safety-critical data (encrypted)
- Coordinating alerts and system responses  

The fog layer runs entirely **locally** and does not depend on cloud infrastructure.

---

## Architecture Role

The fog node acts as the **core of the event pipeline**:

Edge → Mesh → MQTT → Fog → Storage / Alerts

It ensures that all incoming data is:

- Validated  
- Authenticated  
- Structured  
- Stored securely  

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

From the fog directory:

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

### Run + Test

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

Open **Terminal** 2:
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

Open **Terminal 3** and publish a test event:
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

### Processing + Storage + Notes

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


## Identity & RFID Integration

The fog node handles RFID as a **human-triggered system interaction**.

### Behavior

- First scan → `login`  
- Second scan (same tag) → `logout`  

These events:

- Enter the same event pipeline as all other incidents  
- Are stored in the encrypted database  
- Are processed deterministically within the fog layer  

### BLE Provisioning Trigger

On `login`, the fog node:

1. Registers the `login` event  
2. Starts a **BLE provisioning session (60 seconds)**  
3. Allows edge devices to connect and receive credentials  

On `logout`:

- A `logout` event is stored  
- No BLE activity is triggered  

### Session Control

- Only one provisioning session can run at a time  
- Concurrent login attempts are safely ignored  
- RFID session state is managed locally  

---

## Provisioning

Provisioning is fully controlled by the fog node.

### Responsibilities

- Generate and manage HMAC keys  
- Distribute credentials via MQTT  
- Expose provisioning over BLE  

### MQTT Topics

- `mesh/provisioning/edgeid`  
- `mesh/provisioning/ca`  
- `mesh/provisioning/hmac`  
- `edge/provisioning/hmac`  

### BLE Flow

1. RFID login triggers provisioning  
2. BLE advertising starts  
3. Edge device connects  
4. HMAC payload is transferred  
5. BLE shuts down after 60 seconds  

### Security

- BLE is **not always active**  
- Provisioning requires explicit RFID interaction  
- HMAC keys are rotated periodically  
- All provisioning is local  

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

## Logging

    RUST_LOG=info cargo run
    RUST_LOG=debug cargo run

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
