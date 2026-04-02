#include "WIFI_selector.h"
#include <WiFi.h>
#include <ESPmDNS.h>

static const char* MDNS_NAME = "esp-kamera";

// -------------------- mDNS --------------------
void startMDNS() {
    if (MDNS.begin(MDNS_NAME)) {
        MDNS.addService("http", "tcp", 80);
        Serial.println("mDNS started: http://" + String(MDNS_NAME) + ".local");
    } else {
        Serial.println("mDNS failed to start");
    }
}

// -------------------- WiFi Selector --------------------
void connectWiFiSelector(const char* fallbackSSID, const char* fallbackPassword,
                         const char* prioritySSIDs[], const char* priorityPasswords[], size_t n)
{
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(MDNS_NAME);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
    WiFi.setSleep(false);  // important for stable camera streaming

    // -------------------- Event Handler --------------------
    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info){
        switch(event) {
            case WIFI_EVENT_STA_DISCONNECTED:
                Serial.println("WiFi lost. Reconnecting...");
                WiFi.reconnect();
                break;

            case IP_EVENT_STA_GOT_IP:
                Serial.println("Reconnected! IP: " + WiFi.localIP().toString());
                startMDNS();  // restart mDNS after reconnect
                break;

            default:
                break;
        }
    });

    // -------------------- Main scan & connect loop --------------------
    while (true) {   // 🔁 never stop trying
        Serial.println("\nScanning for Wi-Fi networks...");
        int nNetworks = WiFi.scanNetworks();

        String selectedSSID = "";
        String selectedPassword = "";
        int bestRSSI = -1000;

        // 1️⃣ Check fallback first
        for (int i = 0; i < nNetworks; i++) {
            if (WiFi.SSID(i) == fallbackSSID) {
                selectedSSID = fallbackSSID;
                selectedPassword = fallbackPassword;
                Serial.println("Fallback found: " + selectedSSID);
                break;
            }
        }

        // 2️⃣ Priority list (choose strongest signal)
        if (selectedSSID.length() == 0) {
            for (int i = 0; i < nNetworks; i++) {
                String ssidName = WiFi.SSID(i);

                for (size_t j = 0; j < n; j++) {
                    if (ssidName == prioritySSIDs[j]) {
                        int rssi = WiFi.RSSI(i);
                        if (rssi > bestRSSI) {
                            bestRSSI = rssi;
                            selectedSSID = ssidName;
                            selectedPassword = priorityPasswords[j];
                        }
                    }
                }
            }
        }

        if (selectedSSID.length() == 0) {
            Serial.println("No preferred Wi-Fi found. Waiting 5s...");
            delay(5000);
            continue;   // 🔁 retry scan forever
        }

        Serial.println("Connecting to: " + selectedSSID);
        WiFi.begin(selectedSSID.c_str(), selectedPassword.c_str());

        unsigned long startAttemptTime = millis();

        while (WiFi.status() != WL_CONNECTED &&
               millis() - startAttemptTime < 15000)
        {
            delay(500);
            Serial.print(".");
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nWi-Fi connected!");
            Serial.println("IP: " + WiFi.localIP().toString());
            startMDNS();
            break;  // exit scan loop
        } else {
            Serial.println("\nConnection failed. Retrying...");
            WiFi.disconnect(true);
            delay(3000);
        }
    }
}