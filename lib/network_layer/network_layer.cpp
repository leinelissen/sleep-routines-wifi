#include "network_layer.h"

// WiFi connection attempt counter
int counter = 0;

// All instantiated classes
WiFiSettings wifiSettings;
WiFiClient net;
MQTTClient *client;
StaticJsonDocument<256> doc;

// A pointer to a suppled MQTT event handling function
void (*mqttEventHandler)(const char*, JsonDocument&);

/**
 * Connect to the WiFi network
 */
void connectWiFi(const char* identity, const char* password, const char* ssid) {
    Serial.println();
    Serial.print("Connecting to network: ");
    Serial.println(ssid);
    wifiSettings = { identity, password, ssid };

    WiFi.disconnect(true);                                                             //disconnect form wifi to set new wifi connection
    WiFi.mode(WIFI_STA);                                                               //init wifi mode
    
    esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)identity, strlen(identity)); //provide identity
    esp_wifi_sta_wpa2_ent_set_username((uint8_t *)identity, strlen(identity)); //provide username --> identity and username is same
    esp_wifi_sta_wpa2_ent_set_password((uint8_t *)password, strlen(password)); //provide password
    esp_wpa2_config_t config = WPA2_CONFIG_INIT_DEFAULT();                             //set config settings to default
    esp_wifi_sta_wpa2_ent_enable(&config);                                             //set config settings to enable function
    WiFi.begin(ssid);                                                                  //connect to wifi
   
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
        counter++;
        if (counter >= 60)
        { //after 30 seconds timeout - reset board
            ESP.restart();
        }
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address set: ");
    Serial.println(WiFi.localIP()); //print LAN IP
}

/**
 * This starts a recurring task to monitor that WiFi is still working
 */
void wiFiLoop() {
    // This apparently increases stability
    delay(10);

    if (WiFi.status() == WL_CONNECTED) {
        counter = 0;
    } else if (WiFi.status() != WL_CONNECTED) { 
        // if we lost connection, retry
        WiFi.begin(wifiSettings.ssid);
    }
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
        counter++;
        if (counter >= 60) {
            ESP.restart();
        }
    }

    delay(100);
}

/**
 * Process a message that has been received from MQTT
 */
void messageReceived(String &topic, String &payload) {
    // Log raw message
    Serial.println("incoming: " + topic + " - " + payload);

    // Parse the JSON that was received
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        Serial.print(F("deserializeJson() failed with code ")); 
        Serial.println(err.c_str());
    }

    // Pass the event onto an event handler function
    mqttEventHandler(doc["event"], doc);
}

/**
 * Connecto to the MQTT network over the established WiFi connection
 */
MQTTClient * connectMQTT(const char* host, const char* username, const char* password, void (*func)(const char*, JsonDocument&)) {
    // Create client and set message handler
    client = new MQTTClient();
    client->begin(host, net);
    client->onMessage(messageReceived);
    mqttEventHandler = func;

    // Try and connect the client
    while (!client->connect("arduino", username, password)) {
        Serial.print(".");
        delay(1000);
    }

    // If the connection is successful, connect to the appropriate MQTT channel;
    client->subscribe("/sleep-routines");

    return client;
}

void networkLayerLoop() {
    client->loop();
    wiFiLoop();
}