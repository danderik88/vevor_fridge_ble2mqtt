#include "WiFi.h"
#include "mqtt.h"

// Safety net: if the WiFi driver gets wedged (coex starvation on a weak AP,
// DHCP lease lost, AP disassoc with an unhandled reason), a plain
// WiFi.begin() sometimes never recovers. We escalate: normal retries first,
// then a full driver reset (WIFI_OFF → WIFI_STA), then ESP.restart().
static int failure_count = 0;
static uint32_t offline_since_ms = 0;

int connect_wifi(const char *ssid, const char *pass)
{

  if (WiFi.status() == WL_CONNECTED) {
    failure_count = 0;
    offline_since_ms = 0;
    return 0;
  }

  if (offline_since_ms == 0) offline_since_ms = millis();

  bool hard_reset = (failure_count >= 3);

  ESP_LOGI("WIFI", "Connecting to WiFi (fail=%d, hard_reset=%d)...", failure_count, hard_reset);
  if (hard_reset) {
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    delay(500);
    WiFi.mode(WIFI_STA);
  } else {
    WiFi.disconnect();
    delay(100);
  }
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, pass);

  if (WiFi.waitForConnectResult(5000) != WL_CONNECTED)
  {
    ESP_LOGE("WIFI", "WiFi Failed!");
    failure_count++;
    if (millis() - offline_since_ms > 600000) {
      ESP_LOGE("WIFI", "Offline > 10 min, restarting");
      ESP.restart();
    }
    return -1;
  }
  delay(100);

  ESP_LOGI("WIFI", "WiFi connection Successful");
  ESP_LOGI("WIFI", "%s", WiFi.localIP().toString().c_str()); // Print the IP address
  ESP_LOGI("WIFI", "%s", WiFi.macAddress().c_str());
  failure_count = 0;
  offline_since_ms = 0;
  return 0;
}