This project is designed to monitor whether a room (e.g., a study hall) remains silent, triggering an alarm if noise levels exceed a set threshold. It employs an ESP32 microcontroller alongside a Raspberry Pi Zero. Sound is captured using a KY-038 microphone. Communication between the two devices is handled via the MQTT protocol, with a Mosquitto broker running on the Pi. To enable long-term analysis of the noise environment, sound level data is stored locally on the Raspberry Pi using a SQLite database.


![image](https://github.com/user-attachments/assets/3baa18f3-a6ff-4686-9d3b-71ca7c335482)
