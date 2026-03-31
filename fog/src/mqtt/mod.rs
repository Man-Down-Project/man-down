pub mod client;
pub mod config;

pub use client::start_mqtt;
pub use config::MqttConfig;

#[derive(Debug, Clone)]
pub struct OutgoingMessage {
    pub topic: String,
    pub payload: String,
}
