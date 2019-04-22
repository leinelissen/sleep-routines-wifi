#include "sleep_routine_events.h"
#include <Arduino.h>
#include "network_layer.h"
#include "config.h"
#include <Preferences.h>

// Hall sensor 
#define HALL_SENSOR_PIN A4
#define HALL_SENSOR_UPPER_BOUND 2000
#define HALL_SENSOR_LOWER_BOUND 1700
const int numberOfReadings = 10;

struct Sensor {
    int i = 0;
    int readings[numberOfReadings];
    unsigned int total = 19650;
    int average = 1965;
    bool active;
};

Sensor sensor;

// MQTT Server
MQTTClient *mqtt_client;

// Interval
unsigned int interval;
Preferences preferences;

void setInterval(unsigned int newInterval) {
    // Store in Flash memory
    preferences.putUInt("interval", newInterval);

    // Store new value in device
    interval = newInterval;

    Serial.print("Stored new interval: ");
    Serial.println(interval);
}

void readInterval() {
    // Init Preferences library
    preferences.begin("sr", false);

    // Read byte from EEPROM and calculate interval from it
    interval = preferences.getUInt("interval");

    Serial.print("Read interval from memory: ");
    Serial.println(interval);
}

void handleMQTTEvent(const char* event, JsonDocument &doc) {
    // Log freshly received event
    Serial.print("Received new event: ");
    Serial.println(event);

    // Switch for the different event types
    if (strcmp(event, SR_EVENT_UPDATE_INTERVAL) == 0) {
        if (doc["sombreroId"].as<unsigned int>() == SOMBRERO_ID) {
            // Store new interval
            setInterval(doc["interval"].as<unsigned int>());

            // Publish acknowledge message
            String payload = "{\"event\": \"updateIntervalAcknowledge\", \"deviceType\": \"" SR_DEVICE_TYPE "\", \"deviceUuid\": \"" + WiFi.macAddress() + "\", \"sombreroId\":" + SOMBRERO_ID + "}";
            mqtt_client->publish("/sleep-routines", payload);
        }
    }
}

void setup() {
    // Setup serial connection
    Serial.begin(115200);

    // Perpare sensor input pin
    pinMode(HALL_SENSOR_PIN, INPUT);

    // Read the interval length from flash
    readInterval();

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

    for (int i = 0; i < numberOfReadings; i++) {
        sensor.readings[i] = 1965;
    }
}

void sensorLoop() {
    // Read the voltage on the hall sensor
    // NOTE: The analogRead function has a 12-bit (4096-value) discretion, and
    // the nominal voltage is 3.3V. The hall sensor outputs a voltage that is
    // half the supply voltage when not excited. Given the 3.3V supply voltage,
    // this comes down to about 1.65V, which is about 2048 when output by the
    // analogRead function. Anything significantly above or below this means
    // that a magnet is closeby. 
    // Nominal values for the analog read value are around 1960-1970
    sensor.total -= sensor.readings[sensor.i];
    sensor.readings[sensor.i] = analogRead(HALL_SENSOR_PIN);
    sensor.total += sensor.readings[sensor.i];
    sensor.average = sensor.total / numberOfReadings;
    sensor.i += 1;

    if (sensor.i >= numberOfReadings) {
        sensor.i = 0;
    }

    sensor.active = sensor.average > HALL_SENSOR_UPPER_BOUND || sensor.average < HALL_SENSOR_LOWER_BOUND;

    delay(1);
}

void loop() {
    networkLayerLoop();
    sensorLoop();
}
