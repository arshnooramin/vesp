import paho.mqtt.client as mqtt
import time

mqttBroker = "mqtt.bucknell.edu"
mqttPort = "7cdfa194a9"

client = mqtt.Client(mqttPort)
client.connect(mqttBroker)

peripheralTopic = "/spi/"
gpioPin = "12"
qos = 2

def on_log(client, userdata, level, buf):
    print("log: ", buf)

client.loop_start()
client.on_log=on_log

# reset the pin to use
# print(f"Configuring LEDC Timer")
client.publish(peripheralTopic + "bus_initialize",
               ",".join(["1", "18", "-1", "26", "-1", "-1", "0", "0", "0", "1"]), qos=qos)
time.sleep(0.5)
client.publish(peripheralTopic + "bus_add_device",
               ",".join(["1", "0", "0", "0", "3", "1", "1000000", "1"]), qos=qos)

rgb_arr = [str(4 + 4*15 + 4 + 1)]

for i in range(5):
    rgb_arr.append(str(50))
    rgb_arr.append(str(255))
    rgb_arr.append(str(0))
    rgb_arr.append(str(0))


for i in range(5):
    rgb_arr.append(str(50))
    rgb_arr.append(str(0))
    rgb_arr.append(str(255))
    rgb_arr.append(str(0))


for i in range(5):
    rgb_arr.append(str(50))
    rgb_arr.append(str(0))
    rgb_arr.append(str(0))
    rgb_arr.append(str(255))

try:
    while True:
        client.publish(peripheralTopic + "device_queue_trans", ",".join(rgb_arr), qos=qos)
        time.sleep(1)
except KeyboardInterrupt:
    client.publish(peripheralTopic + "device_queue_trans", ",".join(rgb_arr_off), qos=qos)
    client.loop_stop()


