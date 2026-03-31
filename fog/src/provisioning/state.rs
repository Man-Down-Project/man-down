use crate::provisioning::models::HmacState;
use std::fs;
use std::path::Path;

const STATE_FILE: &str = "state/hmac.json";

pub fn load_hmac_state() -> Option<HmacState> {
    let path = Path::new(STATE_FILE);

    if !path.exists() {
        return None;
    }

    let data = fs::read_to_string(path).ok()?;
    serde_json::from_str(&data).ok()
}

pub fn save_hmac_state(state: &HmacState) -> std::io::Result<()> {
    fs::create_dir_all("state")?;
    let json = serde_json::to_string_pretty(state)?;
    fs::write(STATE_FILE, json)
}
