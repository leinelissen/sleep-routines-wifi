#include "sleep_routine_events.h"
#include <Arduino.h>
#include "network_layer.h"
#include "config.h"

// Hall sensor 
#define HALL_SENSOR_PIN A7
#define HALL_SENSOR_UPPER_BOUND 2000
#define HALL_SENSOR_LOWER_BOUND 1940
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

void handleMQTTEvent(const char* event, JsonDocument &doc) {
    // Log freshly received event
    Serial.print("Received new event: ");
    Serial.println(event);
}

void setup() {
    Serial.begin(115200);

    pinMode(HALL_SENSOR_PIN, INPUT);

    // Connect to WiFi and MQTT
    connectWiFi(WIFI_IDENTITY, WIFI_PASSWORD, WIFI_SSID);
    mqtt_client = connectMQTT(MQTT_HOST, MQTT_USER, MQTT_PASSWORD, handleMQTTEvent);

    // Also post a message to signal connection
    String payload = "{\"event\": \"" SR_EVENT_DEVICE_CONNECTED "\", \"deviceType\": \"" SR_DEVICE_TYPE "\", \"deviceUuid\": \"" + WiFi.macAddress(); + "\"}";
    mqtt_client->publish("/sleep-routines", payload);

    for (int i = 0; i < numberOfReadings; i++) {
        sensor.readings[i] = 1965;
    }
}

void loop() {
    networkLayerLoop();

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
