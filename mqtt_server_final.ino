#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESP8266mDNS.h>
#include <LittleFS.h>
#include <PubSubClient.h>

#define LED_PIN 2  // Built-in LED pin (GPIO2 on NodeMCU)

// Wi-Fi credentials
const char* ssid = "kabaddi_drone";
const char* password = "robodrone";

// Static IP configuration
IPAddress local_IP(192, 168, 1, 200);    // Set your static IP
IPAddress gateway(192, 168, 1, 1);      // Set your gateway
IPAddress subnet(255, 255, 255, 0);     // Set your subnet mask

// MQTT settings
const char* mqttBroker = "192.168.1.199";
const char* scoreTopic = "kabaddi/score";
const char* thresholdTopic = "kabaddi/rssi_threshold";
const char* buttonTopic = "kabaddi/button";

// Network clients
WiFiClient espClient;
PubSubClient mqttClient(espClient);

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

String scores = "";
int totalScore = 0;
String attackerName = "DefaultAttacker";
String defender1Name = "Defender1";
String defender2Name = "Defender2";
String defender3Name = "Defender3";

String currentDefenderName = "";
String targetIp = "";

// Default RSSI threshold
int rssiThreshold = -38;

// Function prototypes
void reconnectMqtt();
void callback(char* topic, byte* payload, unsigned int length);

String getScoresHtml() {
    return "<tr><th>Attacker</th><th>Defender</th></tr>" + scores;
}

String getScoreHtml() {
    String html = "<!DOCTYPE html><html><head><title>Kabaddi Scoring</title>"
                "<style>body {font-family: Arial, sans-serif; background-color: #f4f7f6; display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; flex-direction: column;} "
                "h1 {color: #0073e6; text-align: center;} "
                "form {text-align: center; margin-bottom: 20px;} "
                "input[type='text'], input[type='submit'] {padding: 10px; margin: 5px; border: 1px solid #0073e6; border-radius: 5px;} "
                "input[type='submit'] {background-color: #0073e6; color: white; cursor: pointer;} "
                "input[type='submit']:hover {background-color: #005bb5;} "
                "table {width: 100%; border-collapse: collapse;} "
                "th, td {border: 1px solid #0073e6; padding: 10px; text-align: center;} "
                "th {background-color: #0073e6; color: white;} "
                "tr:nth-child(even) {background-color: #f2f2f2;} "
                "#scoreTableContainer {max-height: 300px; overflow-y: auto; width: 80%;} "
                ".sticky {position: -webkit-sticky; position: sticky; top: 0; background-color: #f4f7f6; text-align: center;} "
                "#totalScoreContainer {text-align: center; margin-top: 20px;} "
                "</style></head><body>"
                "<div class='sticky'>"
                "<h1>Kabaddi Scoring System</h1>"
                "<form action='/set_attacker' method='POST'>"
                "<label for='attacker'>Attacker Name:</label>"
                "<input type='text' id='attacker' name='attacker' value='" + attackerName + "'>"
                "<input type='submit' value='Set'>"
                "</form>"
                "<form action='/set_defenders' method='POST'>"
                "<label for='defender1'>Defender 1 Name:</label>"
                "<input type='text' id='defender1' name='defender1' value='" + defender1Name + "'>"
                "<label for='defender2'>Defender 2 Name:</label>"
                "<input type='text' id='defender2' name='defender2' value='" + defender2Name + "'>"
                "<label for='defender3'>Defender 3 Name:</label>"
                "<input type='text' id='defender3' name='defender3' value='" + defender3Name + "'>"
                "<input type='submit' value='Set Defenders'>"
                "</form>"
                "<form action='/set_threshold' method='POST'>"
                "<label for='rssiThreshold'>Set RSSI Threshold:</label>"
                "<input type='text' id='rssiThreshold' name='rssiThreshold' value='" + String(rssiThreshold) + "'>"
                "<input type='submit' value='Set Threshold'>"
                "</form>"
                "<form action='/reset' method='GET'>"
                "<input type='submit' value='Reset Scores'>"
                "</form>"
                "</div>"
                "<div id='totalScoreContainer'><h2>Total Score: <span id='totalScore'>" + String(totalScore) + "</span></h2></div>"
                "<div id='scoreTableContainer'><table id='scoreTable'>" + getScoresHtml() + "</table></div>"
                "<audio id='airHorn' src='/dj.wav' preload='auto'></audio>"
                "<script>"
                "var webSocket = new WebSocket('ws://' + window.location.hostname + '/ws');"
                "webSocket.onmessage = function(event) {"
                "  var data = event.data.split('|totalScore|');"
                "  if (data.length > 1) {"
                "    document.getElementById('scoreTable').innerHTML = data[0];"
                "    document.getElementById('totalScore').innerText = data[1];"
                "    var airHorn = document.getElementById('airHorn');"
                "    if (airHorn) {"
                "      airHorn.play().catch(error => {"
                "        console.error('Error playing sound:', error);"
                "      });"
                "    }"
                "  }"
                "};"
                "</script>"
                "</body></html>";
    return html;
}

