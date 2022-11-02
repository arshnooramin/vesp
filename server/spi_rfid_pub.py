import paho.mqtt.client as mqtt
import paho.mqtt.subscribe as subscribe
import time
import random

mqttBroker = "mqtt.bucknell.edu"
mqttPort = "7cdfa194a9"

client = mqtt.Client(mqttPort)
client.connect(mqttBroker)

peripheralTopic = "/spi/"
qos = 2

is_msg = False
msg_str = ''

def on_connect(client, userdata, flags, rc):
    client.subscribe("/console/spi/device_transmit")

def on_log(client, userdata, level, buf):
    print("log: ", buf)

def on_message(client, userdata, msg):
    # print(msg.topic+" "+str(msg.payload))
    global is_msg
    global msg_str
    is_msg = True
    msg_str = msg.payload.decode('utf-8', errors='replace')

client.loop_start()
# client.on_log=on_log
client.on_connect=on_connect
client.on_message=on_message

client.publish(peripheralTopic + "bus_initialize",
               ",".join(["2", "34", "33", "35", "-1", "-1", "0", "0", "0", "0"]), qos=qos)
time.sleep(0.5)
client.publish(peripheralTopic + "bus_add_device",
               ",".join(["2", "0", "0", "0", "0", "15", "5000000", "7"]), qos=qos)

def clear_bitmask(addr, mask):
    # print('clear_bitmask')
    readaddr = read(addr)
    write(addr, readaddr & (~mask))

def set_bitmask(addr, mask):
    # print('set_bitmask')
    readaddr = read(addr)
    write(addr, readaddr | mask)

def write_to_card(command, send_data):
    # print('write_to_card')
    back_len = 0
    status = 2
    back_data = []

    if command == 0x0E:
        irq = 0x12
        irq_wait = 0x10
    elif command == 0x0C:
        irq = 0x77
        irq_wait = 0x30
    write(0x02, irq | 0x80)
    clear_bitmask(0x04, 0x80)
    set_bitmask(0x0A, 0x80)
    write(0x01, 0x00)

    for data in send_data:
        write(0x09, data)
    write(0x01, command)

    if command == 0x0C:
        set_bitmask(0x0D, 0x80)
    
    i = 1000
    while True:
        n = read(0x04)
        i = i - 1
        if (not (i != 0 and (((n & 0x01) == 0) and ((n & irq_wait) == 0)))):
            break
    
    clear_bitmask(0x0D, 0x80)

    if i != 0:
        temp = read(0x06)
        if (temp & 0x1B) == 0x00:
            status = 0
            if command == 0x0C:
                n = read(0x0A)
                last_bits = read(0x0C) & 0x07

                if last_bits != 0:
                    back_len = (n - 1) + last_bits
                else:
                    back_len = n
                if n == 0:
                    n = 1
                if n > 16:
                    n = 16
                
                for i in range(n):
                    back_data.append(read(0x09))
        else:
            status = 2
    return (status, back_data, back_len)
                

def read(addr):
    # print('read')
    global is_msg
    global msg_str
    spi_arr_str = "1,1," + ",".join([str(int((addr << 1) & 0x7E | 0x80))])
    client.publish(peripheralTopic + "device_transmit", spi_arr_str, qos=qos)
    # event class
    while (not is_msg) and (len(msg_str) <= 0):
        # timeout or message queue
        pass
    retval = int(msg_str.split(',')[0])
    is_msg = False
    msg_str = ''
    return retval

def read_n(addr, n):
    # print('read')
    global is_msg
    global msg_str
    spi_arr_str = f"1,{n}," + ",".join([str(int((addr << 1) & 0x7E))])
    client.publish(peripheralTopic + "device_transmit", spi_arr_str, qos=qos)
    while (not is_msg) and (len(msg_str) <= 0):
        pass
    retval = int(msg_str.split(',')[0])
    is_msg = False
    msg_str = ''
    return retval

def write(addr, val):
    # print('write')
    spi_arr_str = "2,0," + ",".join([str(int((addr << 1) & 0x7E)), str(int(val))])
    client.publish(peripheralTopic + "device_transmit", spi_arr_str, qos=qos)

def write_n(addr, val_arr, n):
    # print('write')
    val_arr = [str(int((addr << 1) & 0x7E))] + val_arr
    spi_arr_str = f"{n + 1},0," + ",".join(val_arr)
    client.publish(peripheralTopic + "device_transmit", spi_arr_str, qos=qos)

def request():
    # print('request')
    write(0x0D, 0x07)
    status, back_data, back_len = write_to_card(0x0C, [0x26])

    return status, back_len

def anticoll():
    # print('anticoll')
    write(0x0D, 0x00)
    status, back_data, back_len = write_to_card(0x0C, [0x93, 0x20])
    return status, back_data

counter = 1
try:
    # client.publish("/gpio/reset_pin", "5")
    # client.publish("/gpio/set_direction",
    #             ",".join(["5", "2"]))
    # client.publish("/gpio/set_level",
    #             ",".join(["5", "0"]))
    # time.sleep(1)
    # client.publish("/gpio/reset_pin", "5")
    # client.publish("/gpio/set_direction",
    #             ",".join(["5", "2"]))
    # client.publish("/gpio/set_level",
    #             ",".join(["5", "1"]))
    # time.sleep(1)
    write(0x01, 0x0F)
    write(0x2A, 0x8D)
    write(0x2B, 0x3E)
    write(0x2D, 0x1E)
    write(0x2C, 0x00)
    write(0x15, 0x40)
    write(0x11, 0x3D)
    temp = read(0x14)
    if (~(temp & 0x03)):
        set_bitmask(0x14, 0x03)
    while True:
        status, back_len = request()
        print("request=", status, back_len)
        status, back_data = anticoll()
        print("anticoll=", status, back_data)
except KeyboardInterrupt:
    # client.publish(peripheralTopic + "device_queue_trans", ",".join(rgb_arr_off), qos=qos)
    client.loop_stop()


