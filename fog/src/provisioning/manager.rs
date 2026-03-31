use chrono::Datelike;
use chrono::{Timelike, Utc};
use chrono_tz::Europe::Stockholm;

use super::models::{EdgeIdList, HmacState};

pub fn build_edge_id_payload(edge_ids: &EdgeIdList) -> String {
    edge_ids.ids.join(",")
}

pub fn build_ca_payload(ca_pem: &str) -> String {
    ca_pem.to_string()
}

pub fn build_hmac_mesh_payload(hmac: &HmacState) -> String {
    let now = Utc::now().with_timezone(&Stockholm);

    let suffix = format!(
        "{:02}{:02}{:02}{:02}",
        now.month(),
        now.day(),
        now.hour(),
        now.minute()
    );

    format!("{}{}", hmac.key_hex, suffix)
}

pub fn build_hmac_edge_payload(hmac: &HmacState) -> String {
    hmac.key_hex.clone()
}
