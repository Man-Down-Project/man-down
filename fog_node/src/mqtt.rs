use crate::events::Envelope;
use rumqttc::{AsyncClient, Event, Key, MqttOptions, Packet, QoS, Transport};
use std::{fs, time::Duration};
use tokio::sync::mpsc::Sender;

pub async fn start_mqtt_tls(tx: Sender<Envelope>) -> Result<(), Box<dyn std::error::Error>> {
    let mut mqttoptions = MqttOptions::new("fog-node", "localhost", 8883);
    mqttoptions.set_keep_alive(Duration::from_secs(5));

    let ca = fs::read("certs/ca.crt")?;
    let client_cert = fs::read("certs/fog.crt")?;
    let client_key = fs::read("certs/fog.key")?;

    mqttoptions.set_transport(Transport::tls(
        ca,
        Some((client_cert, Key::RSA(client_key))),
        None,
    ));

    let (client, mut eventloop) = AsyncClient::new(mqttoptions, 10);

    client
        .subscribe("md/v1/device/+/events", QoS::AtLeastOnce)
        .await?;

    log::info!("MQTT TLS Connected and subscribed to md/v1/device/+/events");

    loop {
        let ev = eventloop.poll().await?;

        if let Event::Incoming(Packet::Publish(p)) = ev {
            log::info!("MQTT publish received on topic={}", p.topic);

            match serde_json::from_slice::<Envelope>(&p.payload) {
                Ok(env) => {
                    tx.send(env).await?;
                }

                Err(e) => {
                    log::warn!("Invalid payload received: {}", e);
                }
            }
        }
    }
}
