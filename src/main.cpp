#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>

#include "Settings.h"

#define RELAY D1
#define ENABLE_DELAY 100
#define MQTT_DELAY 100
const char *topic_target_1 = "office/usb/target/1";
const char *topic_target_2 = "office/usb/target/2";

const char *ssid = WIFI_SSID;
const char *password = WIFI_PSK;
const char *mqtt_broker = MQTT_BROKER;
const char *mqtt_user = MQTT_USER;
const char *mqtt_password = MQTT_PASSWORD;

const char *ota_hostname = OTA_HOSTNAME;
const char *ota_username = OTA_USERNAME;
const char *ota_password = OTA_PASSWORD;
const char *firmware_version = "1.1.1";

WiFiClient espClient;
PubSubClient client(espClient);
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

// Default active USB target is 1
int activeTarget = 1;

void switchRelay() {
  digitalWrite(RELAY, HIGH);
  delay(ENABLE_DELAY);
  digitalWrite(RELAY, LOW);
}

void publishTopics() {
  if(activeTarget == 1) {
    // Target 1 is selected
    client.publish(topic_target_1, "1");
    client.publish(topic_target_2, "0");
    Serial.println("+ Published active state target 1");
  } 
  else if(activeTarget == 2) {
    // Target 2 is selected
    client.publish(topic_target_1, "0");
    client.publish(topic_target_2, "1");
    Serial.println("+ Published active state target 2");
  }
  else {
    // Invalid state
    client.publish(topic_target_1, "0");
    client.publish(topic_target_2, "0");
    Serial.println("+ Published panic mode");
  }
}

void switchUSBTarget(char* target, String message, int switchDelay) {
  if(strcmp(target, topic_target_1) == 0) {
    // Operate on target 1
    if(activeTarget == 1 && message == "0") {
      // Target 1 is currently active but should be switched off
      Serial.println("+ Switch from 1 to 2, 1 was active");
      activeTarget = 2;
      switchRelay();
      publishTopics();
    }
    else if(activeTarget == 2 && message == "1") {
      // Target 1 is currently inactive but should be switched on
      Serial.println("+ Switch from 2 to 1, 1 was inactive");
      activeTarget = 1;
      switchRelay();
      publishTopics();
    }
  }
  else if(strcmp(target, topic_target_2) == 0) {
    // Operate on target 2
    if(activeTarget == 2 && message == "0") {
      // Target 2 is currently active but should be switched off
      Serial.println("+ Switch from 2 to 1, 2 was active");
      activeTarget = 1;
      switchRelay();
      publishTopics();
    }
    else if(activeTarget == 1 && message == "1") {
      // Target 2 is currently inactive but should be switched on
      Serial.println("+ Switch from 1 to 2, 2 was inactive");
      activeTarget = 2;
      switchRelay();
      publishTopics();
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (byte i = 0; i < length; i++) {
      char tmp = char(payload[i]);
      msg += tmp;
  }
  switchUSBTarget(topic, msg, ENABLE_DELAY);
}

void handleRoot() {
  String response = F("<!DOCTYPE html>\n"
              "<html lang='en'>\n"
              "<head>\n"
              "<title>usb-switch-switcher</title>\n"
              "</head>\n"
              "<link rel=\"stylesheet\" href=\"https://unpkg.com/sakura.css/css/sakura.css\" type=\"text/css\">\n" 
              "<body>\n"
              "<h1>usb-switch-switcher</h1>\r\n");
  response = response + "<h2>firmware version: " + firmware_version + "</h2>\r\n";
  response = response + "<p>current active usb source:\r\n<b>device on port " + activeTarget + "</b></p>";
  response = response + "</body></html>";
  httpServer.send(200, "text/html", response);
}

void setup() {
  // Setup console
  Serial.begin(9600);
  Serial.println();

  // Setup pin
  digitalWrite(RELAY, LOW);
  pinMode(RELAY, OUTPUT);

  // Setup wifi
  WiFi.begin(ssid, password);
  Serial.print("+ Connecting to wifi");
  while (WiFi.status() != WL_CONNECTED)
  {
      delay(500);
      Serial.print(".");
  }
  Serial.println();
  Serial.print("+ Connected, IP address: ");
  Serial.println(WiFi.localIP());

  // Setup dns discovery
  MDNS.begin(ota_hostname);
  MDNS.addService("http", "tcp", 80);
  // Setup http update server with credentials
  httpUpdater.setup(&httpServer, ota_username, ota_password);
  // Add root handler
  httpServer.on("/", handleRoot);
  httpServer.begin();
  Serial.printf("+ HTTPUpdateServer ready! Open http://%s.local/update in your browser\n", ota_hostname);

  // Setup mqtt connection
  client.setServer(mqtt_broker, 1883);
  client.setCallback(callback);

  Serial.println("+ Initialization complete. Waiting for incoming connections...");
}

void loop() {
  if (!client.connected()) {
        while (!client.connected()) {
            client.connect("usb-switch-switcher", mqtt_user, mqtt_password);
            client.subscribe(topic_target_1);
            client.subscribe(topic_target_2);
            // Publish default state
            client.publish(topic_target_1, "1");
            client.publish(topic_target_2, "0");
            delay(MQTT_DELAY);
        }
    }
    client.loop();
    httpServer.handleClient();
    MDNS.update();
}


