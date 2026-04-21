use chrono::Duration as ChronoDuration;
use chrono::{DateTime, Utc};
use chrono_tz::Europe::Stockholm;
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Envelope {
    pub device_id: String,
    pub mesh_node_id: String,
    pub seq: u8,
    pub mesh_timestamp: u16,
    pub received_at: DateTime<Utc>,
    pub incident: Incident,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(tag = "type")]
pub enum Incident {
    ManDown { zone_hint: Option<String> },
    Gas,
    MeshDisconnect { duration_s: u32 },
    Login { worker_id: String },
    Logout { worker_id: String },
    BatteryLow { battery_level: u8 },
    SensorFault { fault: SensorFault },
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SensorFault {
    pub sensor: SensorType,
    pub severity: FaultSeverity,
    pub code: Option<u32>,
    pub message: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum SensorType {
    Accelerometer,
    Gyroscope,
    Unknown,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum FaultSeverity {
    Warning,
    Critical,
}

impl Envelope {
    pub fn validate_basic(&self) -> Result<(), String> {
        self.validate_common()?;
        self.validate_incident()?;
        Ok(())
    }

    fn validate_common(&self) -> Result<(), String> {
        if self.device_id.trim().is_empty() {
            return Err("device_id is empty".into());
        }
        if !self
            .device_id
            .chars()
            .all(|c| c.is_ascii_hexdigit() || c == ':')
        {
            return Err(format!(
                "device_id must look like a MAC address: {}",
                self.device_id
            ));
        }
        if self.device_id.len() > 17 {
            return Err(format!("device_id too long: {}", self.device_id));
        }
        if self.mesh_node_id.trim().is_empty() {
            return Err("mesh_node_id is empty".into());
        }
        if self.mesh_node_id.len() > 32 {
            return Err(format!("mesh_node_id too long: {}", self.mesh_node_id));
        }
        if !self
            .mesh_node_id
            .chars()
            .all(|c| c.is_ascii_alphanumeric() || c == '-' || c == '_')
        {
            return Err(format!(
                "mesh_node_id contains invalid characters: {}",
                self.mesh_node_id
            ));
        }
        if self.seq == 0 {
            return Err("seq must be > 0".into());
        }
        if self.received_at > Utc::now() + ChronoDuration::minutes(5) {
            return Err("received_at too far in future".into());
        }

        Ok(())
    }

    fn validate_incident(&self) -> Result<(), String> {
        match &self.incident {
            Incident::BatteryLow { battery_level } => {
                if *battery_level > 100 {
                    return Err(format!("invalid battery level: {}", battery_level));
                }
            }

            Incident::Login { worker_id } | Incident::Logout { worker_id } => {
                if worker_id.trim().is_empty() {
                    return Err("worker_id is empty".into());
                }

                if worker_id.len() > 32 {
                    return Err(format!("worker_id too long: {}", worker_id));
                }
            }

            Incident::ManDown { zone_hint } => {
                if let Some(zone) = zone_hint {
                    if zone.trim().is_empty() {
                        return Err("zone_hint is empty".into());
                    }

                    if zone.len() > 16 {
                        return Err(format!("zone_hint too long: {}", zone));
                    }
                }
            }

            Incident::Gas | Incident::MeshDisconnect { .. } | Incident::SensorFault { .. } => {}
        }

        Ok(())
    }
}

#[derive(Debug, Clone)]
pub struct SignedEdgeEvent {
    pub event: EdgeEvent,
    pub hmac_hex: String,
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct EdgeEvent {
    pub device_id: [u8; 6],
    pub event_type: u8,
    pub location: u8,
    pub battery: u8,
    pub seq: u8,
    pub time_stamp: u16,
}

impl EdgeEvent {
    #[allow(dead_code)]
    pub const LEN: usize = 12;

    pub fn from_bytes(b: &[u8]) -> Option<Self> {
        if b.len() != Self::LEN {
            return None;
        }
        let device_id: [u8; 6] = b[0..6].try_into().ok()?;
        let ts_bytes: [u8; 2] = b[10..12].try_into().ok()?;
        let time_stamp = u16::from_le_bytes(ts_bytes);

        Some(Self {
            device_id,
            event_type: b[6],
            location: b[7],
            battery: b[8],
            seq: b[9],
            time_stamp,
        })
    }

    pub fn to_bytes(&self) -> [u8; 12] {
        let ts = self.time_stamp.to_le_bytes();
        [
            self.device_id[0],
            self.device_id[1],
            self.device_id[2],
            self.device_id[3],
            self.device_id[4],
            self.device_id[5],
            self.event_type,
            self.location,
            self.battery,
            self.seq,
            ts[0],
            ts[1],
        ]
    }

    pub fn is_heartbeat(&self) -> bool {
        self.event_type == 0x00
    }

    pub fn heartbeat_log_line(&self, mesh_node_id: &str, received_at: DateTime<Utc>) -> String {
        let stockholm_time = received_at.with_timezone(&Stockholm);

        let mac = format!(
            "{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}",
            self.device_id[0],
            self.device_id[1],
            self.device_id[2],
            self.device_id[3],
            self.device_id[4],
            self.device_id[5],
        );

        format!(
            r#"{{"type":"heartbeat","device_id":"{}","mesh_node_id":"{}","seq":{},"mesh_timestamp":{},"battery_level":{},"received_at":"{}"}}"#,
            mac,
            mesh_node_id,
            self.seq,
            self.time_stamp,
            self.battery,
            stockholm_time.to_rfc3339()
        )
    }

    pub fn to_envelope(self, mesh_node_id: String) -> Envelope {
        let device_id = format!(
            "{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}",
            self.device_id[0],
            self.device_id[1],
            self.device_id[2],
            self.device_id[3],
            self.device_id[4],
            self.device_id[5],
        );

        let incident = match self.event_type {
            0x01 => Incident::ManDown {
                zone_hint: Some(self.location.to_string()),
            },
            0x02 => Incident::Gas,
            0x03 => Incident::BatteryLow {
                battery_level: self.battery,
            },
            0x04 => Incident::Login {
                worker_id: device_id.clone(),
            },
            0x05 => Incident::Logout {
                worker_id: device_id.clone(),
            },
            _ => Incident::SensorFault {
                fault: SensorFault {
                    sensor: SensorType::Unknown,
                    severity: FaultSeverity::Warning,
                    code: Some(self.event_type as u32),
                    message: Some(format!(
                        "Unknown event_type=0x{:02x} location={} battery={} seq={}",
                        self.event_type, self.location, self.battery, self.seq
                    )),
                },
            },
        };

        Envelope {
            device_id,
            mesh_node_id,
            seq: self.seq,
            mesh_timestamp: self.time_stamp,
            received_at: Utc::now(),
            incident,
        }
    }
}

pub fn verify_hmac(signed: &SignedEdgeEvent, key: &[u8]) -> Result<(), String> {
    use hmac::{Hmac, Mac};
    use sha2::Sha256;

    type HmacSha256 = Hmac<Sha256>;

    let mut mac = HmacSha256::new_from_slice(key).map_err(|e| format!("invalid HMAC key: {e}"))?;

    mac.update(&signed.event.to_bytes());

    let expected = hex::decode(&signed.hmac_hex).map_err(|e| format!("invalid hmac hex: {e}"))?;

    if expected.len() != 8 {
        return Err(format!(
            "expected 8-byte HMAC, got {} bytes",
            expected.len()
        ));
    }

    let full = mac.finalize().into_bytes();

    log::info!("DEBUG computed hmac (full): {}", hex::encode_upper(&full));
    log::info!("DEBUG computed hmac (8): {}", hex::encode_upper(&full[..8]));

    if &full[..8] == expected.as_slice() {
        Ok(())
    } else {
        Err("HMAC verification failed".to_string())
    }
}
