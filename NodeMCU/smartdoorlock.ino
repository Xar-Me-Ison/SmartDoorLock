// Wireless RFID and WiFi Smart Door Lock using NodeMCU ESP8266

#include <SPI.h>
#include <Servo.h>
#include <MFRC522.h>
#include <WiFiManager.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>

#define _WIFIMANAGER_LOGLEVEL_ NONE // For the WiFiManager library

#define RST_PIN 0     // RST-PIN for RC522 - RFID - SPI - Module GPIO0 (D3)
#define SDA_PIN 2     // SDA-PIN for RC522 - RFID - SPI - Module GPIO2 (D4)
#define SRV_PIN 5     // PIN for Servo Motor - DC Motor - Module GPIO5 (D1)
#define BTN_PIN 4     // PIN for Button - GPIO4 (D2)
#define LED_R_PIN 15  // Red pin for RGB LED - Module GPIO16 (D0)
#define LED_G_PIN 16  // Green pin for RGB LED - Module GPIO15 (D8)
#define LED_B_PIN 3   // Blue pin for RGB LED - Using 3.3V pin

// Global variables
bool SERVO_OPEN = false;
bool AUTH_TAG = 0;

// Wi-Fi connection information
const char* ssid = "";
const char* pswd = "";

// MQTT connection information
const char* mqtt_server = "mqtt.eclipseprojects.io";
String clientId = "ESP8266Client-" + String(ESP.getChipId());
String topic = "smartdoorlock";

// RFID information
const char* rfid_card = "29 73 A4 89";
const char* rfid_tag = "60 1A A8 1E";

// Create instances
WiFiClientSecure espClientSecure;
PubSubClient client(espClientSecure);
MFRC522 mfrc522(SDA_PIN, RST_PIN);
Servo servo;

/* Setup function */
void setup() {
  Serial.begin(115200);
  SPI.begin();
  Servo_Init();
  MFRC522_Init();
  Button_Init();
  WiFi_Init();
  MQTT_Init();
  RGB_LED_Init();
}

/* Loop function */
void loop() {
  MQTT_Check();
  MFRC522_Check();
  Button_Check();
}

/* Initialization functions */
void Servo_Init() {
  Serial.println("\n* Servo Motor initialization *");
  servo.attach(SRV_PIN);
  servo.write(0);
  Serial.println("Status: initialized");
}

void MFRC522_Init() {
  Serial.println("\n* RFID MFRC522 initialization *");
  mfrc522.PCD_Init();
  if (mfrc522.PCD_PerformSelfTest()) {
    mfrc522.PCD_DumpVersionToSerial();
    Serial.println("Status: initialized");
  } else {
    Serial.println("Status: uninitialized");
  }
}

void Button_Init() {
  Serial.println("\n* Button initialization *");
  pinMode(BTN_PIN, INPUT_PULLUP);
  Serial.println("Status: initialized");
}

void WiFi_Init() {
  WiFiManager wifiManager;
  wifiManager.setDebugOutput(false);

  Serial.println("\n* WiFi initialization *");
  if (!wifiManager.autoConnect("SmartDoorLock_AP")) {
    Serial.println("Access point: SmartDoorLock_AP");
    ESP.restart();
  }

  Serial.printf("WiFi connected (%s)\n", WiFi.SSID().c_str());
  Serial.printf("IP address: %s\n", WiFi.localIP().toString().c_str());
  delay(3000);
}

void MQTT_Init() {
  Serial.println("\n* MQTT initialization *");

  // Set up secure connection parameters
  espClientSecure.setInsecure(); // Use this line if you do not have specific CA, CERT, or KEY to load.
  client.setServer(mqtt_server, 8883);
  client.setCallback(MQTT_callback);

  Serial.println("Status: initialized");
  Serial.printf("MQTT Server: %s\n", mqtt_server);
  Serial.printf("Subscribed: %s\n", topic.c_str());
}

void RGB_LED_Init() {
  pinMode(LED_R_PIN, OUTPUT);
  pinMode(LED_G_PIN, OUTPUT);
  pinMode(LED_B_PIN, OUTPUT);
  RGB_LED_SetColor(0, 0, 0); // Turn off LED initially
}

/* Loop check functions */
void MQTT_Check() {
  if (!client.connected()) {
    MQTT_reconnect();
  }
  client.loop();
}

void MFRC522_Check() {
  if (!mfrc522.PICC_IsNewCardPresent()) {
    delay(50);
    return;
  }
  if (!mfrc522.PICC_ReadCardSerial()) {
    delay(50);
    return;
  }
  String content = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
    content.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  // Serial.println();
  content.toUpperCase();
  // Serial.println("Card ID: " + content);

  if (content.substring(1) == rfid_card || content.substring(1) == rfid_tag) {
    AUTH_TAG = 1;

    String state;
    if (SERVO_OPEN) {
      servo.write(0);
      RGB_LED_SetColor(255, 0, 0); // Red for locked
      state = "locked";
    } else {
      servo.write(180);
      RGB_LED_SetColor(0, 255, 0); // Green for unlocked
      state = "unlocked";
    }
    client.publish(topic.c_str(), state.c_str());
    SERVO_OPEN = !SERVO_OPEN;
  } else {
    AUTH_TAG = 0;
    RGB_LED_SetColor(0, 0, 255); // Blue for unauthorized
  }
  delay(2000);
}

void Button_Check() {
  static bool lastButtonState = HIGH;
  bool currentButtonState = digitalRead(BTN_PIN);

  if (lastButtonState == HIGH && currentButtonState == LOW) {
    String state;
    if (SERVO_OPEN) {
      servo.write(0);
      RGB_LED_SetColor(255, 0, 0); // Red for locked
      state = "locked";
    } else {
      servo.write(180);
      RGB_LED_SetColor(0, 255, 0); // Green for unlocked
      state = "unlocked";
    }
    client.publish(topic.c_str(), state.c_str());
    SERVO_OPEN = !SERVO_OPEN;
  }
  lastButtonState = currentButtonState;
}

/* MQTT handler functions */
void MQTT_callback(char* topic, byte* payload, unsigned int length) {
  String message;
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    message += (char)payload[i];
  }
  Serial.println();

  if (message == "unlocked") {
    servo.write(180);
    RGB_LED_SetColor(0, 255, 0); // Green for unlocked
    SERVO_OPEN = true;
    // Serial.println("MQTT: Door unlocked\n");
  } else if (message == "locked") {
    servo.write(0);
    RGB_LED_SetColor(255, 0, 0); // Red for locked
    SERVO_OPEN = false;
    // Serial.println("MQTT: Door locked\n");
  }
}

void MQTT_reconnect() {
  Serial.println("\n* MQTT connection *");
  while (!client.connected()) {
    Serial.print("Status: ");
    if (client.connect(clientId.c_str())) {
      Serial.println("connected\n");
      client.subscribe(topic.c_str());
      client.publish(topic.c_str(), "locked");
    } else {
      Serial.print("failed [rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds]");
      delay(5000);
    }
  }
}

/* RGB LED handler functions */
void RGB_LED_SetColor(int red, int green, int blue) {
  analogWrite(LED_R_PIN, red);
  analogWrite(LED_G_PIN, green);
  analogWrite(LED_B_PIN, blue);
}
