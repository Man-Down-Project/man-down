use crate::events::{Envelope, Incident};
use rusqlite::{Connection, Result, params};

pub struct Storage {
    conn: Connection,
}

impl Storage {
    pub fn new(path: &str) -> Result<Self> {
        let conn = Connection::open(path)?;

        conn.execute(
            "CREATE TABLE IF NOT EXISTS events (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            device_id TEXT NOT NULL,
            mesh_node_id TEXT NOT NULL,
            seq INTEGER NOT NULL,
            sent_at TEXT NOT NULL,
            event_type TEXT NOT NULL,
            incident TEXT NOT NULL,
            UNIQUE(device_id, seq)
            )",
            [],
        )?;

        conn.execute(
            "CREATE INDEX IF NOT EXISTS idx_device_time
            ON events(device_id, sent_at)",
            [],
        )?;

        Ok(Self { conn })
    }

    pub fn insert_event(&self, env: &Envelope) -> Result<()> {
        let incident_json =
            serde_json::to_string(&env.incident).map_err(|_| rusqlite::Error::InvalidQuery)?;

        let event_type = match &env.incident {
            Incident::ManDown { .. } => "man_down",
            Incident::MeshDisconnect { .. } => "mesh_disconnect",
            Incident::Login { .. } => "login",
            Incident::Logout { .. } => "logout",
            Incident::BatteryLow { .. } => "battery_low",
            Incident::SensorFault { .. } => "sensor_fault",
        };

        self.conn.execute(
            // MQTT QoS1 kan leverera samma event flera gånger.
            // UNIQUE(device_id, seq) + INSERT OR IGNORE förhindrar dubletter i databasen.
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
