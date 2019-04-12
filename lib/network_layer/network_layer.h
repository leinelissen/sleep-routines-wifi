#ifndef NETWORK_LAYER_H_   /* Include guard */
#define NETWORK_LAYER_H_

#include <WiFi.h> 
#include <MQTT.h>
#include <ArduinoJson.h>
#include "esp_wpa2.h"
#include "sleep_routine_events.h"

struct WiFiSettings {
    const char* username;
    const char* password;
    const char* ssid;
};

void connectWiFi(const char*, const char*, const char*);
void wiFiLoop();
void messageReceived(String&, String&);
MQTTClient * connectMQTT(const char*, const char*, const char*, void (*)(const char*, JsonDocument&));
void networkLayerLoop();

#endif
