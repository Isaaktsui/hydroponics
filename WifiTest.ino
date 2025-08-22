#include <SoftwareSerial.h>

#define ESP_RX 5 // Arduino receives from Wifi Bee TX (DOUT)
#define ESP_TX 6 // Arduino transmits to Wifi Bee RX (DIN)

SoftwareSerial espSerial(ESP_RX, ESP_TX); // RX, TX

void setup() {
  Serial.begin(115200);       // Serial monitor on PC
  espSerial.begin(115200);    // ESP8266 (Wifi Bee) UART

  Serial.println("Wifi Bee ESP8266 Test Ready");
  Serial.println("Type AT commands below.");
}

void loop() {
  // PC to Wifi Bee
  if (Serial.available()) {
    char c = Serial.read();
    espSerial.write(c);
  }
  // Wifi Bee to PC
  if (espSerial.available()) {
    char c = espSerial.read();
    Serial.write(c);
  }
}