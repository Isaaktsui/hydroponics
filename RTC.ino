#include <Wire.h>
#include "DFRobot_SD3031.h"

DFRobot_SD3031 rtc;

void setup() {
  Serial.begin(115200);
  rtc.begin();

  // Uncomment, fill in the correct values, and upload ONCE to set the RTC,
  // then comment this line again and re-upload so time is not reset each time.
  rtc.setTime(2025,06,02,15,17,30); // YYYY, MM, DD, HH, MM, SS
}

void loop() {
  sTimeData_t now = rtc.getRTCTime();

  Serial.print("Date: ");
  Serial.print(now.year); Serial.print("-");
  Serial.print(now.month); Serial.print("-");
  Serial.print(now.day); Serial.print(" ");
  Serial.print("Time: ");
  Serial.print(now.hour); Serial.print(":");
  Serial.print(now.minute); Serial.print(":");
  Serial.println(now.second);

  delay(1000);
}