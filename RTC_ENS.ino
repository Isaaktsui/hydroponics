#include <Wire.h>
#include "DFRobot_SD3031.h"
#include "DFRobot_ENS160.h"

#define ENS160_I2C_ADDR 0x53

DFRobot_SD3031 rtc;
DFRobot_ENS160_I2C ens160(&Wire, ENS160_I2C_ADDR);

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Wire.begin();

  if (rtc.begin() != 0) {
    Serial.println("DFRobot SD3031 RTC not found!");
    while (1);
  } else {
    Serial.println("SD3031 RTC initialized.");
  }

  if (ens160.begin() != 0) {
    Serial.println("DFRobot ENS160 not found!");
    while (1);
  } else {
    Serial.println("ENS160 sensor initialized.");
  }
}

void loop() {
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint >= 1000) {
    lastPrint = millis();

    sTimeData_t now = rtc.getRTCTime();
    Serial.print("[RTC] ");
    Serial.print(now.year); Serial.print("-");
    Serial.print(now.month); Serial.print("-");
    Serial.print(now.day); Serial.print(" ");
    Serial.print(now.hour); Serial.print(":");
    Serial.print(now.minute); Serial.print(":");
    Serial.print(now.second);

    // No "available()" check needed
    uint16_t eco2 = ens160.getECO2();
    uint16_t tvoc = ens160.getTVOC();
    Serial.print(" | ENS160 eCO2: ");
    Serial.print(eco2);
    Serial.print(" ppm, TVOC: ");
    Serial.print(tvoc);
    Serial.println(" ppb");
  }
}
