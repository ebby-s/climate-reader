# Climate Reader

ESP32-C3 firmware that reads temperature and humidity from a DHT11 sensor and uploads readings to ThingSpeak every 60 seconds. Connects to eduroam (WPA2-Enterprise / PEAP) — ideal for university campus deployments.

## Hardware

- **Board:** ESP32-C3-DevKitM-1
- **Sensor:** DHT11 on GPIO pin 2

## Getting Started

1. Install [PlatformIO](https://platformio.org/) (CLI or VS Code extension).
2. Clone the repo.
3. Create `src/secrets.h` (see below) with your Wi-Fi and ThingSpeak credentials.
4. Build and upload:

```bash
pio run -t upload
pio device monitor
```

## `src/secrets.h`

```c
#pragma once

#define WIFI_SSID       "eduroam"
#define EAP_IDENTITY    "user@ institution"
#define EAP_USERNAME    "user@institution"
#define EAP_PASSWORD    "password"
#define THINGSPEAK_API_KEY "your_key_here"
```

This file is gitignored and must be created manually.

## How It Works

1. On boot, the device connects to eduroam using WPA2-Enterprise (PEAP) authentication with time-check disabled.
2. Every 60 seconds it reads temperature (°C) and humidity (%) from the DHT11.
3. Successful readings are sent to ThingSpeak via HTTP GET (`api.thingspeak.com/update`).
4. If Wi-Fi drops, it automatically reconnects.

## Dependencies

Managed by PlatformIO (`platformio.ini`):
- `adafruit/DHT sensor library`
- Arduino framework for ESP32 (built-in: WiFi, HTTPClient, esp_wpa2)
