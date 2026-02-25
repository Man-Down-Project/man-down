# Man-Down Security Node

A distributed industrial safety system designed for deterministic edge safety decisions and minimal, incident-based data persistence.

Built with:

- **ESP32 (C + FreeRTOS)** at the edge  
- **Rust** at the fog layer

The system is explicitly designed as a **safety incident recorder**, not a monitoring or tracking platform.

Data is persisted only when:

- A confirmed `man_down` event occurs  
- An edge device loses mesh connectivity beyond a defined safety threshold  
- Worker logs in or out (with timestamp)  
- Battery level reported by edge device falls below threshold  

No continuous telemetry is stored.

---

# System Architecture

[ Edge Node (ESP32 + Secure Mesh) ] → [ Fog Node (Rust) ] → [ Optional Cloud ]

Safety logic executes locally on the edge.  
Fog validates and stores safety-critical incidents only.  

System operation does not depend on cloud availability.

---

# Edge Layer (ESP32, C + FreeRTOS)

## Responsibilities

- Accelerometer sampling (volatile memory only)  
- Fall detection state machine  
- Local alarm activation (LED + buzzer)  
- Manual reset handling  
- NFC/RFID registration events (login/logout, device association)  
- Secure mesh communication  
- MQTT over Mutual TLS  
- Sequence-based replay protection  
- Retry with explicit acknowledgment for critical events  
- Battery level monitoring  
- Watchdog for connectivity loss  

## Critical Constraint

All sensor data:  

- Exists only in RAM  
- Is never written to flash  
- Is never stored persistently  
- Is never transmitted unless a qualifying safety event occurs  

---

# Fog Layer (Rust)

## Responsibilities

- MQTT subscription (QoS 1)  
- Mutual TLS authentication  
- Event validation  
- Sequence-based deduplication  
- Incident storage (encrypted)  
- Alarm escalation  
- Store **worker_id**, **device_id**, **man_down events**, **connection watchdog events**, **login/logout timestamps**, and **battery levels**  

Fog acts as:

- Validation authority  
- Incident recorder  
- Escalation coordinator  

---

# Event Model

## Events That Trigger Persistence

1. Confirmed `man_down`  
2. Prolonged mesh connectivity loss (`watchdog`)  
3. Worker login/logout  
4. Battery level alert  

--- 

# Data Persistence Policy

## Stored (Encrypted)

- `worker_id`  
- `device_id`  
- `event_type`  
- Timestamp  
- Battery level (if relevant)  
- Coarse zone hint  
- Validation metadata  

## Not Stored

- Continuous acceleration logs  
- Orientation history  
- Movement tracking  
- Behavioral metrics  
- Biometric data  
- Continuous heartbeat logs  
- Location history  

_All non-critical data is processed transiently and discarded immediately._

---

# Storage Security (Fog)

- SQLite database  
- SQLCipher full-database encryption  
- Column-level encryption for sensitive fields  
- Secure key storage  
- Key rotation support  

_Edge devices store no persistent sensitive data._

---

# Failure Handling Guarantees

- **If mesh fails:**  
  → Local alarm remains active  
  → `mesh_disconnect` event logged if threshold exceeded  

- **If fog is unavailable:**  
  → Edge continues independent operation  

- **If MQTT fails:**  
  → Retry logic activates  

- **If acknowledgment not received:**  
  → Alarm escalation continues locally  

_Safety does not depend on cloud connectivity._

---

# NFC / RFID Model

Used for:

- Worker check-in  
- Worker check-out  
- Optional device association  

RFID interactions:

- Not logged unless associated with a safety event  
- Not stored as tracking history  
- Not used for behavioral analytics  

---

# Why C (Edge) and Rust (Fog)?

## C on Edge

The edge environment is:

- Real-time  
- Resource-constrained  
- Interrupt-driven  

C provides:

- Deterministic timing  
- Direct hardware control  
- Minimal runtime overhead  
- Tight memory footprint  
- Mature ESP32 ecosystem support  

_Safety logic must operate without unpredictable latency or memory overhead._  
_C ensures strict timing guarantees for alarm triggering._

## Rust on Fog

The fog node is:

- Network-facing  
- Concurrent  
- Encryption-heavy  
- Storage-oriented  

Rust provides:

- Memory safety without garbage collection  
- Concurrency without data races  
- Protection against buffer overflows  
- Reduced remote exploitation risk  
- High performance comparable to C++  

_This minimizes security vulnerabilities in network-exposed components._

---

# Privacy & Compliance Model

The system is designed to align with:

- Privacy by Design  
- Data Minimization  
- Purpose Limitation (Workplace Safety Only)  
- Incident-Based Persistence  
- Pseudonymized Identifiers  

_The architecture supports compliance with the General Data Protection Regulation (EU 2016/679)._  

_The system is a safety mechanism — not a surveillance platform._

---

# Future Improvements

- Hardware secure element (secure key storage)  
- High-availability fog cluster  
- Secure firmware update pipeline  
- Administrative dashboard (event-only view)

