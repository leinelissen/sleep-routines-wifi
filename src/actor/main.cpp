#include "sleep_routine_events.h"
#include <Arduino.h>
#include "network_layer.h"
#include "actor/hourglass.h"
#include "actor/config.h"

// MQTT Server
MQTTClient *mqtt_client;

// Hourglass
#define SERVO_PIN 12
Hourglass hourglass;

float batteryVoltage;

void handleMQTTEvent(const char* event, JsonDocument &doc) {
    // Log freshly received event
    Serial.print("Received new event: ");
    Serial.println(event);

    // Switch for the different event types
    if (strcmp(event, SR_EVENT_DEVICE_DETECTED_COUPLING) == 0) {
        Serial.println("Received sombreroDidConnect event. Starting sequence...");
        Serial.println(doc["interval"].as<unsigned int>());
        hourglass.start(doc["interval"].as<unsigned int>());
    } else if (strcmp(event, SR_EVENT_DEVICE_DETECTED_DECOUPLING) == 0) {
        hourglass.stop();
        mqtt_client->publish("/sleep-routines", "{}");
    }
}

void setup() {
    Serial.begin(115200);

    // Connect to WiFi and MQTT
    connectWiFi(WIFI_IDENTITY, WIFI_PASSWORD, WIFI_SSID);
    mqtt_client = connectMQTT(MQTT_HOST, MQTT_USER, MQTT_PASSWORD, handleMQTTEvent);

    // Also post a message to signal connection
    String payload = "{\"event\": \"" SR_EVENT_DEVICE_CONNECTED "\", \"deviceType\": \"" SR_DEVICE_TYPE "\", \"deviceUuid\": \"" + WiFi.macAddress(); + "\"}";
    mqtt_client->publish("/sleep-routines", payload);

    // Init hourglass class
    hourglass.init(SERVO_PIN, mqtt_client);

    // Determine battery voltage
    pinMode(A13, INPUT);
    batteryVoltage = 2.0 * (analogRead(A13) / 4095.0) * 3.3;
    Serial.print("Battery voltage: ");
    Serial.println(batteryVoltage);
    Serial.println("Application initialised");
}

void loop() {
    networkLayerLoop();
    hourglass.loop();
}
