use chrono::Utc;
use tokio::sync::mpsc;

use crate::events::{Envelope, Incident};
use crate::rfid::models::{RfidAction, RfidEvent, RfidScan, RfidTagKind, classify_tag};
use crate::rfid::state::RfidSessionState;

pub fn scan_to_envelope(tag_id: String, scan: RfidScan, seq: u8) -> Envelope {
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
        seq,
        mesh_timestamp: 0,
        received_at: Utc::now(),
        incident,
    }
}

pub async fn run_rfid_service(
    mut tag_rx: mpsc::Receiver<String>,
    tx: mpsc::Sender<Envelope>,
    edge_event_tx: mpsc::Sender<RfidEvent>,
) {
    let mut state = RfidSessionState::new();
    let mut seq: u8 = 1;

    while let Some(tag_id) = tag_rx.recv().await {
        if tag_id.trim().is_empty() {
            continue;
        }

        match classify_tag(&tag_id) {
            RfidTagKind::Unknown => {
                log::warn!("Unknown RFID tag prefix: {}", tag_id);
                continue;
            }
            RfidTagKind::Worker => {
                // worker-taggar fortsätter som vanligt
            }
            RfidTagKind::Edge => {
                if let Err(e) = edge_event_tx
                    .send(RfidEvent::EdgeTag {
                        tag_id: tag_id.clone(),
                    })
                    .await
                {
                    log::error!("RFID: failed to send edge tag event: {}", e);
                    return;
                }

                log::info!("Edge tag forwarded for provisioning flow: {}", tag_id);
                continue;
            }
        }

        let action = state.handle_scan(&tag_id);

        let action_str = match action {
            RfidAction::Login => "login",
            RfidAction::Logout => "logout",
        };

        let worker_id = tag_id.clone();

        log::info!(
            "RFID EVENT: action={} tag={} worker={}",
            action_str,
            tag_id,
            worker_id
        );

        let scan = RfidScan { worker_id, action };

        let worker_id = scan.worker_id.clone();

        let env = scan_to_envelope(tag_id.clone(), scan, seq);

        log::info!(
            "RFID AUTH outgoing: action={} tag={} env.device_id={} worker_id={}",
            action_str,
            tag_id,
            env.device_id,
            worker_id
        );

        if let Err(e) = tx.send(env).await {
            log::error!("RFID: failed to send event: {}", e);
            return;
        }
        seq = if seq == 255 { 1 } else { seq + 1 };
    }
}
