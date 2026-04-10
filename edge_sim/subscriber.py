import paho.mqtt.client as mqtt

BROKER = "mqtt-broker"
PORT = 1883
TOPIC = "mesh/node/#"   # # = wildcard (alla under mesh/node)

def on_connect(client, userdata, flags, rc):
    print(f"Connected with result code {rc}")
    client.subscribe(TOPIC)

def on_message(client, userdata, msg):
    print(f"Received on {msg.topic}: {msg.payload}")

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

client.connect(BROKER, PORT, 60)
client.loop_forever()
