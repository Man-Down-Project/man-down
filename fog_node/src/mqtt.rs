use crate::events::Envelope;
use rumqttc::{AsyncClient, Event, Key, MqttOptions, Packet, QoS, Transport};
use std::{fs, time::Duration};
use tokio::sync::mpsc::Sender;

pub async fn start_mqtt_tls(tx: Sender<Envelope>) {
    let mut mqttoptions = MqttOptions::new("fog-node", "localhost", 8883);
    mqttoptions.set_keep_alive(Duration::from_secs(5));

    let ca = fs::read("certs/ca.crt").expect("missing ca.crt");
    let client_cert = fs::read("certs/fog.crt").expect("missing fog.crt");
    let client_key = fs::read("certs/fog.key").expect("missing fog.key");

    mqttoptions.set_transport(Transport::tls(
        ca,
        Some((client_cert, Key::RSA(client_key))),
        None,
    ));

    let (client, mut eventloop) = AsyncClient::new(mqttoptions, 10);

    client
        .subscribe("md/v1/device/+/events", QoS::AtLeastOnce)
        .await
        .expect("subscribe failed");

    log::info!("MQTT TLS Connected and subscribed to md/v1/device/+/events");

    loop {
        match eventloop.poll().await {
            Ok(Event::Incoming(Packet::Publish(p))) => {
                log::info!("MQTT publish received on topic={}", p.topic);
                match serde_json::from_slice::<Envelope>(&p.payload) {
                    Ok(env) => {
                        if let Err(e) = tx.send(env).await {
                            eprintln!("Failed to forward envelope : {}", e);
                        }
                    }
                    Err(e) => log::warn!("Invalid payload received: {}", e),
                }
            }
            Ok(_) => {}
            Err(e) => {
                eprintln!("MQTT error: {:?}", e);
                break;
            }
        }
    }
}
