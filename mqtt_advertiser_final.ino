#include <BLEDevice.h>
#include <WiFi.h>
#include <PubSubClient.h>

// Wi-Fi credentials for connecting to broker AP
const char* ssid = "kabaddi_drone";
const char* password = "robodrone";

// MQTT broker and topic
const char* mqttBroker = "192.168.1.199"; // Broker ESP8266 IP
const char* mqttTopic = "esp32/rssi";
const char* buttonTopic = "kabaddi/button";

#define LED_PIN 2 // Adjust the LED pin for NodeMCU (D4 corresponds to GPIO2)
#define RECONNECT_INTERVAL 5000 // Time in ms to retry connections

WiFiClient espClient;
PubSubClient mqttClient(espClient);
unsigned long lastReconnectAttempt = 0;

// BLE Task Handle
TaskHandle_t bleTaskHandle;

// MQTT Task Handle
TaskHandle_t mqttTaskHandle;

// BLE initialization
void bleTask(void *pvParameters) {
  // Initialize BLE
  BLEDevice::init("ESP32_Simple_Advertise");

  // Print MAC address
  String macAddress = BLEDevice::getAddress().toString().c_str();
  Serial.print("Device MAC Address: ");
  Serial.println(macAddress);

  // Start BLE advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->start();

  Serial.println("BLE advertising started...");

  // Keep the task alive
  while (true) {
    vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay to reduce CPU usage
  }
}

// MQTT message callback
void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.printf("MQTT message received: %s\n", message.c_str());

  if (message == "44") {
    digitalWrite(LED_PIN, HIGH);
    delay(5000);
    digitalWrite(LED_PIN, LOW);
  }
}

// Wi-Fi connection
void connectToWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi connected");
}

// MQTT connection
void connectToMqtt() {
  if (!mqttClient.connected()) {
    Serial.println("Connecting to MQTT...");
    if (mqttClient.connect("NodeMCU_Client")) { // Set unique client ID
      Serial.println("MQTT connected!");
      mqttClient.subscribe(buttonTopic);
    } else {
      Serial.print("MQTT connection failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" retrying...");
    }
  }
}

// MQTT Task
void mqttTask(void *pvParameters) {
  // Connect to Wi-Fi
  connectToWiFi();

  // Set up MQTT
  mqttClient.setServer(mqttBroker, 1883);
  mqttClient.setCallback(onMqttMessage);

  while (true) {
    // Ensure MQTT connection
    if (!mqttClient.connected()) {
      unsigned long now = millis();
      if (now - lastReconnectAttempt > RECONNECT_INTERVAL) {
        lastReconnectAttempt = now;
        connectToMqtt();
      }
    } else {
      mqttClient.loop();
    }
    vTaskDelay(10 / portTICK_PERIOD_MS); // Allow other tasks to run
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Create BLE task
  xTaskCreatePinnedToCore(
    bleTask,         // Task function
    "BLE Task",      // Name of the task
    10000,           // Stack size
    NULL,            // Task parameters
    1,               // Priority
    &bleTaskHandle,  // Task handle
    0                // Core to run the task on
  );

  // Create MQTT task
  xTaskCreatePinnedToCore(
    mqttTask,        // Task function
    "MQTT Task",     // Name of the task
    10000,           // Stack size
    NULL,            // Task parameters
    1,               // Priority
    &mqttTaskHandle, // Task handle
    1                // Core to run the task on
  );
}

void loop() {
  // Nothing here; tasks handle the operation
}
