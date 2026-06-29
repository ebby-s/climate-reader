#include <Arduino.h>
#include <DHT.h>
#include <WiFi.h>
#include <esp_wpa2.h>
#include <esp_wifi.h>
#include <esp_system.h>
#include <HTTPClient.h>
#include <Adafruit_NeoPixel.h>
#include <Preferences.h>
#include "secrets.h"

#define DHT_PIN     2
#define DHT_TYPE    DHT11

#define LED_PIN     8
#define LED_COUNT   1
#define LED_BRIGHT  10

DHT dht(DHT_PIN, DHT_TYPE);
Adafruit_NeoPixel rgbLed(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

static const uint32_t WIFI_RETRY_INTERVAL = 15000;
static const uint32_t UPLOAD_INTERVAL     = 60000;

const char* wifiStatusStr(wl_status_t s) {
  switch (s) {
    case WL_IDLE_STATUS:     return "WL_IDLE_STATUS";
    case WL_NO_SSID_AVAIL:   return "WL_NO_SSID_AVAIL";
    case WL_SCAN_COMPLETED:  return "WL_SCAN_COMPLETED";
    case WL_CONNECTED:       return "WL_CONNECTED";
    case WL_CONNECT_FAILED:  return "WL_CONNECT_FAILED";
    case WL_CONNECTION_LOST: return "WL_CONNECTION_LOST";
    case WL_DISCONNECTED:    return "WL_DISCONNECTED";
    default:                 return "UNKNOWN";
  }
}

void ledOff() {
  rgbLed.clear();
  rgbLed.show();
}

void ledFlash(uint8_t r, uint8_t g, uint8_t b, int ms) {
  rgbLed.setPixelColor(0, rgbLed.Color(r, g, b));
  rgbLed.show();
  delay(ms);
  ledOff();
}

bool connectWiFi() {
  Serial.print("Connecting to "); Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(200);

  esp_wifi_sta_wpa2_ent_set_disable_time_check(true);
  WiFi.begin(WIFI_SSID, WPA2_AUTH_PEAP,
             EAP_IDENTITY, EAP_USERNAME, EAP_PASSWORD);

  Serial.print("Waiting");
  int a = 0;
  while (WiFi.status() != WL_CONNECTED && a < 180) {
    delay(500);
    if (a % 10 == 0) Serial.print(".");
    a++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi connected, IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }

  Serial.print("WiFi failed: ");
  Serial.println(wifiStatusStr(WiFi.status()));
  return false;
}

bool uploadToThingSpeak(float t, float h) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, skipping upload");
    return false;
  }

  HTTPClient http;
  String url = "http://api.thingspeak.com/update?api_key="
             + String(THINGSPEAK_API_KEY)
             + "&field1=" + String(t)
             + "&field2=" + String(h);

  http.begin(url);
  int httpCode = http.GET();

  if (httpCode > 0) {
    Serial.print("ThingSpeak upload OK, HTTP ");
    Serial.println(httpCode);
    ledFlash(0, 5, 0, 50);
    http.end();
    return true;
  }

  Serial.print("ThingSpeak upload failed, error: ");
  Serial.println(http.errorToString(httpCode).c_str());
  ledFlash(8, 0, 0, 50);
  delay(120);
  ledFlash(8, 0, 0, 50);
  http.end();
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== Climate Reader ===");

  rgbLed.begin();
  rgbLed.setBrightness(LED_BRIGHT);
  ledOff();

  uint8_t reason = esp_reset_reason();
  Preferences prefs;
  prefs.begin("climate", false);
  uint32_t brownoutCount = prefs.getUInt("boc", 0);

  if (reason == ESP_RST_BROWNOUT) {
    brownoutCount++;
    prefs.putUInt("boc", brownoutCount);

    for (int i = 0; i < 3; i++) {
      ledFlash(10, 0, 0, 40);
      delay(120);
    }

    uint32_t cooldown = brownoutCount < 12 ? brownoutCount * 5u : 60u;
    Serial.printf("BROWNOUT #%u detected — cooling %u s\n",
                  brownoutCount, cooldown);
    while (cooldown--) delay(1000);
  } else {
    brownoutCount = 0;
    prefs.putUInt("boc", 0);
  }

  prefs.end();

  Serial.printf("Boot — reset: %u  brownout tally: %u\n",
                reason, brownoutCount);

  WiFi.setTxPower(WIFI_POWER_17dBm);
  esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

  dht.begin();
  connectWiFi();
}

void loop() {
  static uint32_t lastWiFiRetry = 0;

  if (WiFi.status() != WL_CONNECTED) {
    static uint32_t lastBlue = 0;
    if (millis() - lastBlue > 3000) {
      lastBlue = millis();
      ledFlash(0, 0, 8, 50);
    }

    if (millis() - lastWiFiRetry > WIFI_RETRY_INTERVAL) {
      lastWiFiRetry = millis();
      Serial.println("WiFi down, reconnecting …");
      connectWiFi();
    }

    delay(1000);
    return;
  }

  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    Serial.println("DHT11 read failed");
  } else {
    Serial.printf("Temp: %.1f °C  Humidity: %.1f %%\n", t, h);
    uploadToThingSpeak(t, h);
  }

  delay(UPLOAD_INTERVAL);
}
