import paho.mqtt.client as mqtt
import time
import random

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

length = str(4 + 4*15 + 4 + 1)
rgb_arr = list()

# start frame
for i in range(4):
    rgb_arr.append(str(0))

for i in range(15):
    rgb_arr.append(str(31 + 224))
    rgb_arr.append(str(0))
    rgb_arr.append(str(0))
    rgb_arr.append(str(0))

# end frame
for i in range(4):
    rgb_arr.append(str(255))

counter = 1
try:
    while True:
        print(rgb_arr)
        if len(rgb_arr) > int(length):
            print("invalid_str")
            break
        if counter > 15:
            counter = 1
            for i in range(1, 16):
                rgb_arr[i*4] = str(31 + 224)
                rgb_arr[i*4 + 1] = str(0)
                rgb_arr[i*4 + 2] = str(0)
                rgb_arr[i*4 + 3] = str(0)
        rgb_arr[counter*4 + 1] = str(random.randint(0, 255))
        rgb_arr[counter*4 + 2] = str(random.randint(0, 255))
        rgb_arr[counter*4 + 3] = str(random.randint(0, 255))
        rgb_arr_str = length + "," + ",".join(rgb_arr)
        client.publish(peripheralTopic + "device_queue_trans", rgb_arr_str, qos=qos)
        counter += 1
except KeyboardInterrupt:
    # client.publish(peripheralTopic + "device_queue_trans", ",".join(rgb_arr_off), qos=qos)
    client.loop_stop()


