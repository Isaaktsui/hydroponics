#include <SoftwareSerial.h>

// Pin definitions
#define ESP8266_RX 6 // Arduino receives from WiFi Bee TX
#define ESP8266_TX 7 // Arduino transmits to WiFi Bee RX

SoftwareSerial espSerial(ESP8266_RX, ESP8266_TX); // RX, TX

// Replace with your WiFi credentials
const char* ssid = "Isaac's iPhone";
const char* password = "Isaac0225";

void sendCommand(const char* cmd, unsigned long timeout = 2000) {
  espSerial.println(cmd);
  unsigned long start = millis();
  while (millis() - start < timeout) {
    while (espSerial.available()) {
      char c = espSerial.read();
      Serial.write(c);
    }
  }
  Serial.println();
}

void setup() {
  Serial.begin(9600);      // Serial monitor
  espSerial.begin(9600);   // WiFi Bee baud rate

  Serial.println("Testing WiFi Bee AT command response...");
  delay(2000);

  // Test AT
  sendCommand("AT", 1500);
  delay(1000);

  // Set WiFi mode to Station
  Serial.println("Setting WiFi mode to Station...");
  sendCommand("AT+CWMODE=1", 1500);
  delay(1000);

  // Connect to WiFi
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  Serial.print("Command: AT+CWJAP=\"");
  Serial.print(ssid);
  Serial.print("\",\"");
  Serial.print(password);
  Serial.println("\"");

  espSerial.print("AT+CWJAP=\"");
  espSerial.print(ssid);
  espSerial.print("\",\"");
  espSerial.print(password);
  espSerial.println("\"");

  unsigned long wifiTimeout = millis();
  bool connected = false;
  while (millis() - wifiTimeout < 20000) { // up to 20 seconds for connecting
    while (espSerial.available()) {
      String line = espSerial.readStringUntil('\n');
      Serial.println(line);
      if (line.indexOf("WIFI CONNECTED") >= 0 || line.indexOf("OK") >= 0) {
        connected = true;
      }
    }
    if (connected) break;
  }

  if (connected) {
    Serial.println("WiFi Connected!");
  } else {
    Serial.println("WiFi NOT Connected.");
    return;
  }

  // Show IP address
  Serial.println("Getting IP address...");
  sendCommand("AT+CIFSR", 2000);
}

void loop() {
  // Nothing to do in loop
}