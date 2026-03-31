use chrono::{DateTime, Utc};

#[derive(Debug, Clone)]
pub struct HmacState {
    pub key_hex: String,

    pub created_at: DateTime<Utc>,
}

#[derive(Debug, Clone)]
pub struct EdgeIdList {
    pub ids: Vec<String>,
}

#[derive(Debug, Clone)]
pub struct CaCert {
    pub pem: String,
}

#[derive(Debug, Clone)]
pub struct ProvisioningState {
    pub hmac: HmacState,
    pub edge_ids: EdgeIdList,
    pub ca: CaCert,
}
