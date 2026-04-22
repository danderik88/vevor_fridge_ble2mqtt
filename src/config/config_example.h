#ifndef _HEADER_ALPICOOL_CONFIG
#define _HEADER_ALPICOOL_CONFIG

// wifi SSID
#define WIFI_SSID "YOUR_WIFI_SSID"
// wifi password
#define WIFI_PASS "YOUR_WIFI_PASSWORD"
// mqtt server. "192.168.0.1" also works
#define MQTT_SERVER "192.168.1.100"
// mqtt user. anonynymous mqqt brokers are not supported
#define MQTT_USER "YOUR_MQTT_USER"
// mqtt password
#define MQTT_PASSWORD "YOUR_MQTT_PASSWORD"
// Ble adress of the fridge
#define FRIDGE_BLE_ADDRESS "11:22:33:44:55:66"

// the rest you can keep defult velues
// device_name, used as mqtt client_id
#define DEVICE_NAME "alpicool-esp32"
// topic to send state data
#define SENSOR_TOPIC "tele/alpicool/sensor"
// topic to subscribe for control commnads (on, off, set temperature)
#define COMMAND_TOPIC "tele/alpicool/cmd"
// topic to publish debug messages
#define DEBUG_TOPIC "tele/alpicool/debug"
// topic to publish availability (online/offline)
#define AVAILABILITY_TOPIC "tele/alpicool/availability"

// how often are state data send to mqtt
#define DATA_PUBLISH_FREQ_MILLIS 1000
#endif
