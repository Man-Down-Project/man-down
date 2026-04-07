use bluer::Session;
use bluer::adv::{Advertisement, AdvertisementHandle};
use bluer::gatt::local::{Application, Characteristic, CharacteristicRead, Service};
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
) -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
    log::info!("BLE: starting provisioning server");

    let session = Session::new().await?;
    let adapter = session.default_adapter().await?;

    log::info!("BLE: using adapter {}", adapter.name());

    adapter.set_powered(true).await?;
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
                    fun: Box::new({
                        let hmac_key = data.hmac_key.clone();
                        move |_req| {
                            log::info!("BLE: edge requested HMAC key");
                            let value = hex::decode(&hmac_key).unwrap();
                            Box::pin(async move { Ok(value) })
                        }
                    }),
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
    log::info!("BLE: would expose HMAC key to edge: {}", data.hmac_key);

    // Prevent function from returning so BLE GATT service stays active
    futures::future::pending::<()>().await;
    #[allow(unreachable_code)]
    Ok(())
}
