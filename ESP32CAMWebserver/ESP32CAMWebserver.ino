#include <Arduino.h>

#include "password.h"
#include "CAM_server.h"
#include "WIFI_selector.h"


const char* fallbackSSID = Fssid;
const char* fallbackPassword = Fpassword;

// Priority list
const char* prioritySSIDs[] = {ssid};
const char* priorityPasswords[] = {password};

void setup() {
  Serial.begin(115200);

  connectWiFiSelector(fallbackSSID, fallbackPassword, prioritySSIDs, priorityPasswords, 2);

  initCamera();
  startCameraServer();
}

void loop() {
  delay(100);
}
