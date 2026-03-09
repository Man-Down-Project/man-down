use chrono::Duration as ChronoDuration;
use chrono::{DateTime, Utc};
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Envelope {
    pub device_id: String,
    pub mesh_node_id: String,
    pub seq: u64,
    pub sent_at: DateTime<Utc>,
    pub incident: Incident,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(tag = "type")]
pub enum Incident {
    ManDown { zone_hint: Option<String> },
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
        if self.device_id.trim().is_empty() {
            return Err("device_id is empty".into());
        }
        if self.mesh_node_id.trim().is_empty() {
            return Err("mesh_node_id is empty".into());
        }
        if self.seq == 0 {
            return Err("seq must be > 0".into());
        }
        if self.sent_at > Utc::now() + ChronoDuration::minutes(5) {
            return Err("timestamp too far in future".into());
        }
        Ok(())
    }
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct EdgeEvent {
    pub device_id: u8,
    pub event_type: u8,
    pub location: u8,
    pub battery: u8,
    pub seq: u64,
}

impl EdgeEvent {
    pub const LEN: usize = 12;

    pub fn from_bytes(b: &[u8]) -> Option<Self> {
        match b.len() {
            5 => Some(Self {
                device_id: b[0],
                event_type: b[1],
                location: b[2],
                battery: b[3],
                seq: b[4] as u64,
            }),

            12 => {
                let seq_bytes: [u8; 8] = b[4..12].try_into().ok()?;
                let seq = u64::from_le_bytes(seq_bytes);

                Some(Self {
                    device_id: b[0],
                    event_type: b[1],
                    location: b[2],
                    battery: b[3],
                    seq,
                })
            }
            _ => None,
        }
    }

    pub fn to_envelope(self, mesh_node_id: String) -> Envelope {
        let device_id = self.device_id.to_string();

        let incident = match self.event_type {
            0x00 => Incident::Login {
                worker_id: device_id.clone(),
            },
            0x01 => Incident::Logout {
                worker_id: device_id.clone(),
            },
            0x02 => Incident::ManDown {
                zone_hint: Some(self.location.to_string()),
            },
            0x03 => Incident::BatteryLow {
                battery_level: self.battery,
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
            sent_at: Utc::now(),
            incident,
        }
    }
}
