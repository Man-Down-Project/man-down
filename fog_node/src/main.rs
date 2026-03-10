mod config;
mod events;
mod mqtt;
mod storage;

use config::MqttConfig;
use events::{Envelope, Incident};
use storage::Storage;
use tokio::sync::{mpsc, watch};

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
    let _ = dotenvy::dotenv();
    env_logger::init();

    let cfg = MqttConfig::from_env()?;
    let storage = Storage::new("fog.db")?;

    log::info!("MQTT broker: {}:{}", cfg.host, cfg.port);

    let (tx, rx) = mpsc::channel::<Envelope>(100);
    let (shutdown_tx, shutdown_rx) = watch::channel(false);

    let mqtt_tx = tx.clone();
    let shutdown_rx = shutdown_rx.clone();
    let cfg = cfg.clone();

    let mqtt_task = tokio::spawn(async move {
        if let Err(e) = mqtt::start_mqtt_tls(cfg, mqtt_tx, shutdown_rx).await {
            log::error!("MQTT task exited with error: {}", e);
        }
    });

    let processor = run_processor(rx, storage);

    tokio::select! {
        _= tokio::signal::ctrl_c() => {
           log::info!("Ctrl+C received, shutting down...");
        }
        _= processor => {
            log::info!("Processor exited");
        }
    }

    let _ = shutdown_tx.send(true);

    drop(tx);

    let _ = mqtt_task.await;

    log::info!("Shutdown complete");
    Ok(())
}

async fn run_processor(mut rx: mpsc::Receiver<Envelope>, storage: Storage) {
    while let Some(env) = rx.recv().await {
        if let Err(e) = env.validate_basic() {
            log::warn!("Dropped invalid envelope: {}", e);
            continue;
        }

        if let Err(e) = storage.insert_event(&env) {
            log::error!("Failed to store event: {}", e);
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
