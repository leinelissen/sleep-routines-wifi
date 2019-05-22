#include "sleep_routine_events.h"
#include <Arduino.h>
#include "network_layer.h"
#include "actor/hourglass.h"
#include "actor/config.h"

// MQTT Server
MQTTClient *mqtt_client;

// Hourglass
#define SERVO_PIN 12
#define TILT_PIN 14
#define TILT_MIN_COUNT 50000
Hourglass hourglass;

float batteryVoltage;
bool isActorUpright = false;
bool tiltRead = false;
unsigned int tiltCounter = 0;

void handleMQTTEvent(const char* event, JsonDocument &doc) {
    // Log freshly received event
    Serial.print("Received new event: ");
    Serial.println(event);

    // Switch for the different event types
    if (strcmp(event, SR_EVENT_DEVICE_DETECTED_COUPLING) == 0) {
        if (doc["sombreroId"].as<unsigned int>()) {
            Serial.println("Received last sombreroDidConnect event. Opening up definitively...");
            hourglass.start();
        } else {
            Serial.println("Received sombreroDidConnect event. Starting sequence...");
            Serial.println(doc["interval"].as<unsigned int>());
            hourglass.start(doc["interval"].as<unsigned int>());
        }
    } else if (strcmp(event, SR_EVENT_DEVICE_DETECTED_DECOUPLING) == 0) {
        hourglass.stop();
    }
}

void setup() {
    Serial.begin(115200);

    // Connect to WiFi and MQTT
#ifdef WIFI_IDENTITY
    connectWiFi(WIFI_IDENTITY, WIFI_PASSWORD, WIFI_SSID);
#else
    connectWiFi(WIFI_PASSWORD, WIFI_SSID);
#endif    

    mqtt_client = connectMQTT(MQTT_HOST, MQTT_USER, MQTT_PASSWORD, handleMQTTEvent);

    // Also post a message to signal connection
    String payload = "{\"event\": \"" SR_EVENT_DEVICE_CONNECTED "\", \"deviceType\": \"" SR_DEVICE_TYPE "\", \"deviceUuid\": \"" + WiFi.macAddress() + "\"}";
    mqtt_client->publish("/sleep-routines", payload);

    // Init hourglass class
    hourglass.init(SERVO_PIN, mqtt_client);

    // Determine battery voltage
    pinMode(A13, INPUT);
    batteryVoltage = 2.0 * (analogRead(A13) / 4095.0) * 3.3;
    Serial.print("Battery voltage: ");
    Serial.println(batteryVoltage);
    Serial.println("Application initialised");

    // Initialise tilt sensor
    pinMode(TILT_PIN, INPUT);
    // We set the actor status to the inverse of the sensor, so that we can
    // reliably trigger the tilt event for the sensor loop
    isActorUpright = !tiltRead;
}

void sensorLoop() {
    // Read tilt sensor
    tiltRead = digitalRead(TILT_PIN);

    if ((!isActorUpright && tiltRead) || (isActorUpright && !tiltRead)) {
        // If isActorUpright and tiltRead don't match, increase the counter by
        // one. This is done so that the event isn't invoked immediately.
        tiltCounter += 1;
    } else {
        // If not, the tiltCounter starts from scratch
        tiltCounter = 0;
    }

    // If a critical mass is reached, we change the actual value
    if (tiltCounter > TILT_MIN_COUNT) {
        // Open and close hourglass accordingly
        if (isActorUpright && !tiltRead) {
            hourglass.start();
        } else if (!isActorUpright && tiltRead) {
            hourglass.stop();
        }

        // Set new variables
        isActorUpright = tiltRead;
        tiltCounter = 0;

        // Send message
        String payload = "{\"event\": \"" SR_EVENT_DEVICE_TILTED "\", \"deviceType\": \"" SR_DEVICE_TYPE "\", \"deviceUuid\": \"" + WiFi.macAddress() + "\", \"isUpright\":" + isActorUpright + "}";
        mqtt_client->publish("/sleep-routines", payload);
    }
}

void loop() {
    networkLayerLoop();
    hourglass.loop();
    sensorLoop();
}
