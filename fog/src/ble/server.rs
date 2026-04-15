use crate::ble::BleEvent;
use bluer::Session;
use bluer::adv::{Advertisement, AdvertisementHandle};
use bluer::agent::Agent;
use bluer::gatt::local::{
    Application, Characteristic, CharacteristicNotify, CharacteristicNotifyMethod,
    CharacteristicRead, Service,
};
use futures::FutureExt;
use std::sync::{
    Arc,
    atomic::{AtomicBool, Ordering},
};
use tokio::sync::mpsc;
use uuid::Uuid;

use crate::ble::config::PROVISIONING_CHAR_UUID;
use crate::ble::config::PROVISIONING_SERVICE_UUID;
use crate::provisioning::manager::build_hmac_edge_payload;
use crate::provisioning::models::HmacState;

#[derive(Debug, Clone)]
pub struct BleProvisioningData {
    pub hmac_key: String,
}

impl BleProvisioningData {
    pub fn from_hmac_state(hmac: &HmacState) -> Self {
        Self {
            hmac_key: build_hmac_edge_payload(hmac),
        }
    }
}

pub async fn start_ble_server(
    data: BleProvisioningData,
    stop: tokio::sync::oneshot::Receiver<()>,
    rfid_enabled: Arc<AtomicBool>,
    app_state: Arc<tokio::sync::Mutex<crate::shared_state::AppState>>,
    ble_event_tx: mpsc::Sender<BleEvent>,
) -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
    log::info!("BLE: starting provisioning server");
    rfid_enabled.store(false, Ordering::Relaxed);
    log::info!("RFID: paused for provisioning");

    let session = Session::new().await?;
    let adapter = session.default_adapter().await?;

    adapter.set_powered(true).await?;
    adapter.set_discoverable(true).await?;
    adapter.set_pairable(true).await?;
    adapter.set_discoverable_timeout(0).await?;

    // this is supposed to remove the bond data after disconnect
    let adapter_cleanup = adapter.clone();
    let app_state_for_monitor = app_state.clone();
    let ble_event_tx_for_monitor = ble_event_tx.clone();

    tokio::spawn(async move {
        log::info!("BLE: Bond cleanup monitor started");
        loop {
            tokio::time::sleep(std::time::Duration::from_secs(2)).await;
            if let Ok(addrs) = adapter_cleanup.device_addresses().await {
                for addr in addrs {
                    if let Ok(device) = adapter_cleanup.device(addr) {
                        let connected = device.is_connected().await.unwrap_or(false);
                        let paired = device.is_paired().await.unwrap_or(false);

                        if connected {
                            let addr_str = addr.to_string();

                            let mut should_emit_event = false;

                            {
                                let mut state = app_state_for_monitor.lock().await;

                                let should_update = match state.selected_device.as_ref() {
                                    Some(current) => current.device_id != addr_str,
                                    None => true,
                                };

                                if should_update {
                                    state.selected_device =
                                        Some(crate::shared_state::PendingDeviceSelection {
                                            device_id: addr_str.clone(),
                                            selected_at: chrono::Utc::now(),
                                        });

                                    should_emit_event = true;
                                }
                            }

                            if should_emit_event {
                                log::info!("BLE: selected device from connection: {}", addr_str);

                                if let Err(e) = ble_event_tx_for_monitor
                                    .send(BleEvent::DeviceConnected {
                                        mac_address: addr_str.clone(),
                                    })
                                    .await
                                {
                                    log::error!(
                                        "BLE: failed to send device-connected event: {}",
                                        e
                                    );
                                }
                            }

                            // If the device is still in the system as "paired" but the
                            // connection is gone, wipe it so we can start fresh next time.
                            if paired && !connected {
                                log::info!("BLE: Cleanup - removing disconnected device {}", addr);
                                let _ = adapter_cleanup.remove_device(addr).await;
                            }
                        }
                    }
                }
            }
        }
    });

    let agent = Agent {
        request_default: true,
        request_confirmation: Some(Box::new(|_req| Box::pin(async move { Ok(()) }))),
        request_authorization: Some(Box::new(|_req| Box::pin(async move { Ok(()) }))),
        authorize_service: Some(Box::new(|_req| Box::pin(async move { Ok(()) }))),

        request_pin_code: None,
        display_pin_code: None,
        request_passkey: None,
        display_passkey: None,

        _non_exhaustive: (),
    };

    let _agent_handle = session.register_agent(agent).await?;
    log::info!("BLE: Auto-pairing agent active");

    log::info!("BLE: adapter powered on");

    let service_uuid = Uuid::parse_str(PROVISIONING_SERVICE_UUID)?;
    let char_uuid = Uuid::parse_str(PROVISIONING_CHAR_UUID)?;

    let adv = Advertisement {
        service_uuids: [service_uuid].into_iter().collect(),
        discoverable: Some(true),
        local_name: Some("fog-node".to_string()),
        ..Default::default()
    };
    let _adv_handle: AdvertisementHandle = adapter.advertise(adv).await?;
    log::info!("BLE: advertising started");

    let app = Application {
        services: vec![Service {
            uuid: service_uuid,
            primary: true,
            characteristics: vec![Characteristic {
                uuid: char_uuid,
                read: Some(CharacteristicRead {
                    read: true,
                    encrypt_read: false,
                    fun: Box::new({
                        let hmac_key = data.hmac_key.clone();
                        move |_req| {
                            log::info!("BLE: edge requested HMAC key");
                            let value = match hex::decode(&hmac_key) {
                                Ok(v) => v,
                                Err(err) => {
                                    log::error!("BLE: failed to decode HMAC payload: {}", err);
                                    return Box::pin(async move {
                                        Err(bluer::gatt::local::ReqError::Failed)
                                    });
                                }
                            };
                            Box::pin(async move { Ok(value) })
                        }
                    }),
                    ..Default::default()
                }),
                notify: Some(CharacteristicNotify {
                    notify: true,
                    indicate: true,
                    method: CharacteristicNotifyMethod::Fun(Box::new({
                        let hmac_key = data.hmac_key.clone();
                        move |mut notifier| {
                            let value = match hex::decode(&hmac_key) {
                                Ok(v) => v,
                                Err(err) => {
                                    log::error!(
                                        "BLE: failed to decode HMAC payload for notify: {}",
                                        err
                                    );
                                    return async move {}.boxed();
                                }
                            };
                            async move {
                                log::info!(
                                    "BLE: notify session started (confirming={:?})",
                                    notifier.confirming()
                                );

                                if let Err(err) = notifier.notify(value).await {
                                    log::error!("BLE: notify failed: {}", err);
                                } else {
                                    log::info!("BLE: HMAC sent via notify/indicate");
                                }
                            }
                            .boxed()
                        }
                    })),
                    ..Default::default()
                }),
                ..Default::default()
            }],
            ..Default::default()
        }],
        ..Default::default()
    };

    let _app_handle = adapter.serve_gatt_application(app).await?;

    log::info!("BLE: provisioning service advertised");
    log::info!("BLE: provisioning service ready; HMAC payload prepared");
    // nya ändringar här! tanken är att det ska förhindra att enheten hänger sig vid omstart
    log::info!("BLE: server running (waiting for stop signal)");

    tokio::select! {
        _ = stop => {
            log::info!("BLE: stop signal received");
        }
        _ = tokio::signal::ctrl_c() => {
            log::info!("BLE: received SIGINT/SIGTERM, shutting down");
        }
        _ = async {
            loop {
            tokio::time::sleep(std::time::Duration::from_secs(10)).await;
            }
        } => {}
    }
    if let Ok(addrs) = adapter.device_addresses().await {
        for addr in addrs {
            let _ = adapter.remove_device(addr).await;
        }
    }

    rfid_enabled.store(true, Ordering::Relaxed);
    log::info!("RFID: resumed after provisioning");

    Ok(())
}
