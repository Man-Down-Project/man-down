use rppal::spi::{Bus, Mode, SlaveSelect, Spi};
use rppal_mfrc522::Mfrc522;
use std::time::Duration;

pub fn read_from_rfid_reader() -> String {
    match try_read_from_rfid_reader() {
        Ok(tag_id) => tag_id,
        Err(err) => {
            log::error!("RFID: failed to read tag: {}", err);
            String::new()
        }
    }
}

fn try_read_from_rfid_reader() -> Result<String, Box<dyn std::error::Error + Send + Sync>> {
    let mut spi = Spi::new(Bus::Spi0, SlaveSelect::Ss0, 1_000_000, Mode::Mode0)?;
    let mut rfid = Mfrc522::new(&mut spi);

    rfid.reset()?;

    loop {
        match rfid.uid(Duration::from_millis(250)) {
            Ok(uid) => {
                let tag_id = format!("{:08X}", uid.to_u32());
                return Ok(tag_id);
            }
            Err(_) => {
                std::thread::sleep(Duration::from_millis(100));
            }
        }
    }
}
