use std::env;
use std::path::PathBuf;
use std::time::Duration;

#[derive(Clone)]
pub struct MqttConfig {
    pub host: String,
    pub port: u16,
    pub client_id: String,
    pub subscribe_topic: String,

    pub use_tls: bool,
    pub mesh_node_id: String,

    pub ca_path: PathBuf,
    pub client_cert_path: PathBuf,
    pub client_key_path: PathBuf,

    pub keep_alive: Duration,
    pub reconnect_delay: Duration,
}

impl MqttConfig {
    pub fn from_env() -> Result<Self, Box<dyn std::error::Error + Send + Sync>> {
        let host = env_or_default("MQTT_HOST", "localhost");
        let port = env_or_default("MQTT_PORT", "8883").parse::<u16>()?;
        let client_id = env_or_default("MQTT_CLIENT_ID", "fog-node");
        let subscribe_topic = env_or_default("MQTT_TOPIC", "md/v1/device/+/events");

        let use_tls = env_or_default("MQTT_USE_TLS", "true").parse::<bool>()?;
        let mesh_node_id = env_or_default("MESH_NODE_ID", "mesh-unknown");

        let ca_path = PathBuf::from(env_or_default("MQTT_CA_PATH", "certs/ca.crt"));
        let client_cert_path = PathBuf::from(env_or_default("MQTT_CERT_PATH", "certs/fog.crt"));
        let client_key_path =
            PathBuf::from(env_or_default("MQTT_KEY_PATH", "certs/fog_pkcs1_rsa.key"));

        let keep_alive =
            Duration::from_secs(env_or_default("MQTT_KEEP_ALIVE_SECS", "5").parse::<u64>()?);
        let reconnect_delay =
            Duration::from_secs(env_or_default("MQTT_RECONNECT_DELAY_SECS", "3").parse::<u64>()?);

        Ok(Self {
            host,
            port,
            client_id,
            subscribe_topic,
            use_tls,
            mesh_node_id,
            ca_path,
            client_cert_path,
            client_key_path,
            keep_alive,
            reconnect_delay,
        })
    }
}

fn env_or_default(key: &str, default: &str) -> String {
    env::var(key).unwrap_or_else(|_| default.to_string())
}
