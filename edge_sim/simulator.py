import time
import random
import paho.mqtt.client as mqtt

BROKER = "mqtt-broker"
PORT = 1883
TOPIC = "mesh/node/test"

client = mqtt.Client()
client.connect(BROKER, PORT, 60)
client.loop_start()

seq = 1
timestamp = 16

event_types = [
        0x01, # ManDown
        0x02, # Gas
        0x03, # BatteryLow
]

device_ids = [1, 2, 3]
locations = [1, 2, 3, 4]

while True:
    device_id = random.choice(device_ids)
    event_type = random.choice(event_types)
    location = random.choice(locations)

    if event_type == 0x03:
        battery = random.randint(5, 25)
    else:
        battery = random.randint(60, 100)

    payload = bytes([
        device_id,
        event_type,
        location,
        battery,
        seq,
        timestamp & 0xFF,
        (timestamp >> 8) & 0xFF,
    ])

    result = client.publish(TOPIC, payload)
    print(
        f"Sent event: device={device_id} type=0x{event_type:02x} "
        f"location={location} battery={battery} seq={seq} ts={timestamp} "
        f"rc={result.rc}",
        flush=True,
    )

    seq = (seq % 255) + 1
    timestamp += 1

    time.sleep(5)
