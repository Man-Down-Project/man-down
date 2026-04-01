use chrono::Datelike;
use chrono::{Timelike, Utc};
use chrono_tz::Europe::Stockholm;
use rand::RngCore;

use crate::mqtt::OutgoingMessage;

use super::models::{EdgeIdList, HmacState, ProvisioningState};

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

pub fn generate_hmac_key_hex() -> String {
    let mut key_bytes = [0u8; 16];
    rand::rngs::OsRng.fill_bytes(&mut key_bytes);
    hex::encode_upper(key_bytes)
}

pub fn build_initial_provisioning_messages(state: &ProvisioningState) -> Vec<OutgoingMessage> {
    let edge_payload = build_edge_id_payload(&state.edge_ids);
    let ca_payload = build_ca_payload(&state.ca.pem);
    let hmac_mesh_payload = build_hmac_mesh_payload(&state.hmac);
    let hmac_edge_payload = build_hmac_edge_payload(&state.hmac);

    vec![
        OutgoingMessage {
            topic: "mesh/provisioning/edgeid".to_string(),
            payload: edge_payload,
        },
        OutgoingMessage {
            topic: "mesh/provisioning/ca".to_string(),
            payload: ca_payload,
        },
        OutgoingMessage {
            topic: "mesh/provisioning/hmac".to_string(),
            payload: hmac_mesh_payload,
        },
        OutgoingMessage {
            topic: "edge/provisioning/hmac".to_string(),
            payload: hmac_edge_payload,
        },
    ]
}
