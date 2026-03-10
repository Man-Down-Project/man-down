mod events;
mod mqtt;

use events::{Envelope, Incident};
use tokio::sync::{mpsc, watch};

#[tokio::main]
async fn main() {
    let _ = dotenvy::dotenv();
    env_logger::init();

    if let Ok(host) = std::env::var("MQTT_HOST") {
        log::info!("MQTT_HOST={}", host);
    }

    let (tx, mut rx) = mpsc::channel::<Envelope>(100);

    let (shutdown_tx, shutdown_rx) = watch::channel(false);

    let processor = tokio::spawn(run_processor(rx));

    let mqtt_task = tokio::spawn({
        let mqtt_tx = tx.clone();
        let shutdown_rx = shutdown_rx.clone();
        async move {
            if let Err(e) = mqtt::start_mqtt_tls(mqtt_tx, shutdown_rx).await {
                log::error!("MQTT task exited with error: {}", e);
            }
        }
    });

    if let Err(e) = tokio::signal::ctrl_c().await {
        log::error!("Failed to listen for Ctrl+C: {}", e);
    }

    log::info!("Ctrl+C received, shutting down...");
    let _ = shutdown_tx.send(true);

    drop(tx);

    let _ = mqtt_task.await;
    let _ = processor.await;

    log::info!("Shutdown complete");
}

async fn run_processor(mut rx: mpsc::Receiver<Envelope>) {
    while let Some(env) = rx.recv().await {
        if let Err(e) = env.validate_basic() {
            log::warn!("Dropped invalid envelope {}", e);
            continue;
        }
        process_envelope(env).await;
    }

    log::info!("Processor: channel closed, exiting");
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
