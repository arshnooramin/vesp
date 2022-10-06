import paho.mqtt.client as mqtt
import time

mqttBroker = "mqtt.bucknell.edu"
mqttPort = "7cdfa194a9"

client = mqtt.Client(mqttPort)
client.connect(mqttBroker)

while True:
    topic = input("mqtt topic: ")
    data = input("mqtt data: ")
    print(f"Publishing {data} to {topic}")
    client.publish(topic, data)
