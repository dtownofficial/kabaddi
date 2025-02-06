#include <WiFi.h>
#include <PubSubClient.h>  // Changed to PubSubClient for simpler MQTT handling
#include <BLEDevice.h>
#include <BLEScan.h>

// WiFi credentials
const char* ssid = "kabaddi_drone";
const char* password = "robodrone";

// MQTT broker details
const char* mqttBroker = "192.168.1.199";
const uint16_t mqttPort = 1883;

// MQTT topics
const char* mqttTopic = "kabaddi/rssi";
const char* buttonTopic = "kabaddi/button";
const char* thresholdTopic = "kabaddi/rssi_threshold";

#define LED_PIN 2
#define DEBOUNCE_TIME 2000

// Global variables
BLEScan* pBLEScan;
WiFiClient espClient;
PubSubClient mqttClient(espClient);
volatile int rssiThreshold = -40;

// Structure for device detection timing
struct DeviceTimer {
    String address;
    unsigned long lastDetection;
};

DeviceTimer deviceTimers[4];

// List of target MAC addresses
const char* targetMacAddresses[] = {
    "e0:e2:e6:63:04:06",
    "1c:69:20:94:49:8e",
    "1c:69:20:92:9c:da",
    "e0:e2:e6:63:22:ea"
};
const int numMacAddresses = sizeof(targetMacAddresses) / sizeof(targetMacAddresses[0]);

// MQTT callback
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    if (strcmp(topic, thresholdTopic) == 0) {
        char message[length + 1];
        memcpy(message, payload, length);
        message[length] = '\0';
        int newThreshold = atoi(message);
        
        if (newThreshold < 0) {
            rssiThreshold = newThreshold;
            Serial.printf("Updated RSSI threshold: %d\n", rssiThreshold);
        }
    }
}

// Reconnect to MQTT broker
void reconnectMQTT() {
    while (!mqttClient.connected()) {
        Serial.println("Attempting MQTT connection...");
        if (mqttClient.connect("ESP32_BLE_Scanner")) {
            Serial.println("Connected to MQTT broker");
            mqttClient.subscribe(thresholdTopic);
        } else {
            Serial.println("Failed to connect to MQTT broker, retrying in 2 seconds");
            delay(2000);
        }
    }
}

// Callback class for BLE scan results
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        String macAddress = advertisedDevice.getAddress().toString().c_str();
        int rssi = advertisedDevice.getRSSI();
        unsigned long currentTime = millis();

        for (int i = 0; i < numMacAddresses; i++) {
            if (macAddress.equals(targetMacAddresses[i])) {
                if (currentTime - deviceTimers[i].lastDetection >= DEBOUNCE_TIME) {
                    if (rssi > rssiThreshold) {
                        Serial.printf("Device found! MAC: %s, RSSI: %d\n", macAddress.c_str(), rssi);
                        deviceTimers[i].lastDetection = currentTime;
                        digitalWrite(LED_PIN, HIGH);

                        // Ensure MQTT connection
                        if (!mqttClient.connected()) {
                            reconnectMQTT();
                        }

                        // Publish RSSI message
                       
                        // Publish button message
                        String buttonMessage = String((i + 1) * 11);
                        mqttClient.publish(buttonTopic, buttonMessage.c_str());
                        Serial.printf("Published Button: %s\n", buttonMessage.c_str());

                        delay(1000); // Short delay between publishes
                        digitalWrite(LED_PIN, LOW);
                    }
                }
                break;
            }
        }
    }
};

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    // Initialize device timers
    for (int i = 0; i < numMacAddresses; i++) {
        deviceTimers[i].address = String(targetMacAddresses[i]);
        deviceTimers[i].lastDetection = 0;
    }

    // Connect to WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected to WiFi");

    // Configure MQTT client
    mqttClient.setServer(mqttBroker, mqttPort);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setKeepAlive(60);
    reconnectMQTT();

    // Initialize BLE
    BLEDevice::init("");
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), true);
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(0x30);
    pBLEScan->setWindow(0x30);
}

void loop() {
    if (!mqttClient.connected()) {
        reconnectMQTT();
    }
    mqttClient.loop();
    
    pBLEScan->start(1, false);
    delay(100);
}