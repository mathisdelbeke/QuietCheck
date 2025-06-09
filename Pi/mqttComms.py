#!/usr/bin/env python3

import paho.mqtt.client as mqtt

MQTT_BROKER_URI = "192.168.0.202"
MQTT_PORT = 1883
MQTT_MEASUREMENTS_TOPIC = "esp32/noise" 

def on_mqtt_connect(client, userdata, flags, rc):
    if rc == 0:
        client.subscribe(MQTT_MEASUREMENTS_TOPIC)

def on_mqtt_message(client, userdata, msg):
    noise = msg.payload[0] | (msg.payload[1] << 8)
    print(noise)

mqtt_client = mqtt.Client()
mqtt_client.on_connect = on_mqtt_connect
mqtt_client.on_message = on_mqtt_message

mqtt_client.connect(MQTT_BROKER_URI, MQTT_PORT, 60)
mqtt_client.loop_forever()
