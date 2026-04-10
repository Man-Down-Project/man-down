use chrono::Utc;
use tokio::sync::mpsc;

use crate::events::{Envelope, Incident};
use crate::rfid::models::{RfidAction, RfidScan};
use crate::rfid::state::RfidSessionState;

pub fn scan_to_envelope(tag_id: String, scan: RfidScan) -> Envelope {
    let incident = match scan.action {
        RfidAction::Login => Incident::Login {
            worker_id: scan.worker_id.clone(),
        },
        RfidAction::Logout => Incident::Logout {
            worker_id: scan.worker_id.clone(),
        },
    };

    Envelope {
        device_id: tag_id,
        mesh_node_id: "fog-rfid".to_string(),
        seq: 1,
        mesh_timestamp: 0,
        received_at: Utc::now(),
        incident,
    }
}

pub async fn run_rfid_service(mut tag_rx: mpsc::Receiver<String>, tx: mpsc::Sender<Envelope>) {
    let mut state = RfidSessionState::new();

    while let Some(tag_id) = tag_rx.recv().await {
        if tag_id.trim().is_empty() {
            continue;
        }

        let action = state.handle_scan(&tag_id);

        let action_str = match action {
            RfidAction::Login => "login",
            RfidAction::Logout => "logout",
        };

        log::info!(
            "RFID EVENT: action={} tag={} worker=worker-01",
            action_str,
            tag_id
        );

        let scan = RfidScan {
            worker_id: "worker-01".to_string(),
            action,
        };

        let env = scan_to_envelope(tag_id.clone(), scan);

        if let Err(e) = tx.send(env).await {
            log::error!("RFID: failed to send event: {}", e);
            return;
        }
    }
}
