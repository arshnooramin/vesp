import paho.mqtt.client as mqtt
import time

mqttBroker = "mqtt.bucknell.edu"
mqttPort = "7cdfa194a9"

client = mqtt.Client(mqttPort)
client.connect(mqttBroker)

peripheralTopic = "/gpio/"
gpioPin, gpioLevel = "2", "0"

# reset the pin to use
print(f"Initializing GPIO Pin {gpioPin}")
client.publish(peripheralTopic + "reset_pin", gpioPin)
client.publish(peripheralTopic + "set_direction",
               ",".join([gpioPin, "2"]))

while True:
    gpioLevel = "1" if gpioLevel == "0" else "0"
    levelStr = "HIGH" if gpioLevel == "1" else "LOW"
    print(f"Setting level {levelStr}")
    client.publish(peripheralTopic + "set_level",
                   ",".join([gpioPin, gpioLevel]))
    time.sleep(.25)
