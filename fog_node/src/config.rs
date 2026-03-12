use std::env;

#[derive(Debug, Clone)]
pub struct MqttConfig {
    pub host: String,
    pub port: u16,
    pub client_id: String,
    pub topic: String,

    pub use_tls: bool,
    pub mesh_node_id: String,

    pub ca_path: String,
    pub client_cert_path: String,
    pub client_key_path: String,

    pub keep_alive_secs: u64,
    pub reconnect_delay_secs: u64,
}

impl MqttConfig {
    pub fn from_env() -> Result<Self, Box<dyn std::error::Error + Send + Sync>> {
        let host = env_var("MQTT_HOST", "localhost");
        let port = env_var("MQTT_PORT", "8883").parse::<u16>()?;
        let client_id = env_var("MQTT_CLIENT_ID", "fog-node");
        let topic = env_var("MQTT_TOPIC", "md/v1/device/+/events");

        let use_tls = env_var("MQTT_USE_TLS", "true").parse::<bool>()?;
        let mesh_node_id = env_var("MESH_NODE_ID", "mesh-unknown");

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
            use_tls,
            mesh_node_id,
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
