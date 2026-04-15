pub mod config;
pub mod server;

#[derive(Debug, Clone)]
pub enum BleEvent {
    DeviceConnected { mac_address: String },
}
