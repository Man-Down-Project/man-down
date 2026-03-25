use crate::mqtt::MqttConfig;
use std::env;

#[derive(Clone)]
pub struct AppConfig {
    pub mqtt: MqttConfig,
    pub db_path: String,
    pub db_key: String,
}

impl AppConfig {
    pub fn from_env() -> Result<Self, Box<dyn std::error::Error + Send + Sync>> {
        Ok(Self {
            mqtt: MqttConfig::from_env()?,
            db_path: env_or_default("DB_PATH", "data/fog.db"),
            db_key: env::var("DB_KEY").map_err(|_| {
                std::io::Error::new(
                    std::io::ErrorKind::NotFound,
                    "DB_KEY must be set in environment",
                )
            })?,
        })
    }
}

fn env_or_default(key: &str, default: &str) -> String {
    env::var(key).unwrap_or_else(|_| default.to_string())
}
