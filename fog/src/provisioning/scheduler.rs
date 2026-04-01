use crate::mqtt::OutgoingMessage;
use crate::provisioning::manager::{
    build_hmac_edge_payload, build_hmac_mesh_payload, generate_hmac_key_hex,
};
use crate::provisioning::models::HmacState;
use crate::provisioning::state::{load_hmac_state, save_hmac_state};
use chrono::{DateTime, Datelike, Days, LocalResult, TimeZone, Utc};
use chrono_tz::Europe::Stockholm;
use tokio::sync::mpsc::Sender;
use tokio::time::{Duration, sleep};

pub async fn run_hmac_rotation_scheduler(outgoing_tx: Sender<OutgoingMessage>) {
    loop {
        let Some(existing) = load_hmac_state() else {
            log::warn!("Scheduler: no HMAC state found, retrying in 60 seconds");
            sleep(Duration::from_secs(60)).await;
            continue;
        };

        let next_rotation = next_rotation_time_stockholm(existing.created_at);
        let now = Utc::now();
        let wait_duration = match (next_rotation - now).to_std() {
            Ok(d) => d,
            Err(_) => Duration::from_secs(0),
        };

        log::info!("Scheduler: next HMAC rotation at {}", next_rotation);
        sleep(wait_duration).await;

        let Some(current) = load_hmac_state() else {
            log::warn!("Scheduler: HMAC disappeared before rotation, retrying");
            continue;
        };

        if current.created_at > existing.created_at {
            log::info!("Scheduler: HMAC already rotated by another path, recalculating");
            continue;
        }

        log::info!("Scheduler: rotation time reached, generating new HMAC");

        let new_hmac = HmacState {
            key_hex: generate_hmac_key_hex(),
            created_at: Utc::now(),
        };

        if let Err(e) = save_hmac_state(&new_hmac) {
            log::error!("Scheduler: failed to save rotated HMAC: {}", e);
            continue;
        }

        let mesh_payload = build_hmac_mesh_payload(&new_hmac);
        let edge_payload = build_hmac_edge_payload(&new_hmac);

        if let Err(e) = outgoing_tx
            .send(OutgoingMessage {
                topic: "mesh/provisioning/hmac".to_string(),
                payload: mesh_payload,
            })
            .await
        {
            log::error!("Scheduler: failed to publish mesh HMAC: {}", e);
        }

        if let Err(e) = outgoing_tx
            .send(OutgoingMessage {
                topic: "edge/provisioning/hmac".to_string(),
                payload: edge_payload,
            })
            .await
        {
            log::error!("Scheduler: failed to publish edge HMAC: {}", e);
        }
    }
}

fn next_rotation_time_stockholm(created_at_utc: DateTime<Utc>) -> DateTime<Utc> {
    let created_local = created_at_utc.with_timezone(&Stockholm);
    let rotation_date = created_local.date_naive() + Days::new(7);

    let next_local = match Stockholm.with_ymd_and_hms(
        rotation_date.year(),
        rotation_date.month(),
        rotation_date.day(),
        0,
        0,
        0,
    ) {
        LocalResult::Single(dt) => dt,
        LocalResult::Ambiguous(dt, _) => dt,
        LocalResult::None => {
            let fallback = rotation_date
                .and_hms_opt(0, 0, 0)
                .expect("valid midnight fallback");
            Stockholm
                .from_local_datetime(&fallback)
                .earliest()
                .expect("resolvable local datetime")
        }
    };

    next_local.with_timezone(&Utc)
}