void handleRoot(AsyncWebServerRequest *request) {
    request->send(200, "text/html", getScoreHtml());
}

void handleSetAttacker(AsyncWebServerRequest *request) {
    if (request->hasParam("attacker", true)) {
        attackerName = request->getParam("attacker", true)->value();
    }
    request->redirect("/");
}

void handleSetDefenders(AsyncWebServerRequest *request) {
    if (request->hasParam("defender1", true)) {
        defender1Name = request->getParam("defender1", true)->value();
    }
    if (request->hasParam("defender2", true)) {
        defender2Name = request->getParam("defender2", true)->value();
    }
    if (request->hasParam("defender3", true)) {
        defender3Name = request->getParam("defender3", true)->value();
    }
    request->redirect("/");
}

void handleSetThreshold(AsyncWebServerRequest *request) {
    if (request->hasParam("rssiThreshold", true)) {
        String thresholdParam = request->getParam("rssiThreshold", true)->value();
        int newThreshold = thresholdParam.toInt();
        if (newThreshold < 0) {
            rssiThreshold = newThreshold;
            Serial.printf("Updated RSSI threshold to: %d\n", rssiThreshold);
            mqttClient.publish(thresholdTopic, String(rssiThreshold).c_str());
        }
    }
    request->redirect("/");
}

void handleReset(AsyncWebServerRequest *request) {
    totalScore = 0;
    scores = "";
    ws.textAll(getScoresHtml() + "|totalScore|" + String(totalScore));
    request->redirect("/");
}

void connectToWiFi() {
    if (!WiFi.config(local_IP, gateway, subnet)) {
        Serial.println("Failed to configure Static IP");
    }
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWi-Fi Connected");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
    String message = "";
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }

    if (message == "11") {
       
        currentDefenderName = defender1Name;
    } else if (message == "22") {
       
        currentDefenderName = defender2Name;
    } else if (message == "33") {
        
        currentDefenderName = defender3Name;
    } else {
        return;
    }

    int currentRssi = -15;  // Simulating RSSI value
    if (currentRssi > rssiThreshold) {
        scores += "<tr><td>" + attackerName + "</td><td>" + currentDefenderName + "</td></tr>";
        ws.textAll(getScoresHtml() + "|totalScore|" + String(++totalScore));
    }
}

void reconnectMqtt() {
    while (!mqttClient.connected()) {
        Serial.print("Attempting MQTT connection...");
        if (mqttClient.connect("KabaddiClient")) {
            Serial.println("connected");
            mqttClient.subscribe(buttonTopic);
        } else {
            Serial.print("failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" retrying in 5 seconds");
            delay(5000);
        }
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);

    if (!LittleFS.begin()) {
        Serial.println("Failed to mount file system");
        return;
    }

    connectToWiFi();
    mqttClient.setServer(mqttBroker, 1883);
    mqttClient.setCallback(callback);

    server.on("/", HTTP_GET, handleRoot);
    server.on("/set_attacker", HTTP_POST, handleSetAttacker);
    server.on("/set_defenders", HTTP_POST, handleSetDefenders);
    server.on("/set_threshold", HTTP_POST, handleSetThreshold);
    server.on("/reset", HTTP_GET, handleReset);

    server.on("/dj.wav", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/dj.wav", "audio/wav");
    });

    ws.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {});
    server.addHandler(&ws);
    server.begin();
}

void loop() {
    if (!mqttClient.connected()) {
        reconnectMqtt();
    }
    mqttClient.loop();
}
