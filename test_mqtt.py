import paho.mqtt.client as mqtt
import sys

# Fill in your broker IP and credentials before running
broker = "192.168.1.100"
user = "mqtt-user"
password = "mqtt-password"

def on_connect(client, userdata, flags, rc, properties=None):
    if rc == 0:
        print("SUCCESS! Credentials are valid and connection accepted.")
        sys.exit(0)
    else:
        print(f"FAILED! Connection refused. Return code: {rc}")
        sys.exit(rc)

# paho-mqtt v2 usage
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
client.on_connect = on_connect
client.username_pw_set(user, password)

try:
    client.connect(broker, 1883, 10)
    client.loop_forever()
except Exception as e:
    print(f"EXCEPTION: {e}")
    sys.exit(1)
