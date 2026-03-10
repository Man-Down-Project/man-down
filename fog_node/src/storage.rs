use crate::events::Envelope;
use rusqlite::{Connection, Result};

pub struct Storage {
    conn: Connection,
}

impl Storage {
    pub fn new(path: &str) -> Result<Self> {
        let conn = Connection::open(path)?;

        conn.execute(
            "CREATE TABLE IF NOT EXISTS events (
            id INTEGER PRIMARY KEY,
            device_id TEXT NOT NULL,
            mesh_node_id TEXT NOT NULL,
            seq INTEGER NOT NULL,
            sent_at TEXT NOT NULL,
            incident TEXT NOT NULL
            )",
            [],
        )?;
        Ok(Self { conn })
    }

    pub fn insert_event(&self, env: &Envelope) -> Result<()> {
        self.conn.execute(
            "INSERT INTO events (
            device_id,
            mesh_node_id,
            seq,
            sent_at,
            incident
        )
        VALUES (?1, ?2, ?3, ?4, ?5)",
            (
                &env.device_id,
                &env.mesh_node_id,
                env.seq,
                env.sent_at.to_rfc3339(),
                format!("{:?}", env.incident),
            ),
        )?;

        Ok(())
    }
}
