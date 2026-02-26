mod events;

use events::{Envelope, Incident};

#[tokio::main]
async fn main() {
    env_logger::init();

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
    env.validate_basic().expect("basic validation failed");

    log::info!("Parsed envelope: {:?}", env);

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
    }
}
