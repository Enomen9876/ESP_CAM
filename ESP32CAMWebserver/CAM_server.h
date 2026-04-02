#ifndef CAMERA_SERVER_H
#define CAMERA_SERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include "esp_camera.h"
#include "esp_http_server.h"

// ===== Public API =====

void initCamera();
void startCameraServer();
void addDebug(const String& msg);

#endif // CAMERA_SERVER_H
