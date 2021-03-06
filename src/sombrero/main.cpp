#include "sleep_routine_events.h"
#include <Arduino.h>
#include "network_layer.h"
#include "config.h"
#include <Preferences.h>

// Hall sensor 
#define HALL_SENSOR_PIN A4
#define HALL_SENSOR_UPPER_BOUND 2000
#define HALL_SENSOR_LOWER_BOUND 1700
#define HALL_SENSOR_NOMINAL_VALUE 1930
const int numberOfReadings = 25;

struct Sensor {
    int i = 0;
    int readings[numberOfReadings];
    unsigned int total = HALL_SENSOR_NOMINAL_VALUE * numberOfReadings;
    int average = HALL_SENSOR_NOMINAL_VALUE;
    bool active;
};

Sensor sensor; 
bool isDocked = false;

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
            mqtt_client->publish("/sleep-routines", payload, true, 1);
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

    // Connect to WiFi
#ifdef WIFI_IDENTITY
    connectWiFi(WIFI_IDENTITY, WIFI_PASSWORD, WIFI_SSID);
#else
    connectWiFi(WIFI_PASSWORD, WIFI_SSID);
#endif

    // Connect to MQTT
    mqtt_client = connectMQTT(MQTT_HOST, MQTT_USER, MQTT_PASSWORD, handleMQTTEvent);

    // Also post a message to signal connection
    String payload = "{\"event\": \"" SR_EVENT_DEVICE_CONNECTED "\", \"deviceType\": \"" SR_DEVICE_TYPE "\", \"deviceUuid\": \"" + WiFi.macAddress() + "\"}";
    mqtt_client->publish("/sleep-routines", payload);

    // Seed the sensor readings so they don't immediately trigger
    for (int i = 0; i < numberOfReadings; i++) {
        sensor.readings[i] = sensor.average;
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
        // Loop around if the iteration exceeds the proposed number of readings
        sensor.i = 0;

        Serial.println(sensor.average);
    }

    // Calculate whether the sensor is active or not
    sensor.active = sensor.average > HALL_SENSOR_UPPER_BOUND 
        || sensor.average < HALL_SENSOR_LOWER_BOUND;

    // Delay for stability?
    delay(1);
}

void loop() {
    networkLayerLoop();
    sensorLoop();

    // Check if we need to send out messages.
    if (!isDocked && sensor.active) {
        // If the sensor becomes active, change state
        isDocked = true;

        // Send MQTT message
        Serial.println("isDocked changed, sending message...");
        String payload = "{\"event\": \"" SR_EVENT_DEVICE_DETECTED_COUPLING "\", \"deviceType\": \"" SR_DEVICE_TYPE "\", \"deviceUuid\": \"" + WiFi.macAddress() + "\", \"interval\": " + interval + ", \"sombreroId\":" + SOMBRERO_ID + "}";
        mqtt_client->publish("/sleep-routines", payload, false, 1);
    } else if (isDocked && !sensor.active) {
        // If the sensor becomes inactive, change state
        isDocked = false;

        // Send MQTT message
        Serial.println("isDocked changed, sending message...");
        String payload = "{\"event\": \"" SR_EVENT_DEVICE_DETECTED_DECOUPLING "\", \"deviceType\": \"" SR_DEVICE_TYPE "\", \"deviceUuid\": \"" + WiFi.macAddress() + "\"}";
        mqtt_client->publish("/sleep-routines", payload, false, 1);
    }
}
