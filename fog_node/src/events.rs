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
