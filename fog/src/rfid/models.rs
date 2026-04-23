#[derive(Debug, Clone)]
pub enum RfidAction {
    Login,
    Logout,
}

#[derive(Debug, Clone)]
pub struct RfidScan {
    pub worker_id: String,
    pub action: RfidAction,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum RfidTagKind {
    Edge,
    Worker,
    Unknown,
}

#[derive(Debug, Clone)]
pub enum RfidEvent {
    EdgeTag { tag_id: String },
}

// Current tag classification is based on known RFID prefixes from our test hardware.
// This is intentionally not universal yet, since different cards/tags can use different
// prefixes depending on hardware/vendor. These values are used for our current setup
// and can be expanded later if more tag types are introduced.
pub fn classify_tag(tag_id: &str) -> RfidTagKind {
    let trimmed = tag_id.trim();

    if trimmed.starts_with("71") || trimmed.starts_with("63") || trimmed.starts_with('5') {
        RfidTagKind::Edge
    } else if trimmed.starts_with('4') || trimmed.starts_with("5") {
        RfidTagKind::Worker
    } else {
        RfidTagKind::Unknown
    }
}
