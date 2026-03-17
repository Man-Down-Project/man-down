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

        let conn = Connection::open(path)?;

        apply_sqlcipher_key(&conn, key)?;
        verify_database_access(&conn)?;
        configure_connection(&conn)?;
        initialize_schema(&conn)?;

        Ok(Self { conn })
    }

    // MQTT QoS1 kan leverera samma event flera gånger.
    // UNIQUE(device_id, seq) + INSERT OR IGNORE förhindrar dubletter.
    pub fn insert_event(&self, env: &Envelope) -> Result<()> {
        let incident_json =
            serde_json::to_string(&env.incident).map_err(|_| rusqlite::Error::InvalidQuery)?;

        let event_type = event_type(&env.incident);

        self.conn.execute(
            "INSERT OR IGNORE INTO events (
                device_id,
                mesh_node_id,
                seq,
                sent_at,
                event_type,
                incident
            )
            VALUES (?1, ?2, ?3, ?4, ?5, ?6)",
            params![
                &env.device_id,
                &env.mesh_node_id,
                env.seq,
                env.sent_at.to_rfc3339(),
                event_type,
                incident_json,
            ],
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
    conn.pragma_update(None, "journal_mode", "WAL")?;
    conn.pragma_update(None, "synchronous", "NORMAL")?;
    conn.pragma_update(None, "kdf_iter", 256000)?;
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
        sent_at TEXT NOT NULL,
        event_type TEXT NOT NULL,
        incident TEXT NOT NULL,
        UNIQUE(device_id, seq)
        );
            
        CREATE INDEX IF NOT EXISTS idx_device_time
        ON events(device_id, sent_at);
        "#,
    )?;

    Ok(())
}

fn ensure_parent_dir(path: &str) -> Result<()> {
    if let Some(parent) = Path::new(path).parent() {
        if !parent.as_os_str().is_empty() {
            std::fs::create_dir_all(parent)
                .map_err(|_| rusqlite::Error::InvalidPath(parent.to_path_buf()))?;
        }
    }

    Ok(())
}

fn event_type(incident: &Incident) -> &'static str {
    match incident {
        Incident::ManDown { .. } => "man_down",
        Incident::MeshDisconnect { .. } => "mesh_disconnect",
        Incident::Login { .. } => "login",
        Incident::Logout { .. } => "logout",
        Incident::BatteryLow { .. } => "battery_low",
        Incident::SensorFault { .. } => "sensor_fault",
    }
}
