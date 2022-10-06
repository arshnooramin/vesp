import paho.mqtt.client as mqtt
import time

mqttBroker = "mqtt.bucknell.edu"
mqttPort = "7cdfa194a9"

def on_connect(client, userdata, flags, rc):
    client.subscribe('/console/gpio/get_level')

def on_message(client, userdata, message):
    level = message.payload.decode('utf-8', errors='replace')
    level_str = "HIGH" if level == "1" else "LOW"
    print(f"Reading: {level_str}")

mqttBroker = "mqtt.bucknell.edu"

client = mqtt.Client(mqttPort)
client.connect(mqttBroker)

client.loop_start()
client.on_message = on_message
client.on_connect = on_connect

peripheralTopic = "/gpio/"
gpioPin = "5"

# reset the pin to use
print(f"Initializing GPIO Pin {gpioPin}")
client.publish(peripheralTopic + "reset_pin", gpioPin)
client.publish(peripheralTopic + "set_direction",
               ",".join([gpioPin, "1"]))

try:
    while True:
        client.publish(peripheralTopic + "get_level", gpioPin)
        time.sleep(0.5)
except KeyboardInterrupt:
    client.loop_stop()