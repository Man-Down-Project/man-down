use chrono::{DateTime, Utc};

#[derive(Debug, Clone)]
pub struct PendingDeviceSelection {
    pub device_id: String,
    pub selected_at: DateTime<Utc>,
}
