use crate::events::{Envelope, Incident};
use rusqlite::{Connection, Result, params};
use std::path::Path;
use std::time::Duration;

pub struct Storage {
    conn: Connection,
}

impl Storage {
    pub fn new(path: &str, key: &str) -> Result<Self> {
        ensure_parent_dir(path)?;

        log::info!("storage: opening db at {}", path);
        let conn = Connection::open(path)?;

        apply_sqlcipher_key(&conn, key)?;
        verify_database_access(&conn)?;
        configure_connection(&conn)?;
        initialize_schema(&conn)?;

        log::info!("storage: initialized schema");

        Ok(Self { conn })
    }

    // MQTT QoS1 kan leverera samma event flera gånger.
    // UNIQUE(device_id, seq) + INSERT OR IGNORE förhindrar dubletter.
    pub fn insert_event(&self, env: &Envelope) -> Result<()> {
        let incident_json =
            serde_json::to_string(&env.incident).map_err(|_| rusqlite::Error::InvalidQuery)?;

        let event_type = event_type(&env.incident);
        let device_id = mac_to_string(&env.device_id);

        self.conn.execute(
            "INSERT OR IGNORE INTO events (
            device_id,
            mesh_node_id,
            seq,
            mesh_timestamp,
            received_at,
            event_type,
            incident
        )
        VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7)",
            params![
                device_id,
                &env.mesh_node_id,
                env.seq,
                env.mesh_timestamp,
                env.received_at.to_rfc3339(),
                event_type,
                incident_json,
            ],
        )?;

        Ok(())
    }

    pub fn insert_auth_event(&self, env: &Envelope) -> Result<()> {
        let (worker_id, action) = match &env.incident {
            Incident::Login { worker_id } => (worker_id, "login"),
            Incident::Logout { worker_id } => (worker_id, "logout"),
            _ => return Ok(()),
        };
        let device_id = mac_to_string(&env.device_id);

        self.conn.execute(
            "INSERT OR IGNORE INTO auth_events (
            device_id,
            worker_id,
            action,
            mesh_node_id,
            seq,
            mesh_timestamp,
            received_at
        )
        VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7)",
            params![
                device_id,
                worker_id,
                action,
                &env.mesh_node_id,
                env.seq,
                env.mesh_timestamp,
                env.received_at.to_rfc3339(),
            ],
        )?;

        Ok(())
    }

    pub fn edge_tag_exists(&self, rfid_tag: &str) -> Result<bool> {
        let mut stmt = self.conn.prepare(
            "SELECT 1
         FROM device_whitelist
         WHERE rfid_tag = ?1
         LIMIT 1",
        )?;

        let mut rows = stmt.query(params![rfid_tag])?;
        Ok(rows.next()?.is_some())
    }

    pub fn is_worker_allowed(&self, worker_id: &str) -> Result<bool> {
        let mut stmt = self.conn.prepare(
            "SELECT 1
         FROM worker_whitelist
         WHERE worker_id = ?1 AND active = 1
         LIMIT 1",
        )?;

        let mut rows = stmt.query(params![worker_id])?;
        Ok(rows.next()?.is_some())
    }

    pub fn add_edge_tag_to_whitelist(&self, rfid_tag: &str) -> Result<()> {
        self.conn.execute(
            "INSERT OR IGNORE INTO device_whitelist (rfid_tag, mac, active)
         VALUES (?1, NULL, 0)",
            params![rfid_tag],
        )?;

        Ok(())
    }

    pub fn bind_mac_to_edge_tag(&self, rfid_tag: &str, mac_address: &str) -> Result<()> {
        self.conn.execute(
            "UPDATE device_whitelist
         SET mac = ?1,
             active = 1
         WHERE rfid_tag = ?2",
            params![mac_address, rfid_tag],
        )?;

        Ok(())
    }

    pub fn add_worker_to_whitelist(&self, worker_id: &str) -> Result<()> {
        self.conn.execute(
            "INSERT OR IGNORE INTO worker_whitelist (worker_id, active)
         VALUES (?1, 1)",
            params![worker_id],
        )?;
        Ok(())
    }
}

fn apply_sqlcipher_key(conn: &Connection, key: &str) -> Result<()> {
    conn.pragma_update(None, "key", key)?;
    Ok(())
}

fn verify_database_access(conn: &Connection) -> Result<()> {
    conn.query_row("SELECT count(*) FROM sqlite_master", [], |_row| Ok(()))?;
    Ok(())
}

fn configure_connection(conn: &Connection) -> Result<()> {
    conn.pragma_update(None, "foreign_keys", "ON")?;
    conn.busy_timeout(Duration::from_secs(5))?;
    Ok(())
}

fn initialize_schema(conn: &Connection) -> Result<()> {
    conn.execute_batch(
        r#"
            CREATE TABLE IF NOT EXISTS events (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            device_id TEXT NOT NULL,
            mesh_node_id TEXT NOT NULL,
            seq INTEGER NOT NULL,
            mesh_timestamp INTEGER NOT NULL,
            received_at TEXT NOT NULL,
            event_type TEXT NOT NULL,
            incident TEXT NOT NULL,
            UNIQUE(device_id, seq)
        );

        CREATE INDEX IF NOT EXISTS idx_device_time
            ON events(device_id, received_at);

        CREATE TABLE IF NOT EXISTS auth_events (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            device_id TEXT NOT NULL,
            worker_id TEXT NOT NULL,
            action TEXT NOT NULL,
            mesh_node_id TEXT NOT NULL,
            seq INTEGER NOT NULL,
            mesh_timestamp INTEGER NOT NULL,
            received_at TEXT NOT NULL,
            UNIQUE(device_id, seq, action)
        );

        CREATE INDEX IF NOT EXISTS idx_auth_worker_time
            ON auth_events(worker_id, received_at);

        CREATE TABLE IF NOT EXISTS device_whitelist (
            rfid_tag TEXT PRIMARY KEY,
            mac TEXT,
            active INTEGER NOT NULL DEFAULT 0
        );

        CREATE TABLE IF NOT EXISTS worker_whitelist (
            worker_id TEXT PRIMARY KEY,
            active INTEGER NOT NULL DEFAULT 1
        );
        "#,
    )?;

    Ok(())
}

fn ensure_parent_dir(path: &str) -> Result<()> {
    if let Some(parent) = Path::new(path)
        .parent()
        .filter(|p| !p.as_os_str().is_empty())
    {
        std::fs::create_dir_all(parent).map_err(|e| {
            log::error!("failed to create db parent dir {}: {}", parent.display(), e);
            rusqlite::Error::InvalidPath(parent.to_path_buf())
        })?;
    }

    Ok(())
}

fn event_type(incident: &Incident) -> &'static str {
    match incident {
        Incident::ManDown { .. } => "man_down",
        Incident::Gas => "gas",
        Incident::MeshDisconnect { .. } => "mesh_disconnect",
        Incident::Login { .. } => "login",
        Incident::Logout { .. } => "logout",
        Incident::BatteryLow { .. } => "battery_low",
        Incident::SensorFault { .. } => "sensor_fault",
    }
}

fn mac_to_string(mac: &[u8]) -> String {
    if mac.len() != 6 {
        return "INVALID_MAC".to_string();
    }
    
    format!(
        "{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}",
        mac[0], mac[1], mac[2],
        mac[3], mac[4], mac[5]
    )
}