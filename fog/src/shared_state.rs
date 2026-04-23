#[derive(Debug, Clone)]
pub struct PendingDeviceSelection {
    pub device_id: String,
}

#[derive(Debug, Clone)]
pub struct PendingEdgeTag {
    pub rfid_tag: String,
}

#[derive(Debug, Default)]
pub struct AppState {
    pub selected_device: Option<PendingDeviceSelection>,
    pub pending_edge_tag: Option<PendingEdgeTag>,
}
