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

// MQTT message callback
void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.printf("MQTT message received: %s\n", message.c_str());

  if (message == "11") {
    digitalWrite(LED_PIN, HIGH);
    delay(1000);
    digitalWrite(LED_PIN, LOW);
  }
}

// Wi-Fi connection function
void connectToWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi connected");
}

// MQTT connection function
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

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Connect to Wi-Fi
  connectToWiFi();

  // Set up MQTT
  mqttClient.setServer(mqttBroker, 1883);
  mqttClient.setCallback(onMqttMessage);
}

void loop() {
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
}
