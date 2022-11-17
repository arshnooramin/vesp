import paho.mqtt.client as mqtt
import time

mqttBroker = "mqtt.bucknell.edu"
mqttPort = "7cdfa194a9"

def on_connect(client, userdata, flags, rc):
    client.subscribe('/console/gpio/isr_handler')

def on_message(client, userdata, message):
    gpio = message.payload.decode('utf-8', errors='replace')
    print(f"Interrupt detected on GPIO{gpio}")

mqttBroker = "mqtt.bucknell.edu"

client = mqtt.Client(mqttPort)
client.connect(mqttBroker)

client.loop_start()
client.on_message = on_message
client.on_connect = on_connect

peripheralTopic = "/gpio/"
gpioPin = "13"

# reset the pin to use
print(f"Initializing GPIO Pin {gpioPin}")
client.publish(peripheralTopic + "reset_pin", gpioPin)
client.publish(peripheralTopic + "set_direction",
               ",".join([gpioPin, "1"]))
client.publish(peripheralTopic + "pulldown_en", gpioPin)
client.publish(peripheralTopic + "set_intr_type",
               ",".join([gpioPin, "1"]))
client.publish(peripheralTopic + "install_isr_service")
client.publish(peripheralTopic + "isr_handler_add", gpioPin)

try:
    while True:
        time.sleep(0.5)
except KeyboardInterrupt:
    client.loop_stop()