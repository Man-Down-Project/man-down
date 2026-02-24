# Man-Down Security Node

A distributed industrial safety system built with **ESP32 (C + FreeRTOS)** at the edge and **Rust** at the fog layer.

Designed for low-latency, safety-critical environments with local decision-making, secure mesh communication, and encrypted fog storage.

---

## Overview

The system detects and responds to:

- Man-Down events (fall detection)
- Geofence violations
- Device instability or suspicious behavior via a dynamic Trust Score

Architecture:

[ Edge (ESP32 + Mesh) ] → [ Fog (Rust) ] → [ Optional Cloud ]

Edge reacts immediately.  
Fog validates, scores, logs, encrypts, and escalates.

---

# Core Features

## Edge Layer (ESP32, C + FreeRTOS)

Real-time safety decision layer.

Handles:

- Accelerometer sampling
- Fall detection (multi-stage state machine)
- Inactivity detection
- Local alarm activation (buzzer / LED)
- Heartbeat transmission
- Sequence-based replay protection
- Mesh communication
- MQTT over TLS (mTLS)
- Event retry with acknowledgment for critical alarms

Edge makes the **first safety decision** to avoid network dependency.

---

## Fog Layer (Rust)

Coordination, validation, and security analysis layer.

Handles:

- MQTT subscription (QoS 1)
- Mutual TLS authentication
- Geofence validation
- Rule engine
- Trust Score calculation
- Event deduplication (sequence-aware)
- Alarm escalation & prioritization
- Secure storage

Storage stack:

- SQLite
- SQLCipher (full database encryption)
- libsodium (column-level encryption for sensitive fields)

Fog acts as a **security validation and escalation authority**.

---

# Communication Model

## Network Layer

- Secure mesh topology between edge devices
- Local routing redundancy
- Timestamp-based validation
- Message expiration handling

## Messaging Layer

- MQTT over **Mutual TLS (mTLS)**
- QoS 1 (At-Least-Once Delivery)
- Application-level idempotency
- Explicit acknowledgment for critical events

### Why QoS 1?

QoS 1 provides:

- Lower latency than QoS 2
- Reduced memory overhead on ESP32
- Reliable delivery with deduplication at fog layer

Critical events (e.g., man_down) use explicit acknowledgment logic in addition to QoS 1.

---

# Event Reliability Strategy

## Sequence-Based Deduplication

Each message includes:

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

---

## Message Validation & Deduplication

Fog stores the last accepted sequence number per device.

Messages with:

- Lower sequence numbers → ignored  
- Duplicate sequence numbers → ignored  
- Expired timestamps → discarded  

This ensures idempotent processing under MQTT QoS 1.

---

## Critical Event Acknowledgment

For safety-critical events:

1. Edge publishes `man_down_event`
2. Fog validates and responds with acknowledgment
3. Edge retries until acknowledgment is received or timeout is reached
4. If no acknowledgment → local escalation persists

This avoids reliance on QoS 2 while maintaining safety reliability.

---

# Man-Down Detection

Implemented as a lightweight state machine on the ESP32.

## Detection Stages

1. Sudden acceleration spike  
2. Impact detection  
3. Orientation change  
4. Inactivity timeout  
5. User confirmation window  

If no acknowledgment is received → emergency event is published.

Edge alarms activate immediately, independent of fog.

---

# Geofence Engine (Fog)

Implemented in Rust.

## Supports

- Zone ID validation  
- Risk-based escalation  
- Restricted zone detection  

## Example Logic

IF `man_down` AND `zone_risk > threshold`  
→ Critical alarm  

IF zone not allowed  
→ Security violation  

---

# Security Model

## Mutual TLS (mTLS)

- All devices authenticate with client certificates  
- Certificate validation at fog  
- Rogue device prevention  
- Certificate revocation support  

---

## Replay Protection

- Per-device sequence numbers  
- Timestamp validation  
- Expiration windows  

---

## Encrypted Storage

Fog node stores:

- Event logs (encrypted)  
- Alarm history (encrypted)  

### Encryption Strategy

- Full-database encryption via SQLCipher  
- Column-level encryption via libsodium (for sensitive fields)  
- Periodic key rotation  
- Secure key storage on fog node  

Edge devices never store sensitive data in plaintext.

---

# Data Handling

## Collected Data

- Acceleration magnitude  
- Orientation changes  
- Activity level  
- Heartbeat timing  
- Zone hints  

## Processed Data

- Fall events  
- Geofence violations  
- Alarm history  

**Data minimization principle applied:**  
Irrelevant data is not persisted.

---

# Performance Strategy

- Event-driven RTOS tasks  
- Edge-side filtering  
- Minimal MQTT payload size  
- Asynchronous Rust backend  
- Defined latency budget per component  
- Local alarm triggering independent of fog  

---

# Testing Focus

- Fall detection accuracy  
- Edge → Fog latency measurement  
- Mesh stability under load  
- Replay simulation  
- Fault injection  
- Encryption validation  
- Key rotation tests  

---

# Future Improvements

- Hardware secure element  
- Redundant fog nodes (HA setup)  
- ML-enhanced anomaly detection  
- GPS-assisted geofencing  
- Fleet management interface  

---

# Why C + Rust?

## C

- Deterministic real-time control  
- Precise hardware interaction  
- Low-level safety logic  

## Rust

- Memory safety  
- Concurrency without data races  
- Secure network-facing services  

Clear separation between real-time safety logic and security orchestration.