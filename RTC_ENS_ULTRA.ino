#include <Wire.h>
#include "DFRobot_SD3031.h"
#include "DFRobot_ENS160.h"

// ENS160 I2C address (default 0x53)
#define ENS160_I2C_ADDR 0x53

// ----- Ultrasonic Sensor Pin Definitions -----
#define TRIG_WATER      22
#define ECHO_WATER      23
#define TRIG_ACID       24
#define ECHO_ACID       25
#define TRIG_BASE       26   
#define ECHO_BASE       27
#define TRIG_NUTRIENT   28
#define ECHO_NUTRIENT   29

// --- Tank "full" heights in centimeters (distance from sensor to tank bottom) ---
#define WATER_TANK_HEIGHT_CM     30.0
#define ACID_TANK_HEIGHT_CM      25.0
#define BASE_TANK_HEIGHT_CM      25.0
#define NUTRIENT_TANK_HEIGHT_CM  20.0

// Instantiate RTC and ENS160 objects
DFRobot_SD3031 rtc;
DFRobot_ENS160_I2C ens160(&Wire, ENS160_I2C_ADDR);

// --- Ultrasonic Sensor Functions ---
float readUltrasonicCM(uint8_t trigPin, uint8_t echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, 30000); // 30ms timeout ≈ 5m max
  float distance = duration * 0.0343 / 2.0;
  return (duration == 0) ? -1 : distance; // -1 = timeout
}

void setupUltrasonics() {
  pinMode(TRIG_WATER, OUTPUT);    pinMode(ECHO_WATER, INPUT);
  pinMode(TRIG_ACID, OUTPUT);     pinMode(ECHO_ACID, INPUT);
  pinMode(TRIG_BASE, OUTPUT);     pinMode(ECHO_BASE, INPUT);
  pinMode(TRIG_NUTRIENT, OUTPUT); pinMode(ECHO_NUTRIENT, INPUT);
}

void printUltrasonicLevels() {
  float waterSensorDist    = readUltrasonicCM(TRIG_WATER, ECHO_WATER);
  float acidSensorDist     = readUltrasonicCM(TRIG_ACID, ECHO_ACID);
  float baseSensorDist     = readUltrasonicCM(TRIG_BASE, ECHO_BASE);
  float nutrientSensorDist = readUltrasonicCM(TRIG_NUTRIENT, ECHO_NUTRIENT);

  Serial.print("Water Tank: ");
  if (waterSensorDist < 0) Serial.print("No Echo");
  else {
    float waterLevel = WATER_TANK_HEIGHT_CM - waterSensorDist;
    Serial.print(waterLevel, 1); Serial.print(" cm (");
    Serial.print(waterSensorDist, 1); Serial.print(" cm from sensor)");
  }
  Serial.print(", Acid Buffer: ");
  if (acidSensorDist < 0) Serial.print("No Echo");
  else {
    float acidLevel = ACID_TANK_HEIGHT_CM - acidSensorDist;
    Serial.print(acidLevel, 1); Serial.print(" cm (");
    Serial.print(acidSensorDist, 1); Serial.print(" cm from sensor)");
  }
  Serial.print(", Base Buffer: ");
  if (baseSensorDist < 0) Serial.print("No Echo");
  else {
    float baseLevel = BASE_TANK_HEIGHT_CM - baseSensorDist;
    Serial.print(baseLevel, 1); Serial.print(" cm (");
    Serial.print(baseSensorDist, 1); Serial.print(" cm from sensor)");
  }
  Serial.print(", Nutrient Tank: ");
  if (nutrientSensorDist < 0) Serial.print("No Echo");
  else {
    float nutrientLevel = NUTRIENT_TANK_HEIGHT_CM - nutrientSensorDist;
    Serial.print(nutrientLevel, 1); Serial.print(" cm (");
    Serial.print(nutrientSensorDist, 1); Serial.print(" cm from sensor)");
  }
  Serial.println();
}

// ----- SETUP -----
void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Wire.begin();

  // RTC init
  if (rtc.begin() != 0) {
    Serial.println("DFRobot SD3031 RTC not found!");
    while (1);
  } else {
    Serial.println("SD3031 RTC initialized.");
  }

  // ENS160 init
  if (ens160.begin() != 0) {
    Serial.println("DFRobot ENS160 not found!");
    while (1);
  } else {
    Serial.println("ENS160 sensor initialized.");
  }

  // Ultrasonic sensors init
  setupUltrasonics();
  Serial.println("Ultrasonic level sensors initialized.");
}

// ----- LOOP -----
void loop() {
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint >= 1000) {
    lastPrint = millis();

    // RTC
    sTimeData_t now = rtc.getRTCTime();
    Serial.print("[RTC] ");
    Serial.print(now.year); Serial.print("-");
    Serial.print(now.month); Serial.print("-");
    Serial.print(now.day); Serial.print(" ");
    Serial.print(now.hour); Serial.print(":");
    Serial.print(now.minute); Serial.print(":");
    Serial.print(now.second);

    // ENS160
    uint16_t eco2 = ens160.getECO2();
    uint16_t tvoc = ens160.getTVOC();
    Serial.print(" | ENS160 eCO2: ");
    Serial.print(eco2);
    Serial.print(" ppm, TVOC: ");
    Serial.print(tvoc);
    Serial.print(" ppb | ");

    // Ultrasonics (Water, Acid, Base, Nutrient)
    printUltrasonicLevels();
  }
}