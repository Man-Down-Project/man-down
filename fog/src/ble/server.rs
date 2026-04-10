use std::sync::Arc;
use std::sync::atomic::AtomicBool;
use std::sync::{
    Arc,
    atomic::{AtomicBool, Ordering},
};

use bluer::Session;
use bluer::adv::{Advertisement, AdvertisementHandle};
use bluer::agent::Agent;
use bluer::gatt::local::{
    Application, Characteristic, CharacteristicNotify, CharacteristicNotifyMethod,
    CharacteristicRead, Service,
};
use futures::FutureExt;
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
                    encrypt_read: true,
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
                // notify: Some(CharacteristicNotify {
                //     notify: true,
                //     indicate: true,
                //     method: CharacteristicNotifyMethod::Fun(Box::new({
                //         let hmac_key = data.hmac_key.clone();
                //         move |mut notifier| {
                //             let value = match hex::decode(&hmac_key) {
                //                 Ok(v) => v,
                //                 Err(err) => {
                //                     log::error!(
                //                         "BLE: failed to decode HMAC payload for notify: {}",
                //                         err
                //                     );
                //                     return async move {}.boxed();
                //                 }
                //             };
                //             async move {
                //                 log::info!(
                //                     "BLE: notify session started (confirming={:?})",
                //                     notifier.confirming()
                //                 );

                //                 if let Err(err) = notifier.notify(value).await {
                //                     log::error!("BLE: notify failed: {}", err);
                //                 } else {
                //                     log::info!("BLE: HMAC sent via notify/indicate");
                //                 }
                //             }
                //             .boxed()
                //         }
                //     })),
                //     ..Default::default()
                // }),
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
            tokio::time::sleep(std::time::Duration::from_secs(60)).await;
        }
    } => {}
}
rfid_enabled.store(true, Ordering::Relaxed);
log::info!("RFID: resumed after provisioning");

Ok(())
    // let _ = stop.await;
    // log::info!("BLE: stopping provisioning server");

    // #[allow(unreachable_code)]
    // Ok(())
}
