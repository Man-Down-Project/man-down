use chrono::Utc;
use tokio::sync::mpsc;

use crate::events::{Envelope, Incident};
use crate::rfid::models::{RfidAction, RfidScan};
use crate::rfid::reader::read_from_rfid_reader;
use crate::rfid::state::RfidSessionState;

pub fn scan_to_envelope(scan: RfidScan) -> Envelope {
    let incident = match scan.action {
        RfidAction::Login => Incident::Login {
            worker_id: scan.worker_id.clone(),
        },
        RfidAction::Logout => Incident::Logout {
            worker_id: scan.worker_id.clone(),
        },
    };

    Envelope {
        device_id: scan.worker_id.clone(),
        mesh_node_id: "fog-rfid".to_string(),
        seq: 1,
        mesh_timestamp: 0,
        received_at: Utc::now(),
        incident,
    }
}

pub async fn run_rfid(tx: mpsc::Sender<Envelope>) {
    let mut state = RfidSessionState::new();

    loop {
        let tag_id = read_from_rfid_reader();

        if tag_id.trim().is_empty() {
            continue;
        }

        let is_login = state.handle_scan(&tag_id);

        let scan = RfidScan {
            worker_id: tag_id.clone(),
            action: if is_login {
                RfidAction::Login
            } else {
                RfidAction::Logout
            },
        };

        let env = scan_to_envelope(scan);

        if let Err(e) = tx.send(env).await {
            log::error!("RFID: failed to send event: {}", e);
            return;
        }

        if is_login {
            log::info!("RFID: LOGIN for {}", tag_id);
        } else {
            log::info!("RFID: LOGOUT for {}", tag_id);
        }
    }
}
