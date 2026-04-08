// Models reserved for future real RFID integration (hardware input)

#[derive(Debug, Clone)]
pub enum RfidAction {
    Login,
    Logout,
}

#[derive(Debug, Clone)]
pub struct RfidScan {
    pub worker_id: String,
    pub action: RfidAction,
}
