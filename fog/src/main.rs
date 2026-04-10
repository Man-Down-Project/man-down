mod app_config;
mod ble;
mod events;
mod mqtt;
mod provisioning;
mod rfid;
mod storage;

use crate::app_config::AppConfig;
use crate::ble::server::{BleProvisioningData, start_ble_server};
use crate::events::{Envelope, Incident};
use crate::mqtt::OutgoingMessage;
use crate::mqtt::start_mqtt;
use crate::provisioning::manager::{build_initial_provisioning_messages, generate_hmac_key_hex};
use crate::provisioning::models::{CaCert, EdgeIdList, HmacState, ProvisioningState};
use crate::provisioning::scheduler::run_hmac_rotation_scheduler;
use crate::provisioning::state::{hmac_needs_rotation, load_hmac_state, save_hmac_state};
use crate::rfid::reader::start_rfid_reader_thread;
use crate::rfid::service::run_rfid_service;
use crate::storage::Storage;
use chrono::Utc;
use std::fs;
use std::sync::Arc;
// use std::sync::atomic::AtomicBool;
use std::sync::atomic::{AtomicBool};
use tokio::sync::{Mutex, mpsc, watch};


#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
    let _ = dotenvy::dotenv();
    env_logger::init();

    let config = AppConfig::from_env()?;
    let storage = Storage::new(&config.db_path, &config.db_key)?;

    let hmac = if let Some(existing) = load_hmac_state() {
        if hmac_needs_rotation(&existing) {
            log::info!("Existing HMAC is older than 7 days, generating new HMAC");

            let new = HmacState {
                key_hex: generate_hmac_key_hex(),
                created_at: Utc::now(),
            };

            let _ = save_hmac_state(&new);
            new
        } else {
            log::info!("Loaded existing HMAC from disk");
            existing
        }
    } else {
        log::info!("No HMAC found on disk, generating new HMAC");

        let new = HmacState {
            key_hex: generate_hmac_key_hex(),
            created_at: Utc::now(),
        };

        let _ = save_hmac_state(&new);
        new
    };

    let provisioning_state = ProvisioningState {
        hmac,
        edge_ids: EdgeIdList {
            ids: vec!["01".to_string(), "02".to_string(), "03".to_string()],
        },
        ca: CaCert {
            pem: fs::read_to_string(&config.mqtt.ca_path)?,
        },
    };

    log::info!("MQTT broker: {}:{}", config.mqtt.host, config.mqtt.port);

    let (tx, rx) = mpsc::channel::<Envelope>(100);
    let (tag_tx, tag_rx) = mpsc::channel::<String>(32);
    let (shutdown_tx, shutdown_rx) = watch::channel(false);
    let (outgoing_tx, outgoing_rx) = mpsc::channel::<OutgoingMessage>(10);

    let ble_running = Arc::new(Mutex::new(false));
    

    let rfid_enabled = Arc::new(AtomicBool::new(true));

    tokio::time::sleep(std::time::Duration::from_secs(3)).await;
    start_rfid_reader_thread(tag_tx, rfid_enabled.clone());

    let rfid_tx = tx.clone();
    let rfid_task = tokio::spawn(async move {
        run_rfid_service(tag_rx, rfid_tx).await;
    });

    let scheduler_tx = outgoing_tx.clone();
    let scheduler_task = tokio::spawn(async move {
        run_hmac_rotation_scheduler(scheduler_tx).await;
    });

    let mqtt_tx = tx.clone();
    let shutdown_rx = shutdown_rx.clone();
    let mqtt_config = config.mqtt.clone();

    let mqtt_task = tokio::spawn(async move {
        if let Err(e) = start_mqtt(mqtt_config, mqtt_tx, shutdown_rx, outgoing_rx).await {
            log::error!("MQTT task exited with error: {}", e);
        }
    });

    publish_initial_provisioning(&outgoing_tx, &provisioning_state).await;

    let processor = run_processor(rx, storage, ble_running.clone(), rfid_enabled.clone());

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

    scheduler_task.abort();
    rfid_task.abort();

    log::info!("Shutdown complete");
    Ok(())
}

async fn publish_initial_provisioning(
    outgoing_tx: &mpsc::Sender<OutgoingMessage>,
    state: &ProvisioningState,
) {
    let messages = build_initial_provisioning_messages(state);

    for msg in messages {
        if let Err(e) = outgoing_tx.send(msg).await {
            log::error!("Failed to queue initial provisioning message: {}", e);
        }
    }
}

async fn run_processor(
    mut rx: mpsc::Receiver<Envelope>,
    storage: Storage,
    ble_running: Arc<Mutex<bool>>,
    rfid_enabled: Arc<AtomicBool>,
) {
    while let Some(env) = rx.recv().await {
        if let Err(e) = env.validate_basic() {
            log::warn!("Dropped invalid envelope: {}", e);
            continue;
        }

        if let Err(e) = storage.insert_event(&env) {
            log::error!("Failed to store event: {}", e);
        }

        process_envelope(env, ble_running.clone(), rfid_enabled.clone()).await;
    }

    log::info!("Processor: channel closed, exiting");
}

async fn process_envelope(
    env: Envelope,
    ble_running: Arc<Mutex<bool>>,
    rfid_enabled: Arc<AtomicBool>,
) {
    let _ = &rfid_enabled;
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
        Incident::Gas => {
            log::warn!("Gas detected");
        }
        Incident::MeshDisconnect { duration_s } => {
            log::warn!("MeshDisconnect duration_s={}", duration_s);
        }
        Incident::Login { worker_id } => {
            log::info!("Login worker_id={}", worker_id);

            let mut running = ble_running.lock().await;
            if *running {
                log::info!("BLE: provisioning already running, skipping new start");
                return;
            }

            *running = true;
            drop(running);

            let hmac_state = match crate::provisioning::state::load_hmac_state() {
                Some(h) => h,
                None => {
                    log::error!("BLE: no HMAC state found on disk");
                    let mut running = ble_running.lock().await;
                    *running = false;
                    return;
                }
            };

            let ble_data = BleProvisioningData::from_hmac_state(&hmac_state);
            let ble_running_for_server = ble_running.clone();
            let ble_running_for_timer = ble_running.clone();

            let (tx, rx) = tokio::sync::oneshot::channel();
            let rfid_enabled_for_ble = rfid_enabled.clone();
            tokio::spawn(async move {
                log::info!("BLE: starting provisioning (60s window)");

                if let Err(e) = start_ble_server(ble_data, rx, rfid_enabled_for_ble).await {
                    log::error!("BLE error: {}", e);
                }

                let mut running = ble_running_for_server.lock().await;
                *running = false;
                log::info!("BLE: provisioning marked as stopped");
            });

            tokio::spawn(async move {
                tokio::time::sleep(std::time::Duration::from_secs(60)).await;
                let _ = tx.send(());
                log::info!("BLE: provisioning window closed");

                let mut running = ble_running_for_timer.lock().await;
                *running = false;
            });
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
