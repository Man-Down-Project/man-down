use rppal::spi::{Bus, Mode, SlaveSelect, Spi};
use rppal_mfrc522::Mfrc522;
use std::thread;
use std::time::{Duration, Instant};
use tokio::sync::mpsc;

pub fn start_rfid_reader_thread(tag_tx: mpsc::Sender<String>) {
    thread::spawn(move || {
        if let Err(err) = run_reader_loop(tag_tx) {
            log::error!("RFID: reader thread exited with error: {}", err);
        }
    });
}

fn run_reader_loop(
    tag_tx: mpsc::Sender<String>,
) -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
    let mut spi = Spi::new(Bus::Spi0, SlaveSelect::Ss0, 1_000_000, Mode::Mode0)?;
    let mut rfid = Mfrc522::new(&mut spi);

    rfid.reset()?;

    let mut last_tag = String::new();
    let mut last_seen = Instant::now() - Duration::from_secs(5);

    loop {
        match rfid.uid(Duration::from_millis(250)) {
            Ok(uid) => {
                let tag_id = format!("{:08X}", uid.to_u32());

                if tag_id == last_tag && last_seen.elapsed() < Duration::from_secs(2) {
                    thread::sleep(Duration::from_millis(150));
                    continue;
                }

                last_tag = tag_id.clone();
                last_seen = Instant::now();

                log::info!("RFID: read tag {}", tag_id);

                if let Err(err) = tag_tx.blocking_send(tag_id) {
                    log::error!("RFID: failed to forward tag to async service: {}", err);
                    return Ok(());
                }
            }
            Err(_) => {
                thread::sleep(Duration::from_millis(100));
            }
        }
    }
}
