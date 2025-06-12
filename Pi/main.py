#!/usr/bin/env python3

import data_base
import mqtt_comms
import time
import queue

data_base.init()
mqtt_comms.init()

while True:
    try:
        noise = mqtt_comms.message_queue.get(timeout=1) # Wait for data
        data_base.add_noise_reading(noise)
        data_base.get_all_readings()
    
    except queue.Empty:
        continue