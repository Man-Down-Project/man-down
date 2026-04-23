use crate::events::{EdgeEvent, Envelope, SignedEdgeEvent, verify_hmac};
use crate::mqtt::{MqttConfig, OutgoingMessage};
use crate::provisioning::state::load_hmac_state;
use chrono::Utc;
use rumqttc::{AsyncClient, Event, MqttOptions, Packet, QoS, Transport};
use std::fs::OpenOptions;
use std::io::Write;
use std::{error::Error, fs::File, io::BufReader, path::Path};
use tokio::sync::{mpsc::Sender, watch};
use tokio_rustls::rustls::{Certificate, ClientConfig, PrivateKey, RootCertStore};

type DynError = Box<dyn Error + Send + Sync>;

pub async fn start_mqtt(
    cfg: MqttConfig,
    tx: Sender<Envelope>,
    mut shutdown_rx: watch::Receiver<bool>,
    mut outgoing_rx: tokio::sync::mpsc::Receiver<OutgoingMessage>,
) -> Result<(), DynError> {
    std::fs::create_dir_all("logs").ok();

    log::info!(
        "MQTT config: host={} port={} client_id={} topics={:?} tls={}",
        cfg.host,
        cfg.port,
        cfg.client_id,
        cfg.subscribe_topics,
        cfg.use_tls
    );

    loop {
        if is_shutdown_requested(&shutdown_rx) {
            log::info!("MQTT: shutdown requested before connect");
            return Ok(());
        }

        log::info!(
            "MQTT: connecting to {}:{} as client_id={} topics={:?}",
            cfg.host,
            cfg.port,
            cfg.client_id,
            cfg.subscribe_topics,
        );

        match run_session(&cfg, &tx, &mut shutdown_rx, &mut outgoing_rx).await {
            Ok(()) => {
                log::info!("MQTT: session stopped cleanly");
                return Ok(());
            }
            Err(e) => {
                if is_shutdown_requested(&shutdown_rx) {
                    log::info!("MQTT: shutdown requested, stopping reconnect loop");
                    return Ok(());
                }
                log::error!("MQTT: session error: {}", e);
                log::info!("MQTT: reconnecting in {:?}", cfg.reconnect_delay);

                tokio::time::sleep(cfg.reconnect_delay).await;
            }
        }
    }
}

#[allow(clippy::collapsible_if)]
async fn run_session(
    cfg: &MqttConfig,
    tx: &Sender<Envelope>,
    shutdown_rx: &mut watch::Receiver<bool>,
    outgoing_rx: &mut tokio::sync::mpsc::Receiver<OutgoingMessage>,
) -> Result<(), DynError> {
    let mqtt_options = build_mqtt_options(cfg)?;
    let (client, mut eventloop) = AsyncClient::new(mqtt_options, 10);

    let mut subscribed = false;

    loop {
        tokio::select! {
            _ = shutdown_rx.changed() => {
                if is_shutdown_requested(shutdown_rx) {
                    log::info!("MQTT: shutdown signal received");
                    return Ok(());
                }
            }

            msg = outgoing_rx.recv() => {
                if let Some(msg) = msg{
                    println!("PUBLISHING TO MQTT: {}", msg.topic);
                    client.publish(msg.topic, QoS::AtLeastOnce, true, msg.payload).await?;
                }
            }

            event = eventloop.poll() => {
                let event = event?;
                if let Event::Incoming(Packet::ConnAck(_)) = &event {
                    if !subscribed {
                        for topic in &cfg.subscribe_topics {
                            client.subscribe(topic, QoS::AtLeastOnce).await?;
                            log::info!("MQTT: subscribed to {}", topic);
                        }
                        subscribed = true;
                    }
                }
                handle_event(event, tx, &cfg.mesh_node_id).await?;
            }
        }
    }
}

fn build_mqtt_options(cfg: &MqttConfig) -> Result<MqttOptions, DynError> {
    let mut options = MqttOptions::new(&cfg.client_id, &cfg.host, cfg.port);
    options.set_keep_alive(cfg.keep_alive);

    options.set_credentials(cfg.username.clone(), cfg.password.clone());

    if cfg.use_tls {
        let tls_config =
            build_tls_config(&cfg.ca_path, &cfg.client_cert_path, &cfg.client_key_path)?;
        options.set_transport(Transport::tls_with_config(tls_config.into()));
    }

    Ok(options)
}

fn build_tls_config(
    ca_path: &Path,
    client_cert_path: &Path,
    client_key_path: &Path,
) -> Result<ClientConfig, DynError> {
    let roots = load_ca(ca_path)?;
    let client_chain = load_client_chain(client_cert_path)?;
    let client_key = load_private_key(client_key_path)?;

    let config = ClientConfig::builder()
        .with_safe_defaults()
        .with_root_certificates(roots)
        .with_client_auth_cert(client_chain, client_key)?;

    Ok(config)
}

