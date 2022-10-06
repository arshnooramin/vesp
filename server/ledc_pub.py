import paho.mqtt.client as mqtt
import time

mqttBroker = "mqtt.bucknell.edu"
mqttPort = "7cdfa194a9"

client = mqtt.Client(mqttPort)
client.connect(mqttBroker)

peripheralTopic = "/ledc/"
gpioPin = "12"
qos = 2

def on_log(client, userdata, level, buf):
    print("log: ",buf)

client.loop_start()
client.on_log=on_log

# reset the pin to use
print(f"Configuring LEDC Timer")
client.publish(peripheralTopic + "timer_config",
               ",".join(["0", "0", "0", "2000", "0"]), qos=qos)
client.publish(peripheralTopic + "channel_config",
               ",".join([gpioPin, "0", "0", "0", "0", "0", "0", "0"]), qos=qos)

counter = 0
try:
    while True:
        counter += 1
        print(f"{counter}: Setting duty")
        client.publish(peripheralTopic + "set_duty", "0,0,2000", qos=qos)
        client.publish(peripheralTopic + "update_duty", "0,0", qos=qos)
        time.sleep(1)
        client.publish(peripheralTopic + "set_duty", "0,0,0", qos=qos)
        client.publish(peripheralTopic + "update_duty", "0,0", qos=qos)
        time.sleep(1)
except KeyboardInterrupt:
    client.publish(peripheralTopic + "set_duty", "0,0,0", qos=qos)
    client.publish(peripheralTopic + "update_duty", "0,0", qos=qos)
client.loop_stop()


