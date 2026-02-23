# Man-Down Security Node

A distributed safety system built with **ESP32 (C)** at the edge and **Rust** at the fog layer.

This project detects:

- Man-Down events (fall detection)
- Geofence violations
- Suspicious or unstable device behavior via a dynamic Trust Score

Designed for industrial and safety-critical environments with low-latency local detection and secure MQTT communication.

---

## Features

- Real-time fall detection on ESP32
- RTOS-based edge architecture
- Secure MQTT over TLS
- Geofence engine (Fog layer)
- Dynamic Trust Score per device
- Heartbeat & replay protection
- Local fail-safe alarm (buzzer/LED)
- Modular and scalable design

---

## Architecture Overview

The system is divided into two main layers:
[ Edge (ESP32, C) ] → [ Fog (Rust) ] → [ Optional Cloud ]

---

### Edge Layer (ESP32, C + FreeRTOS)

Responsible for real-time detection and low-latency safety decisions.

**Handles:**

- Accelerometer sampling
- Fall detection (state machine)
- Inactivity detection
- Heartbeat messages
- Sequence numbers (anti-replay)
- Local alarm activation
- MQTT publishing over TLS

Edge devices take the **first safety decision**.

---

### Fog Layer (Rust)

Responsible for coordination, validation, and security analysis.

**Handles:**

- MQTT subscription
- Geofence validation
- Rule engine
- Trust Score calculation
- Event logging
- Alarm prioritization & escalation
- Secure storage of sensitive data using **SQLite + SQLCipher**
- Column-level encryption using **libsodium** for highly sensitive fields (e.g., personal identifiers or health metrics)

Fog validates and escalates edge events.

---

## Man-Down Detection

Implemented as a lightweight state machine on the ESP32.

Detection logic includes:

1. Sudden acceleration spike  
2. Impact detection  
3. Orientation change  
4. Inactivity timeout  
5. User confirmation window  

If no acknowledgment is received → emergency event is published.

---

## Geofence Engine

Implemented in Rust on the fog node.

Supports:

- Zone validation via zone ID
- Risk-level based escalation
- Restricted zone detection

Example logic:

IF man_down AND zone_risk > threshold  
→ Critical alarm

IF zone not allowed  
→ Security violation

---

## Trust Score System

Each edge node maintains a dynamic Trust Score (0–100).

Initial score: **100**  
Adjusted continuously based on behavior.

### Score Decreases When:

- Missed heartbeats
- Invalid sequence numbers
- Replay attempts
- Sensor values out of range
- Abnormal timing patterns

### Score Increases When:

- Stable communication
- Valid telemetry
- Consistent sampling

### Response Levels

| Score   | Status         |
|----------|---------------|
| 80–100   | Normal        |
| 50–79    | Warning       |
| 20–49    | Isolate node  |
| <20      | Critical      |

This provides a lightweight intrusion detection mechanism without heavy machine learning.

---

## Communication Model

- MQTT over TLS
- QoS 2 (Exactly once delivery) between Edge and Fog
- Sequence numbers per message
- Heartbeat monitoring
- Event-driven publishing (no unnecessary traffic)

Example payload:

```json
{
  "device_id": "edge01",
  "seq": 1023,
  "timestamp": 17000000,
  "man_down": false,
  "activity_level": 0.02,
  "zone_hint": 2
}

```
## Modular Design

### Edge Modules
- Sensor module
- Detection engine
- Communication module
- Alarm module

### Fog Modules
- MQTT client
- Rule engine
- Trust Score engine
- Logging module
- Secure storage module (SQLite + SQLCipher + libsodium)

This allows easy feature expansion and scalability.

---

## Data Handling

### Collected Data
- Acceleration magnitude
- Activity level
- Orientation changes
- Heartbeat timing

### Processed Data
- Fall events
- Zone violations
- Device trust metrics

### Storage
Fog node stores:
- Event logs (encrypted)
- Trust history (encrypted)
- Alarm history (encrypted)

**Encryption Strategy:**
- Full-database encryption via SQLCipher
- Column-level encryption for sensitive fields via libsodium (e.g., personal identifiers, sensitive health metrics)
- Keys are stored securely in the Fog node and rotated periodically
- Edge devices never store sensitive data in plaintext

---

## Performance Strategy
- Event-driven RTOS tasks
- Edge-side filtering
- Minimal MQTT payload size
- Async Rust backend
- Low-latency local alarm triggering

---

## Security Measures
- MQTT over TLS
- Sequence-based replay protection
- Heartbeat monitoring
- Device-level Trust Score
- Local fail-safe behavior
- Encrypted storage: SQLite database encrypted with SQLCipher
- Sensitive column encryption: libsodium for critical fields
- Key management and rotation at fog layer

Designed with distributed security in mind:  
Edge reacts immediately, Fog verifies, encrypts, and escalates.

---

## Testing Focus
- Fall detection accuracy
- Latency measurement (Edge → Fog)
- Packet loss resilience
- Trust Score stability under normal operation
- Simulated replay and fault scenarios
- Data encryption validation and key rotation tests

---

## Future Improvements
- GPS-based geofencing
- Hardware secure element
- Mutual TLS authentication
- Mobile monitoring interface
- Redundant fog nodes
- ML-enhanced anomaly detection

---

## Why C + Rust?
- C for deterministic real-time control on microcontrollers
- Rust for safe, concurrent, network-facing logic
- Clear separation between low-level control and high-level security