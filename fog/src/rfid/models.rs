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
    Worker(RfidScan),
    EdgeTag { tag_id: String },
}

pub fn classify_tag(tag_id: &str) -> RfidTagKind {
    let trimmed = tag_id.trim();

    if trimmed.starts_with("71") {
        RfidTagKind::Edge
    } else if trimmed.starts_with('4') {
        RfidTagKind::Worker
    } else if trimmed.starts_with('5') {
        RfidTagKind::Edge
    } else {
        RfidTagKind::Unknown
    }
}
