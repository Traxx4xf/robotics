#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h> // Library for parsing JSON into text

// Wi-Fi Credentials
const char* ssid = "TECNO CAMON 40 Pro 5G";
const char* password = "santrifiwa";

// Mosquitto / HiveMQ Broker Settings
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* listen_topic = "aldeb"; // Topic to listen to external publishers

// OLED Display Configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

WiFiClient espClient;
PubSubClient client(espClient);

// Received Data Tracking
String rxTopic = "";
String rxPayload = "";
String parsedText = ""; // Holds the clean text parsed from JSON
unsigned long rxCount = 0;

// Update OLED screen whenever a new message arrives
void updateOLEDDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Line 1: Header + Message Count
  display.setCursor(0, 0);
  display.print("RX MODE");
  display.setCursor(65, 0);
  display.print("Msgs: ");
  display.print(rxCount);

  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  // Line 2: Topic
  display.setCursor(0, 14);
  display.print("Topic: ");
  display.print(rxTopic);

  display.drawLine(0, 24, 128, 24, SSD1306_WHITE);

  // Line 3: Parsed Clean Text
  display.setCursor(0, 28);
  display.print(parsedText);

  display.display();
}

// Callback function triggered immediately when ANOTHER device sends a message
void callback(char* topic, byte* payload, unsigned int length) {
  rxCount++;
  rxTopic = String(topic);

  // Convert incoming payload bytes to String
  rxPayload = "";
  for (unsigned int i = 0; i < length; i++) {
    rxPayload += (char)payload[i];
  }

  // --- Parse JSON into formatted text ---
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload, length);

  parsedText = "";

  if (error) {
    // Plain string (not JSON)
    parsedText = rxPayload;
  } else {
    // Extract standard sensor fields if available
    if (doc.containsKey("temp") || doc.containsKey("temperature")) {
      float t = doc["temp"] | doc["temperature"];
      parsedText += "Temp: " + String(t, 1) + " C\n";
    }
    if (doc.containsKey("hum") || doc.containsKey("humidity")) {
      float h = doc["hum"] | doc["humidity"];
      parsedText += "Hum : " + String(h, 1) + " %\n";
    }
    if (doc.containsKey("time")) {
      parsedText += "Time: " + doc["time"].as<String>() + "\n";
    }
    if (doc.containsKey("seq")) {
      parsedText += "Seq : #" + String(doc["seq"].as<long>()) + "\n";
    }
    if (doc.containsKey("event")) {
      parsedText += "Evt : " + doc["event"].as<String>() + "\n";
    }

    // Generic fallback: format any custom JSON key-value pairs
    if (parsedText == "") {
      for (JsonPair kv : doc.as<JsonObject>()) {
        parsedText += String(kv.key().c_str()) + ": " + kv.value().as<String>() + "\n";
      }
    }
  }

  // Print to Serial Monitor
  Serial.print("\n[INCOMING SIGNAL #");
  Serial.print(rxCount);
  Serial.print("] Topic: ");
  Serial.println(rxTopic);
  Serial.println("Raw JSON    : " + rxPayload);
  Serial.println("Parsed Text :\n" + parsedText);

  // Refresh OLED with parsed text
  updateOLEDDisplay();
}

void setup_wifi() {
  delay(10);
  Serial.print("Connecting to Wi-Fi");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected! IP: " + WiFi.localIP().toString());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("WAIT U FCKING IDIOT IM CONNECTING TO THE BROKER...");
    String clientId = "ESP32_Receiver_" + String(random(0xffff), HEX);

    if (client.connect(clientId.c_str())) {
      Serial.println(" NOT CONNECTED!(kidding)");

      if (client.subscribe(listen_topic)) {
        Serial.print("Listening for messages on: ");
        Serial.println(listen_topic);
      }
    } else {
      Serial.print(" Failed, rc=");
      Serial.print(client.state());
      Serial.println(" (Retrying in 2s...)");
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  Wire.begin();
  Wire.setClock(400000); // 400kHz I2C clock

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("OLED allocation failed!");
    for (;;);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 25);
  display.println("Hmmm wheres the signal???...");
  display.display();

  setup_wifi();

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  client.setBufferSize(512);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}