async fn handle_event(
    event: Event,
    tx: &Sender<Envelope>,
    default_mesh_node_id: &str,
) -> Result<(), DynError> {
    match event {
        Event::Incoming(Packet::Publish(publish)) => {
            log::debug!(
                "MQTT: publish received topic={} payload_len={}",
                publish.topic,
                publish.payload.len()
            );

            const EDGE_EVENT_LEN: usize = EdgeEvent::LEN;
            const HMAC_LEN: usize = 8;
            const SIGNED_LEN: usize = EDGE_EVENT_LEN + HMAC_LEN;

            if publish.payload.len() == SIGNED_LEN {
                let event_bytes = &publish.payload[..EDGE_EVENT_LEN];
                let hmac_bytes = &publish.payload[EDGE_EVENT_LEN..SIGNED_LEN];

                let edge = match EdgeEvent::from_bytes(event_bytes) {
                    Some(e) => e,
                    None => {
                        log::warn!("MQTT: failed to parse signed edge event");
                        return Ok(());
                    }
                };
                let hmac_state = match load_hmac_state() {
                    Some(h) => h,
                    None => {
                        log::warn!("MQTT: no local HMAC state found, dropping packet");
                        return Ok(());
                    }
                };

                let key = match hex::decode(&hmac_state.key_hex) {
                    Ok(k) => k,
                    Err(e) => {
                        log::warn!("MQTT: invalid local HMAC key hex: {}", e);
                        return Ok(());
                    }
                };

                let signed = SignedEdgeEvent {
                    event: edge,
                    hmac_hex: hex::encode_upper(hmac_bytes),
                };

                log::info!(
                    "DEBUG event bytes: {}",
                    hex::encode_upper(edge.into_bytes())
                );
                log::info!("DEBUG incoming hmac: {}", hex::encode_upper(hmac_bytes));

                if let Err(e) = verify_hmac(&signed, &key) {
                    log::warn!("MQTT: HMAC verification failed, dropping packet: {}", e);
                    return Ok(());
                }

                let received_at = Utc::now();

                if edge.is_heartbeat() {
                    let line = edge.heartbeat_log_line(default_mesh_node_id, received_at);
                    if let Err(e) = write_heartbeat_log(&line) {
                        log::error!("failed to write heartbeat log: {}", e);
                    }
                    return Ok(());
                }

                let env = edge.to_envelope(default_mesh_node_id.to_string());
                tx.send(env)
                    .await
                    .map_err(|e| format!("processor channel closed: {}", e))?;

                return Ok(());
            }

            match decode_envelope(&publish.payload, default_mesh_node_id) {
                Ok(env) => {
                    tx.send(env)
                        .await
                        .map_err(|e| format!("processor channel closed: {}", e))?;
                }
                Err(err) => {
                    log::warn!(
                        "MQTT: failed to decode payload on topic={} err={}",
                        publish.topic,
                        err
                    );
                }
            }
        }

        Event::Incoming(packet) => {
            log::debug!("MQTT incoming packet: {:?}", packet);
        }

        Event::Outgoing(packet) => {
            log::debug!("MQTT outgoing packet: {:?}", packet);
        }
    }

    Ok(())
}

fn decode_envelope(payload: &[u8], default_mesh_node: &str) -> Result<Envelope, DynError> {
    if let Some(edge) = EdgeEvent::from_bytes(payload) {
        return Ok(edge.to_envelope(default_mesh_node.to_string()));
    }

    let env: Envelope = serde_json::from_slice(payload)?;
    Ok(env)
}

fn is_shutdown_requested(shutdown_rx: &watch::Receiver<bool>) -> bool {
    *shutdown_rx.borrow()
}

fn load_ca(path: &Path) -> Result<RootCertStore, DynError> {
    let file = File::open(path)?;
    let mut reader = BufReader::new(file);

    let certs = rustls_pemfile::certs(&mut reader)?;
    if certs.is_empty() {
        return Err(format!("No CA certificates found in {}", path.display()).into());
    }

    let mut roots = RootCertStore::empty();
    for cert in certs {
        roots.add(&Certificate(cert))?;
    }

    Ok(roots)
}

fn load_client_chain(path: &Path) -> Result<Vec<Certificate>, DynError> {
    let file = File::open(path)?;
    let mut reader = BufReader::new(file);

    let certs = rustls_pemfile::certs(&mut reader)?;
    if certs.is_empty() {
        return Err(format!("No client certs found in {}", path.display()).into());
    }

    Ok(certs.into_iter().map(Certificate).collect())
}

fn load_private_key(path: &Path) -> Result<PrivateKey, DynError> {
    let file = File::open(path)?;
    let mut reader = BufReader::new(file);
    let mut keys = rustls_pemfile::rsa_private_keys(&mut reader)?;

    if let Some(key) = keys.pop() {
        return Ok(PrivateKey(key));
    }

    let file = File::open(path)?;
    let mut reader = BufReader::new(file);

    let mut keys = rustls_pemfile::pkcs8_private_keys(&mut reader)?;

    if let Some(key) = keys.pop() {
        return Ok(PrivateKey(key));
    }

    Err(format!("No private key found in {}", path.display()).into())
}

fn write_heartbeat_log(line: &str) -> Result<(), std::io::Error> {
    let mut file = OpenOptions::new()
        .create(true)
        .append(true)
        .open("logs/heartbeat.log")?;

    writeln!(file, "{}", line)?;
    Ok(())
}
