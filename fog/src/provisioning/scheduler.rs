use crate::mqtt::OutgoingMessage;
use crate::provisioning::manager::{
    build_hmac_edge_payload, build_hmac_mesh_payload, generate_hmac_key_hex,
};
use crate::provisioning::models::HmacState;
use crate::provisioning::state::{hmac_needs_rotation, load_hmac_state, save_hmac_state};
use ::tokio::sync::mpsc::Sender;
use chrono::Utc;
use tokio::time::{Duration, sleep};

pub async fn run_hmac_rotation_scheduler(outgoing_tx: Sender<OutgoingMessage>) {
    loop {
        sleep(Duration::from_secs(60)).await;

        let Some(existing) = load_hmac_state() else {
            continue;
        };

        if !hmac_needs_rotation(&existing) {
            continue;
        }

        log::info!("Scheduler: HMAC needs rotation, generating new key");

        let new_hmac = HmacState {
            key_hex: generate_hmac_key_hex(),
            created_at: Utc::now(),
        };

        if let Err(e) = save_hmac_state(&new_hmac) {
            log::error!("Scheduler: failed to save rotated HMAC: {}", e);
            continue;
        }

        let mesh_payload = build_hmac_mesh_payload(&new_hmac);
        let edge_payload = build_hmac_edge_payload(&new_hmac);

        if let Err(e) = outgoing_tx
            .send(OutgoingMessage {
                topic: "mesh/provisioning/hmac".to_string(),
                payload: mesh_payload,
            })
            .await
        {
            log::info!("Scheduler: failed to publish mesh HMAC: {}", e);
        }

        if let Err(e) = outgoing_tx
            .send(OutgoingMessage {
                topic: "edge/provisioning/hmac".to_string(),
                payload: edge_payload,
            })
            .await
        {
            log::error!("Scheduler: failed to publish edge HMAC: {}", e);
        }
    }
}
