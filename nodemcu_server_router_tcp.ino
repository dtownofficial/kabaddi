#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESP8266mDNS.h>
#include <LittleFS.h>
#include <WiFiClient.h>

#define LED_PIN 2  // Built-in LED pin (GPIO2 on NodeMCU)

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// TCP settings
WiFiServer tcpServer(12345);  // TCP server port for communication
WiFiClient client;            // TCP client object

String scores = "";
int totalScore = 0;
String attackerName = "DefaultAttacker";
String defender1Name = "Defender1";
String defender2Name = "Defender2";
String defender3Name = "Defender3";
// String defender4Name = "Defender4";

String currentDefenderName = "";
String tcpip = "";

// Default RSSI threshold
int rssiThreshold = -38;

String getScoresHtml() {
    String html = "<tr><th>Attacker</th><th>Defender</th></tr>" + scores;
    return html;
}

String getScoreHtml() {
    String html = "<!DOCTYPE html><html><head><title>Kabaddi Scoring</title>"
                "<style>body {font-family: Arial, sans-serif; background-color: #f4f7f6; display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; flex-direction: column;}"
                "h1 {color: #0073e6; text-align: center;}"
                "form {text-align: center; margin-bottom: 20px;}"
                "input[type='text'], input[type='submit'] {padding: 10px; margin: 5px; border: 1px solid #0073e6; border-radius: 5px;}"
                "input[type='submit'] {background-color: #0073e6; color: white; cursor: pointer;}"
                "input[type='submit']:hover {background-color: #005bb5;}"
                "table {width: 100%; border-collapse: collapse;}"
                "th, td {border: 1px solid #0073e6; padding: 10px; text-align: center;}"
                "th {background-color: #0073e6; color: white;}"
                "tr:nth-child(even) {background-color: #f2f2f2;}"
                "#scoreTableContainer {max-height: 300px; overflow-y: auto; width: 80%;}"
                ".sticky {position: -webkit-sticky; position: sticky; top: 0; background-color: #f4f7f6; text-align: center;}"
                "#totalScoreContainer {text-align: center; margin-top: 20px;}"
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
                // New form for setting RSSI threshold
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

// Handler for setting RSSI threshold and sending it via TCP
void handleSetThreshold(AsyncWebServerRequest *request) {
    if (request->hasParam("rssiThreshold", true)) {
        String thresholdParam = request->getParam("rssiThreshold", true)->value();
        int newThreshold = thresholdParam.toInt();
        if (newThreshold < 0) {
            rssiThreshold = newThreshold;  // Update RSSI threshold
            Serial.printf("Updated RSSI threshold to: %d\n", rssiThreshold);
            
            // Send updated RSSI threshold via TCP to the target device (192.168.1.4)
            if (client.connect("192.168.1.4", 12345)) {
                client.print(rssiThreshold);
                client.stop();
                Serial.println("Sent updated RSSI threshold via TCP.");
            }
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

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    // Handle WebSocket events if needed
}

void setup() {
    Serial.begin(115200);

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);

    if (!LittleFS.begin()) {
        Serial.println("Failed to mount file system");
        return;
    }

    // Connect to the external WiFi network
    WiFi.begin("kabaddi_drone", "robodrone");

    // Set static IP
    IPAddress localIP(192, 168, 1, 2);
    IPAddress gateway(192, 168, 1, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.config(localIP, gateway, subnet);

    // Wait for the connection
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }

    Serial.println("Connected to WiFi");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    // Start TCP server
    tcpServer.begin();

    // Route for handling requests
    server.on("/", HTTP_GET, handleRoot);
    server.on("/set_attacker", HTTP_POST, handleSetAttacker);
    server.on("/set_defenders", HTTP_POST, handleSetDefenders); 
    server.on("/set_threshold", HTTP_POST, handleSetThreshold); // New route for setting RSSI threshold
    server.on("/reset", HTTP_GET, handleReset);

    server.on("/dj.wav", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/dj.wav", "audio/wav");
    });

    ws.onEvent(onWsEvent);
    server.addHandler(&ws);
    server.begin();
}

void loop() {
    // Check for incoming TCP connections
    WiFiClient newClient = tcpServer.available();
    if (newClient) {
        String incomingMessage = newClient.readString();
        newClient.stop();
        Serial.printf("Received message: %s\n", incomingMessage.c_str());

        // Simulating RSSI check (replace this with actual RSSI fetching logic)
        int currentRssi = -15; // Example RSSI value, replace with actual RSSI reading logic

        // Update currentDefenderName based on received TCP message
        if (incomingMessage == "11") {
            tcpip = "192.168.1.3";
            currentDefenderName = defender1Name;
        } else if (incomingMessage == "22") {
            tcpip = "192.168.1.6";
            currentDefenderName = defender2Name;
        } else if (incomingMessage == "33") {
            tcpip = "192.168.1.7";
            currentDefenderName = defender3Name;
        } else {
            return;  // Ignore other messages
        }
        Serial.println("Received Defender: " + currentDefenderName);
        if (currentRssi > rssiThreshold) {
            scores += "<tr><td>" + attackerName + "</td><td>" + currentDefenderName + "</td></tr>";
            totalScore++;
            ws.textAll(getScoresHtml() + "|totalScore|" + String(totalScore));


            digitalWrite(LED_PIN, LOW);
            delay(3000);
            digitalWrite(LED_PIN, HIGH);
           
        } else {
            Serial.printf("Score not registered. Current RSSI (%d) is below the threshold (%d).\n", currentRssi, rssiThreshold);
        }
    }
}
