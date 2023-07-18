#ifndef ESPNOW_H
#define ESPNOW_H

#include <esp_now.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "settings.h"

bool initEspNow();
bool espNowSend(const String&);
bool espNowSend(const JsonDocument&);

#endif