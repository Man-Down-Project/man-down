use crate::events::{EdgeEvent, Envelope};
use rumqttc::{AsyncClient, Event, MqttOptions, Packet, QoS, Transport};
use std::{env, io::BufReader, time::Duration};
use tokio::sync::{mpsc::Sender, watch};
use tokio_rustls::rustls::{Certificate, ClientConfig, PrivateKey, RootCertStore};

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
        let host = env_var("MQTT_HOST", "localhost"); //alt. localhost
        let port = env_var("MQTT_PORT", "8883").parse::<u16>()?;
        let client_id = env_var("MQTT_CLIENT_ID", "fog-node");
        let topic = env_var("MQTT_TOPIC", "md/v1/device/+/events");

        let ca_path = env_var("MQTT_CA_PATH", "certs/ca.crt");
        let client_cert_path = env_var("MQTT_CERT_PATH", "certs/fog.crt");
        let client_key_path = env_var("MQTT_KEY_PATH", "certs/fog_pkcs1_rsa.key");

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

fn load_ca(path: &str) -> Result<RootCertStore, Box<dyn std::error::Error + Send + Sync>> {
    let file = std::fs::File::open(path)?;
    let mut reader = BufReader::new(file);

    let certs = rustls_pemfile::certs(&mut reader)?;

    if certs.is_empty() {
        return Err(format!("No CA certs found in {}", path).into());
    }

    let mut roots = RootCertStore::empty();
    for cert in certs {
        roots.add(&Certificate(cert))?;
    }

    Ok(roots)
}

fn load_client_chain(
    path: &str,
) -> Result<Vec<Certificate>, Box<dyn std::error::Error + Send + Sync>> {
    let file = std::fs::File::open(path)?;
    let mut reader = BufReader::new(file);

    let certs = rustls_pemfile::certs(&mut reader)?;

    if certs.is_empty() {
        return Err(format!("No client certs found in {}", path).into());
    }

    Ok(certs.into_iter().map(Certificate).collect())
}

fn load_key(path: &str) -> Result<PrivateKey, Box<dyn std::error::Error + Send + Sync>> {
    let file = std::fs::File::open(path)?;
    let mut reader = BufReader::new(file);

    let mut keys = rustls_pemfile::rsa_private_keys(&mut reader)?;

    if let Some(key) = keys.pop() {
        return Ok(PrivateKey(key));
    }

    let file = std::fs::File::open(path)?;
    let mut reader = BufReader::new(file);

    let mut keys = rustls_pemfile::pkcs8_private_keys(&mut reader)?;

    if let Some(key) = keys.pop() {
        return Ok(PrivateKey(key));
    }

    Err(format!("No private key found in {}", path).into())
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
    let mut mqttoptions = MqttOptions::new(&cfg.client_id, &cfg.host, cfg.port);
    mqttoptions.set_keep_alive(Duration::from_secs(cfg.keep_alive_secs));

    if cfg.port == 8883 {
        log::info!("MQTT: enabling TLS");

        let roots = load_ca(&cfg.ca_path)?;
        let client_chain = load_client_chain(&cfg.client_cert_path)?;
        let client_key = load_key(&cfg.client_key_path)?;

        let tls_config = ClientConfig::builder()
            .with_safe_defaults()
            .with_root_certificates(roots)
            .with_client_auth_cert(client_chain, client_key)?;

        mqttoptions.set_transport(Transport::tls_with_config(tls_config.into()));
    }

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

                    log::info!(
                        "MQTT: received topic={} len={} bytes={:02x?}",
                        p.topic,
                        p.payload.len(),
                        &p.payload
                    );

                    if let Some(edge) = EdgeEvent::from_bytes(&p.payload){
                        let env = edge.to_envelope("mesh-unknown".to_string());
                        tx.send(env).await?;
                        continue;
                    }
                    else {
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
}
