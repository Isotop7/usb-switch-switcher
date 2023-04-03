#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include "Settings.h"

#define RELAY D1
#define ENABLE_DELAY 100
#define MQTT_DELAY 100
const char *topic_source_1 = "office/usb/source/1";
const char *topic_source_2 = "office/usb/source/2";

const char *ssid = WIFI_SSID;
const char *password = WIFI_PSK;
const char *mqtt_broker = MQTT_BROKER;
const char *mqtt_user = MQTT_USER;
const char *mqtt_password = MQTT_PASSWORD;

WiFiClient espClient;
PubSubClient client(espClient);
// Default active USB source is 1
int activeSource = 1;

void switchRelay() {
  digitalWrite(RELAY, LOW);
  delay(ENABLE_DELAY);
  digitalWrite(RELAY, HIGH);
}

void publishTopics() {
  if(activeSource == 1) {
    // Source 1 is selected
    client.publish(topic_source_1, "1");
    client.publish(topic_source_2, "0");
    Serial.println("+ Published active state source 1");
  } 
  else if(activeSource == 2) {
    // Source 2 is selected
    client.publish(topic_source_1, "0");
    client.publish(topic_source_2, "1");
    Serial.println("+ Published active state source 2");
  }
  else {
    // Invalid state
    client.publish(topic_source_1, "0");
    client.publish(topic_source_2, "0");
    Serial.println("+ Published panic mode");
  }
}

void switchUSBSource(char* source, String message, int switchDelay) {
  if(strcmp(source, topic_source_1) == 0) {
    // Operate on source 1
    if(activeSource == 1 && message == "0") {
      // Source 1 is currently active but should be switched off
      Serial.println("+ Switch from 1 to 2, 1 was active");
      activeSource = 2;
      switchRelay();
      publishTopics();
    }
    else if(activeSource == 2 && message == "1") {
      // Source 1 is currently inactive but should be switched on
      Serial.println("+ Switch from 2 to 1, 1 was inactive");
      activeSource = 1;
      switchRelay();
      publishTopics();
    }
  }
  else if(strcmp(source, topic_source_2) == 0) {
    // Operate on source 2
    if(activeSource == 2 && message == "0") {
      // Source 2 is currently active but should be switched off
      Serial.println("+ Switch from 2 to 1, 2 was active");
      activeSource = 1;
      switchRelay();
      publishTopics();
    }
    else if(activeSource == 1 && message == "1") {
      // Source 2 is currently inactive but should be switched on
      Serial.println("+ Switch from 1 to 2, 2 was inactive");
      activeSource = 2;
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
  switchUSBSource(topic, msg, ENABLE_DELAY);
}

void setup() {
  // Setup console
  Serial.begin(9600);
  Serial.println();

  // Setup pin
  pinMode(RELAY, OUTPUT);
  digitalWrite(RELAY, HIGH);

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

  // Setup mqtt connection
  client.setServer(mqtt_broker, 1883);
  client.setCallback(callback);

  Serial.println("+ Initialization complete. Waiting for incoming connections...");
}

void loop() {
  if (!client.connected()) {
        while (!client.connected()) {
            client.connect("usb-switch-switcher", mqtt_user, mqtt_password);
            client.subscribe(topic_source_1);
            client.subscribe(topic_source_2);
            // Publish default state
            client.publish(topic_source_1, "1");
            client.publish(topic_source_2, "0");
            delay(MQTT_DELAY);
        }
    }
    client.loop();
}


