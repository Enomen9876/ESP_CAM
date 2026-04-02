#ifndef WIFI_SELECTOR_H
#define WIFI_SELECTOR_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>

void connectWiFiSelector(const char* fallbackSSID, const char* fallbackPassword,
                         const char* prioritySSIDs[], const char* priorityPasswords[], size_t n);

#endif