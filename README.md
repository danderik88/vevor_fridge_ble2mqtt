# Alpicool ESP32 MQTT Gateway (Improved Fork)

This project allows you to control and monitor Alpicool (and compatible) fridges over WiFi and MQTT using an ESP32. It bridges the proprietary BLE protocol of the fridge to a standard MQTT interface, making it perfect for Home Assistant integration.

This is an **improved and actively maintained fork** of the original project, containing several critical bugfixes and new features to ensure rock-solid stability and modern usability.

## 🚀 Key Improvements in this Fork

- **WDT Panic & Crash Fixes:** Fixed severe `Interrupt wdt timeout on CPU0` crashes that occurred in environments with many BLE devices (like Apple devices). The ESP32 now flawlessly filters out duplicate advertisements and yields properly, ensuring 100% stable uptime.
- **Over-The-Air (OTA) Updates:** Added native `ArduinoOTA` support! You can now flash future firmware updates wirelessly without ever needing to unplug the ESP32 or open its case.
- **Serial Bottleneck Removed:** Increased baud rate from `9600` to `115200`, preventing internal buffer overflows that triggered the watchdog.
- **Fixed Home Assistant YAML:** The provided `configuration.yaml` for Home Assistant has been fully rewritten, fixing trailing quote syntax errors, broken standard presets (`boost` instead of `burst`), incorrect topics, and boolean type-casting bugs in binary sensors.
- **Strict MAC Address Matching Fixed:** Ensure the MAC string is accurately resolved without manual casing bugs.

## 🛠 Prerequisites
You will need:
- An **ESP32** developer board (ESP32-WROOM, etc).
- PlatformIO extension for Visual Studio Code.
- A running MQTT broker (Mosquitto) and Home Assistant.
- Your fridge's BLE MAC Address (Find it via the official Alpicool App or a BLE Scanner app).

## 📥 Installation & Setup

1. **Clone the repository:**
   Download or clone this repo and open it in Visual Studio Code with PlatformIO.

2. **Setup your Configuration:**
   - Locate `src/config/config_example.h`.
   - Rename it to `src/config/config.h`.
   - Open `config.h` and fill in your WiFi credentials, MQTT broker details, and your fridge's **lowercase** MAC address.

   ```cpp
   #define WIFI_SSID "YOUR_WIFI_SSID"
   #define WIFI_PASS "YOUR_WIFI_PASSWORD"
   #define MQTT_SERVER "192.168.1.100"
   // Make sure the MAC address is in lowercase!
   #define FRIDGE_BLE_ADDRESS "11:22:33:44:55:66" 
   ```

3. **Build and Flash:**
   Connect your ESP32 via USB and click the **PlatformIO: Upload** button (or run `pio run -t upload`). 
   *Note: For all future updates, your ESP32 will automatically appear in PlatformIO as an OTA network port!*

## 🏠 Home Assistant Integration

Copy the following code into your Home Assistant `configuration.yaml` file to instantly get a Climate card and all sensor entities. 

*If your config already has an `mqtt:` block, just merge the sections.*

```yaml
mqtt:
  climate:
    - name: "Alpicool fridge"
      unique_id: "alpicoolfriddgeuid"
      max_temp: 20
      min_temp: -20
      modes:
        - "off"
        - "cool"
      current_temperature_topic: "tele/alpicool/sensor"
      current_temperature_template: "{{ value_json.f_actual_temperature | round(1, 'floor') }}"
      preset_modes:
        - "eco"
        - "boost"
      preset_mode_command_template: >-
        {% set jsonon = { 'fridge_eco': True } %}
        {% set jsonoff = { 'fridge_eco': False } %}
        {{ jsonon | tojson if value == 'eco' else jsonoff | tojson }}
      preset_mode_state_topic: "tele/alpicool/sensor"
      preset_mode_value_template: >-
        {{ 'eco' if value_json.f_eco == True else 'boost' }}
      preset_mode_command_topic: "tele/alpicool/cmd"
      mode_state_topic: "tele/alpicool/sensor"
      mode_state_template: >-
        {{ 'cool' if value_json.f_on == True else 'off' }}
      mode_command_topic: "tele/alpicool/cmd"
      mode_command_template: >-
        {% set jsonon = { 'fridge_on': True } %}
        {% set jsonoff = { 'fridge_on': False } %}
        {{ jsonon | tojson if value == 'cool' else jsonoff | tojson }}
      power_command_topic: "tele/alpicool/cmd"
      payload_on: '{ "fridge_on":true }'
      payload_off: '{ "fridge_on":false }'
      temperature_command_topic: "tele/alpicool/cmd"
      temperature_command_template: '{"fridge_temperature": {{ value }} }'
      temperature_state_topic: "tele/alpicool/sensor"
      temperature_state_template: "{{ value_json.f_desired_temperature | round(1, 'floor')}}"
      temp_step: 1
      precision: 1

  binary_sensor:
    - name: "Fridge On"
      unique_id: "zmkfridgeonoff"
      state_topic: "tele/alpicool/sensor"
      payload_on: "True"
      payload_off: "False"
      value_template: "{{ value_json.f_on }}"

    - name: "Fridge Eco Mode"
      unique_id: "zmkfridgeecomode"
      state_topic: "tele/alpicool/sensor"
      payload_on: "True"
      payload_off: "False"
      value_template: "{{ value_json.f_eco }}"

  sensor:
    - name: "Fridge Set Temperature"
      unique_id: "zmkfridgessettemperature"
      state_topic: "tele/alpicool/sensor"
      suggested_display_precision: 0
      unit_of_measurement: "°C"
      value_template: "{{ value_json.f_desired_temperature }}"

    - name: "Fridge Temperature"
      unique_id: "zmkfridgetemperature"
      state_topic: "tele/alpicool/sensor"
      suggested_display_precision: 0
      unit_of_measurement: "°C"
      value_template: "{{ value_json.f_actual_temperature }}"

    - name: "Fridge Voltage"
      unique_id: "zmkfridgevoltage"
      state_topic: "tele/alpicool/sensor"
      suggested_display_precision: 2
      unit_of_measurement: "V"
      value_template: "{{ value_json.f_voltage | round(2, 'floor') }}"
```
## 🌟 Special Thanks
Special thanks to the original author for the foundational BLE decoding and payload management!
