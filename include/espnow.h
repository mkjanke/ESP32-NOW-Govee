#ifndef ESPNOW_H
#define ESPNOW_H

#include <ArduinoJson.h>
#include "settings.h"
#include <WiFi.h>
#include <esp_now.h>

bool initEspNow();
bool espNowSend(const std::string &);
bool espNowSend(const char *);
bool espNowSend(const JsonDocument &);

#endif