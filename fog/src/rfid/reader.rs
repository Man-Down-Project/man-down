use rppal::spi::{Bus, Mode, SlaveSelect, Spi};
use rppal_mfrc522::Mfrc522;
use rppal::gpio::Gpio;
use std::thread;
use std::time::{Duration, Instant};
use tokio::sync::mpsc;
use std::sync::{
    atomic::{AtomicBool, Ordering},
    Arc,
};

pub fn start_rfid_reader_thread(tag_tx: mpsc::Sender<String>, enabled: Arc<AtomicBool>) {
    println!("RFID: reader thread starting...");
    thread::spawn(move || {
        if let Err(err) = run_reader_loop(tag_tx, enabled) {
            log::error!("RFID: reader thread exited with error: {}", err);
        }
    });
}

fn run_reader_loop(
    tag_tx: mpsc::Sender<String>,
    enabled: Arc<AtomicBool>,
) -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
    // let mut spi = Spi::new(Bus::Spi0, SlaveSelect::Ss0, 1_000_000, Mode::Mode0)?;
    // let mut rfid = Mfrc522::new(&mut spi);
    println!("RFID: initializing SPI...");
    let gpio = Gpio::new()?;
    let mut rst = gpio.get(25)?.into_output();

    rst.set_low();
    thread::sleep(Duration::from_millis(300));
    rst.set_high();
    thread::sleep(Duration::from_millis(300));
    let mut spi = Spi::new(Bus::Spi0, SlaveSelect::Ss0, 1_000_000, Mode::Mode0)?;
    println!("RFID: SPI initialized");
   

    let mut rfid = Mfrc522::new(&mut spi);
    println!("RFID: MFRC522 created");

    rfid.reset()?;
    println!("RFID: reader reset OK");

    let mut last_tag = String::new();
    let mut last_seen = Instant::now() - Duration::from_secs(5);

    loop {
    // 🔥 PAUSE RFID WHEN DISABLED
        if !enabled.load(Ordering::Relaxed) {
            thread::sleep(Duration::from_millis(300));
            continue;
        }
        
        match rfid.uid(Duration::from_millis(500)) {
            Ok(uid) => {
                let tag_id = format!("{:08X}", uid.to_u32());

                if tag_id == last_tag && last_seen.elapsed() < Duration::from_secs(2) {
                    thread::sleep(Duration::from_millis(150));
                    continue;
                }

                last_tag = tag_id.clone();
                last_seen = Instant::now();
                println!("RFID: detected tag {}", tag_id);
                log::info!("RFID: read tag {}", tag_id);

                if let Err(err) = tag_tx.blocking_send(tag_id) {
                    log::error!("RFID: failed to forward tag to async service: {}", err);
                    continue;
                }
            }
            Err(err) => {
                use rppal_mfrc522::Error;

                match err {
                    Error::Timeout => {
                    // normal → ignore
            }
            _ => {
                println!("RFID error: {:?}", err);
            }
        }

    thread::sleep(Duration::from_millis(200));
}
        }
    }
}
