use chrono::{DateTime, Utc};

#[derive(Debug, Clone)]
pub struct PendingDeviceSelection {
    pub device_id: String,
    pub selected_at: DateTime<Utc>,
}

#[derive(Debug, Clone)]
pub struct PendingEdgeTag {
    pub rfid_tag: String,
    pub selected_at: DateTime<Utc>,
}

#[derive(Debug, Default)]
pub struct AppState {
    pub selected_device: Option<PendingDeviceSelection>,
    pub pending_edge_tag: Option<PendingEdgeTag>,
}
