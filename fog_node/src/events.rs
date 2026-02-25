use chrono::Utc;
use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize, Debug)]
pub enum EventType {
    ManDown,
    Meshdisconnect,
    Login,
    Logout,
    BatteryLow,
}

#[derive(Serialize, Deserialize, Debug)]
pub struct Incident {
    worker_id: String,
    device_id: String,
    event_type: EventType,
    timestamp: String,
    battery_level: Option<u8>,
}
