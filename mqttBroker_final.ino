/**
 * @file simpleMqttBroker.ino
 * @author Alex Cajas
 * @brief 
 * Simple example of using this MQTT Broker
 * @version 1.0.0
 */

#include <WiFi.h> 
#include "EmbeddedMqttBroker.h"
using namespace mqttBrokerName;

const char *ssid = "kabaddi_drone";
const char *password = "robodrone";

// Static IP configuration
IPAddress local_IP(192, 168, 1, 199); // Desired static IP address
IPAddress gateway(192, 168, 1, 1);    // Gateway IP
IPAddress subnet(255, 255, 255, 0);  // Subnet mask

/******************* MQTT broker ********************/
uint16_t mqttPort = 1883;
MqttBroker broker(mqttPort);

void setup() {

  /**
   * @brief To see outputs of broker activity 
   * (message to publish, new client's id etc...), 
   * set your core debug level higher to NONE (I recommend INFO level).
   * More info: @link https://github.com/alexCajas/EmbeddedMqttBroker @endlink
   */

  Serial.begin(115200);
  Serial.println();
  Serial.println();
  Serial.println("Simple MQTT broker");
  Serial.print("Connecting to ");
  Serial.println(ssid);

  // Configure static IP
  if (!WiFi.config(local_IP, gateway, subnet)) {
    Serial.println("Failed to configure static IP");
  }

  // Connect to WiFi network
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");

  // Start the MQTT broker
  broker.setMaxNumClients(9); // Set according to your system
  broker.startBroker();
  Serial.println("Broker started");

  // Print the IP address
  Serial.print("Use this IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("And this port: ");
  Serial.println(mqttPort);
  Serial.println("To connect to MQTT broker");
}

void loop() {
  // Add your code here if needed
}
