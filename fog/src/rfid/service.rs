use chrono::Utc;
use tokio::sync::mpsc;

use crate::events::{Envelope, Incident};
use crate::rfid::state::RfidSessionState;

pub fn scan_to_envelope(worker_id: String, is_login: bool) -> Envelope {
    let incident = if is_login {
        Incident::Login {
            worker_id: worker_id.clone(),
        }
    } else {
        Incident::Logout {
            worker_id: worker_id.clone(),
        }
    };

    Envelope {
        device_id: worker_id.clone(),
        mesh_node_id: "fog-rfid".to_string(),
        seq: 1,
        mesh_timestamp: 0,
        received_at: Utc::now(),
        incident,
    }
}

pub async fn run_simulated_rfid(tx: mpsc::Sender<Envelope>) {
    let mut state = RfidSessionState::new();

    let tag_id = "worker-01".to_string();

    loop {
        //simulerar blipp var 5e sekund
        tokio::time::sleep(std::time::Duration::from_secs(5)).await;

        let is_login = state.handle_scan(&tag_id);

        let env = scan_to_envelope(tag_id.clone(), is_login);

        if let Err(e) = tx.try_send(env) {
            log::error!("RFID: failed to sent event: {}", e);
            return;
        }
        if is_login {
            log::info!("RFIF: LOGIN for {}", tag_id);
        } else {
            log::info!("RFID: LOGOUT for {}", tag_id);
        }
    }
}
