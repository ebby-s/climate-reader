#include <Arduino.h>
#include <DHT.h>
#include <WiFi.h>
#include <esp_wpa2.h>
#include <HTTPClient.h>
#include "secrets.h"

#define DHT_PIN 2
#define DHT_TYPE DHT11

DHT dht(DHT_PIN, DHT_TYPE);

const char* wifiStatusToString(wl_status_t status) {
  switch (status) {
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

static bool wifiInitDone = false;
static bool wasEverConnected = false;

void connectWiFi() {
  if (!wifiInitDone) {
    Serial.print("Connecting to ");
    Serial.println(WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    delay(200);

    esp_wifi_sta_wpa2_ent_set_disable_time_check(true);

    WiFi.begin(WIFI_SSID, WPA2_AUTH_PEAP, EAP_IDENTITY, EAP_USERNAME, EAP_PASSWORD);
    wifiInitDone = true;
  } else if (wasEverConnected) {
    Serial.println("Reconnecting WiFi...");
    WiFi.reconnect();
  }

  Serial.print("Waiting");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 180) {
    delay(500);
    if (attempts % 10 == 0) Serial.print(".");
    if (attempts > 0 && attempts % 40 == 0) {
      Serial.print("(");
      Serial.print(attempts / 2);
      Serial.print("s)");
    }
    attempts++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    if (!wasEverConnected) wasEverConnected = true;
    Serial.println("WiFi connected");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.print("WiFi not connected. Status: ");
    Serial.print(WiFi.status());
    Serial.print(" (");
    Serial.print(wifiStatusToString(WiFi.status()));
    Serial.println(")");
  }
}

void uploadToThingSpeak(float temp, float humidity) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, skipping upload");
    return;
  }

  HTTPClient http;
  String url = "http://api.thingspeak.com/update?api_key="
             + String(THINGSPEAK_API_KEY)
             + "&field1=" + String(temp)
             + "&field2=" + String(humidity);

  http.begin(url);
  int httpCode = http.GET();

  if (httpCode > 0) {
    Serial.print("ThingSpeak upload OK, HTTP ");
    Serial.println(httpCode);
  } else {
    Serial.print("ThingSpeak upload failed, error: ");
    Serial.println(http.errorToString(httpCode).c_str());
  }

  http.end();
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  connectWiFi();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    Serial.println("DHT11 read failed");
  } else {
    Serial.print("Temp: ");
    Serial.print(t);
    Serial.print(" C  Humidity: ");
    Serial.print(h);
    Serial.println(" %");

    uploadToThingSpeak(t, h);
  }

  delay(60000);
}
