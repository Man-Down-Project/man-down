mod events;

use events::{Envelope, Incident};
use tokio::sync::mpsc;

#[tokio::main]
async fn main() {
    env_logger::init();

    let (tx, mut rx) = mpsc::channel::<Envelope>(100);

    let processor = tokio::spawn(async move {
        while let Some(env) = rx.recv().await {
            if let Err(e) = env.validate_basic() {
                log::warn!("Dropped invalid envelope: {}", e);
                continue;
            }

            process_envelope(env).await;
        }
    });

    let raw = r#"
    {
    "device_id": "esp32-01",
    "mesh_node_id": "mesh-01",
    "seq": 42,
    "sent_at": "2026-02-26T18:00:00Z",
    "incident": {
      "type": "ManDown",
      "zone_hint": "Zone-A"
}
}
"#;
    let env: Envelope = serde_json::from_str(raw).expect("Failed to parse Envelope");
    tx.send(env).await.expect("processor dropped");

    drop(tx); //stäng av kanalen så prosessorn kan avsluta (demo)

    processor.await.unwrap(); //vänta tills processen blir klar
}

async fn process_envelope(env: Envelope) {
    log::info!(
        "Processing device_id={} seq={} incident={:?}",
        env.device_id,
        env.seq,
        env.incident
    );

    match env.incident {
        Incident::ManDown { zone_hint } => {
            log::warn!("ManDown! zone_hint={:?}", zone_hint);
        }
        Incident::MeshDisconnect { duration_s } => {
            log::warn!("MeshDisconnect duration_s={}", duration_s);
        }
        Incident::Login { worker_id } => {
            log::info!("Login worker_id={}", worker_id);
        }
        Incident::Logout { worker_id } => {
            log::info!("Logout worker_id={}", worker_id);
        }
        Incident::BatteryLow { battery_level } => {
            log::warn!("BatteryLow level={}", battery_level);
        }
        Incident::SensorFault { fault } => {
            log::warn!(
                "SensorFault sensor={:?} severity={:?} code={:?} message={:?}",
                fault.sensor,
                fault.severity,
                fault.code,
                fault.message
            );
        }
    }
}
