#!/usr/bin/env python3

import paho.mqtt.client as mqtt

MQTT_BROKER_URI = "192.168.0.202"   
MQTT_PORT = 1883                
MQTT_MEASUREMENTS_TOPIC = "esp32/noice" 

def on_mqtt_connect(client, userdata, flags, rc):
    if rc == 0:
        client.subscribe(MQTT_MEASUREMENTS_TOPIC)

# Callback when a message is received
def on_mqtt_message(client, userdata, msg):
    value = msg.payload[0] | (msg.payload[1] << 8)
    print(value)

mqtt_client = mqtt.Client()
mqtt_client.on_connect = on_mqtt_connect
mqtt_client.on_message = on_mqtt_message

# (Optional) Authentication
# client.username_pw_set("your_username", "your_password")

# Connect to broker
mqtt_client.connect(MQTT_BROKER_URI, MQTT_PORT, 60)

# Loop forever to wait for messages
mqtt_client.loop_forever()
