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
