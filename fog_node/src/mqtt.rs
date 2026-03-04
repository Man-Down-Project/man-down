use crate::events::Envelope;
use rumqttc::{AsyncClient, Event, Key, MqttOptions, Packet, QoS, Transport};
use std::{env, time::Duration};
use tokio::sync::{mpsc::Sender, watch};

#[derive(Debug, Clone)]
pub struct MqttConfig {
    pub host: String,
    pub port: u16,
    pub client_id: String,
    pub topic: String,

    pub ca_path: String,
    pub client_cert_path: String,
    pub client_key_path: String,

    pub keep_alive_secs: u64,
    pub reconnect_delay_secs: u64,
}

impl MqttConfig {
    pub fn from_env() -> Result<Self, Box<dyn std::error::Error + Send + Sync>> {
        let host = env_var("MQTT_HOST", "127.0.0.1"); // localhost byts mot 127.0.0.1
        let port = env_var("MQTT_PORT", "8883").parse::<u16>()?;
        let client_id = env_var("MQTT_CLIENT_ID", "fog-node");
        let topic = env_var("MQTT_TOPIC", "md/v1/device/+/events");

        let ca_path = env_var("MQTT_CA_PATH", "certs/ca.crt");
        let client_cert_path = env_var("MQTT_CERT_PATH", "certs/fog.crt");
        let client_key_path = env_var("MQTT_KEY_PATH", "certs/fog_rsa.key");

        let keep_alive_secs = env_var("MQTT_KEEP_ALIVE_SECS", "5").parse()?;
        let reconnect_delay_secs = env_var("MQTT_RECONNECT_DELAY_SECS", "3").parse()?;

        Ok(Self {
            host,
            port,
            client_id,
            topic,
            ca_path,
            client_cert_path,
            client_key_path,
            keep_alive_secs,
            reconnect_delay_secs,
        })
    }
}

fn env_var(key: &str, default: &str) -> String {
    env::var(key).unwrap_or_else(|_| default.to_string())
}

pub async fn start_mqtt_tls(
    tx: Sender<Envelope>,
    mut shutdown_rx: watch::Receiver<bool>,
) -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
    let cfg = MqttConfig::from_env()?;
    log::info!(
        "MQTT TLS config: host={} port={} ca={} cert={} key={}",
        cfg.host,
        cfg.port,
        cfg.ca_path,
        cfg.client_cert_path,
        cfg.client_key_path
    );

    loop {
        if *shutdown_rx.borrow() {
            log::info!("MQTT: shutdown requested before conect");
            return Ok(());
        }

        log::info!(
            "MQTT: connecting host={} port={} client_id={} topic={}",
            cfg.host,
            cfg.port,
            cfg.client_id,
            cfg.topic
        );

        match run_once(&cfg, &tx, &mut shutdown_rx).await {
            Ok(()) => {
                log::info!("MQTT: stopped cleanly");
                return Ok(());
            }
            Err(e) => {
                if *shutdown_rx.borrow() {
                    return Ok(());
                }
                log::error!("MQTT: connecting loop error: {}", e);
                log::info!("MQTT: reconnecting is in {}s...", cfg.reconnect_delay_secs);
                tokio::time::sleep(Duration::from_secs(cfg.reconnect_delay_secs)).await;
            }
        }
    }
}
async fn run_once(
    cfg: &MqttConfig,
    tx: &Sender<Envelope>,
    shutdown_rx: &mut watch::Receiver<bool>,
) -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
    let ca = std::fs::read(&cfg.ca_path)
        .map_err(|e| format!("Could not read CA '{}': {}", cfg.ca_path, e))?;
    let client_cert = std::fs::read(&cfg.client_cert_path)
        .map_err(|e| format!("Could not read cert '{}': {}", cfg.client_cert_path, e))?;
    let client_key = std::fs::read(&cfg.client_key_path)
        .map_err(|e| format!("Could not read key '{}': {}", cfg.client_key_path, e))?;

    let mut mqttoptions = MqttOptions::new(&cfg.client_id, &cfg.host, cfg.port);
    mqttoptions.set_keep_alive(Duration::from_secs(cfg.keep_alive_secs));
    //mqttoptions.set_transport(Transport::tls(
    //    ca,
    //    Some((client_cert, Key::RSA(client_key))),
    //    None,
    //));

    let (client, mut eventloop) = AsyncClient::new(mqttoptions, 10);

    client.subscribe(&cfg.topic, QoS::AtLeastOnce).await?;
    log::info!("MQTT: connected + subscribed");

    loop {
        tokio::select! {
            _= shutdown_rx.changed() => {
                if *shutdown_rx.borrow() {
                    return Ok(());
                }
            }

            ev = eventloop.poll() => {
                let ev = ev?;
                if let Event::Incoming(Packet::Publish(p)) = ev {
                    match serde_json::from_slice::<Envelope>(&p.payload) {
                        Ok(env) => {
                            tx.send(env).await?;
                        }
                        Err(e) => {
                            log::warn!("MQTT: invalid payload on topic={} err={}", p.topic, e);
                        }
                    }
                }
            }
        }
    }
}
