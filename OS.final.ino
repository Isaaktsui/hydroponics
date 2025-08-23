#include <Wire.h>
#include <Servo.h>
#include <DHT.h>
#include "DFRobot_SD3031.h"
#include "DFRobot_ENS160.h"
#include <avr/wdt.h>

#define LED_FAULT 13 // Use built-in LED for alarm

// Tank heights (in cm)
#define WATER_TANK_HEIGHT_CM     10.0
#define ACID_TANK_HEIGHT_CM      10.0
#define BASE_TANK_HEIGHT_CM      10.0
#define NUTRIENT_TANK_HEIGHT_CM  10.0
const float EC_R_KNOWN = 10000.0; // 10k resistor (ohms)

// Ultrasonic Tank Sensors
#define TRIG_WATER      22
#define ECHO_WATER      23
#define TRIG_ACID       26
#define ECHO_ACID       27
#define TRIG_BASE       24   
#define ECHO_BASE       25
#define TRIG_NUTRIENT   28
#define ECHO_NUTRIENT   29

// DHT22
#define DHTPIN 30
#define DHTTYPE DHT22

// Analog Sensors
#define LDR_PIN A0
#define PH_PIN  A1
#define EC_PIN  A2

// Actuators
#define LEDS_MOSFET_GATE_PIN 46
#define UV1_PIN   48
#define UV2_PIN   49
#define FAN1_PWM_PIN 50
#define FAN2_PWM_PIN 51
#define SERVO1_PIN 52
#define SERVO2_PIN 53
#define PUMP1_PIN 35
#define PUMP2_PIN 33
#define PUMP3_PIN 34
#define PUMP4_PIN 32 // Acid
#define PUMP5_PIN 36 // Base
#define PUMP6_PIN 31 // Nutrient
#define MISTER_PIN 47
#define WIFI_RX_PIN 18
#define WIFI_TX_PIN 19

// L298N Motor Driver Shield Pins
#define L298N_IN1 9    // Motor A IN1
#define L298N_IN2 10   // Motor A IN2
#define L298N_IN3 11   // Motor B IN3
#define L298N_IN4 12   // Motor B IN4

// --- Heating Addition ---
#define HEATER_PIN 45 // Assign a pin for the heater relay/module

Servo servo1, servo2;
DHT dht(DHTPIN, DHTTYPE);
DFRobot_SD3031 rtc;
DFRobot_ENS160_I2C ens160(&Wire, 0x53); // Default I2C address

enum DosingState { DOSING_IDLE, DOSING_ACTIVE, DOSING_WAIT_MIX };
DosingState acidState = DOSING_IDLE, baseState = DOSING_IDLE, nutrState = DOSING_IDLE;
unsigned long acidDosingStart = 0, baseDosingStart = 0, nutrDosingStart = 0;

// Fault flags
bool acidError = false, baseError = false, nutrError = false, waterError = false;
bool anyFault = false;

// Mister logic
static unsigned long lastMister = 0;
const unsigned long MISTER_ON_TIME = 3000;   // 3s burst
const unsigned long MISTER_OFF_TIME = 10000; // 10s off

const unsigned long DOSING_PULSE = 1000;     // ms pump ON per dose
const unsigned long DOSING_MIX = 12000;      // ms wait after dose

float lastPH = 7.0, lastEC = 1.0; // For feedback; should be actual readings
float target_pH_hi = 7.1, target_pH_lo = 5.7;
float target_ec = 1.6;

bool tankPrompted = false, tankConfirmed = false;

String inputBuffer = "";
bool promptForRTC = false;

// LDR dynamic light control
int targetLDR = 200; // Set your desired LDR value for "ideal" brightness
int ledPWM = 0;      // Current PWM value for lights

void setupUltrasonics() {
  pinMode(TRIG_WATER, OUTPUT);    pinMode(ECHO_WATER, INPUT);
  pinMode(TRIG_ACID, OUTPUT);     pinMode(ECHO_ACID, INPUT);
  pinMode(TRIG_BASE, OUTPUT);     pinMode(ECHO_BASE, INPUT);
  pinMode(TRIG_NUTRIENT, OUTPUT); pinMode(ECHO_NUTRIENT, INPUT);
}

void actuatorInit() {
  pinMode(LEDS_MOSFET_GATE_PIN, OUTPUT); analogWrite(LEDS_MOSFET_GATE_PIN, 0);
  pinMode(UV1_PIN, OUTPUT); digitalWrite(UV1_PIN, LOW);
  pinMode(UV2_PIN, OUTPUT); digitalWrite(UV2_PIN, LOW);
  pinMode(FAN1_PWM_PIN, OUTPUT); analogWrite(FAN1_PWM_PIN, 0);
  pinMode(FAN2_PWM_PIN, OUTPUT); analogWrite(FAN2_PWM_PIN, 0);
  servo1.attach(SERVO1_PIN); servo2.attach(SERVO2_PIN);
  servo1.write(90); servo2.write(90);
  int pumpPins[] = {PUMP1_PIN, PUMP2_PIN, PUMP3_PIN, PUMP4_PIN, PUMP5_PIN, PUMP6_PIN};
  for(int i=0; i<6; ++i) { pinMode(pumpPins[i], OUTPUT); digitalWrite(pumpPins[i], LOW); }
  pinMode(MISTER_PIN, OUTPUT); digitalWrite(MISTER_PIN, LOW);
  pinMode(LED_FAULT, OUTPUT); digitalWrite(LED_FAULT, LOW);

  // L298N pins
  pinMode(L298N_IN1, OUTPUT);
  pinMode(L298N_IN2, OUTPUT);
  pinMode(L298N_IN3, OUTPUT);
  pinMode(L298N_IN4, OUTPUT);

  // --- Heating Addition ---
  pinMode(HEATER_PIN, OUTPUT);
  digitalWrite(HEATER_PIN, LOW); // Heater OFF by default

  // Ensure Peltiers and fans are OFF
  setL298N('A', 'o', 0);
  setL298N('B', 'o', 0);
}

void setL298N(char channel, char dir, int pwm) {
  pwm = constrain(pwm, 0, 255);
  // L298N: INx pins for direction, PWM (if shield exposes ENA/ENB, use those for speed)
  if (channel == 'A') { // Motor A: IN1/IN2
    if (dir == 'f') {
      digitalWrite(L298N_IN1, HIGH);
      digitalWrite(L298N_IN2, LOW);
    } else if (dir == 'r') {
      digitalWrite(L298N_IN1, LOW);
      digitalWrite(L298N_IN2, HIGH);
    } else { // off
      digitalWrite(L298N_IN1, LOW);
      digitalWrite(L298N_IN2, LOW);
    }
    analogWrite(L298N_IN1, pwm); // If ENA is not exposed, use PWM on IN1
  } else if (channel == 'B') { // Motor B: IN3/IN4
    if (dir == 'f') {
      digitalWrite(L298N_IN3, HIGH);
      digitalWrite(L298N_IN4, LOW);
    } else if (dir == 'r') {
      digitalWrite(L298N_IN3, LOW);
      digitalWrite(L298N_IN4, HIGH);
    } else { // off
      digitalWrite(L298N_IN3, LOW);
      digitalWrite(L298N_IN4, LOW);
    }
    analogWrite(L298N_IN3, pwm); // If ENB is not exposed, use PWM on IN3
  }
}

void enableWatchdog() { wdt_enable(WDTO_8S); }
void resetWatchdog() { wdt_reset(); }

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  Wire.begin();
  rtc.begin();
  ens160.begin();
  dht.begin();
  setupUltrasonics();
  pinMode(LDR_PIN, INPUT); pinMode(PH_PIN, INPUT); pinMode(EC_PIN, INPUT);
  actuatorInit();
  Serial1.begin(115200);
  Serial.println("Type 'setrtc' and press Enter to set RTC.");
  Serial.println("Please fill all tanks and premix the main reservoir. Type 'ready' to continue.");
  tankPrompted = true;
  enableWatchdog();
}

// ======================== LOOP ========================
void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (inputBuffer.length() > 0) {
        if (promptForRTC) {
          handleRTCInput(inputBuffer);
        } else {
          handleSerialCommands(inputBuffer);
        }
        inputBuffer = "";
      }
    } else if (isPrintable(c)) {
      inputBuffer += c;
    }
  }
  if (tankPrompted && !tankConfirmed) {
    delay(100);
    resetWatchdog();
    return;
  }
  static unsigned long lastPrint = 0;
  if (!promptForRTC && millis() - lastPrint >= 1000) {
    lastPrint = millis();
    printSensorStatus();
    updateClimateControl();
    updateHydraulicControl();
    updateLightingControl();
    updateUVControl();
    runSafetyChecks();
    runDosingControl();
    printSensorCSV();
  }
  resetWatchdog();
}

// ======================== SERIAL COMMANDS ========================
void handleSerialCommands(String cmd) {
  cmd.trim();
  cmd.toLowerCase();
  if (cmd == "mister on") {
    digitalWrite(MISTER_PIN, HIGH); Serial.println("Mister ON");
  } else if (cmd == "mister off") {
    digitalWrite(MISTER_PIN, LOW); Serial.println("Mister OFF");
  } else if (cmd == "wifi test") {
    Serial1.println("AT"); Serial.println("Sent AT to ESP8266");
  } else if (cmd.startsWith("fan1 ")) {
    int val = cmd.substring(5).toInt();
    analogWrite(FAN1_PWM_PIN, val);
    Serial.print("Fan1 PWM set to "); Serial.println(val);
  } else if (cmd == "status") {
    printSensorStatus();
  } else if (cmd == "ready" && tankPrompted && !tankConfirmed) {
    Serial.println("Tanks filled, system is now active!");
    tankConfirmed = true;
  } else if (cmd == "test acidpump") {
    Serial.println("Testing acid pump (Pump4) for 2 seconds...");
    digitalWrite(PUMP4_PIN, HIGH); delay(2000); digitalWrite(PUMP4_PIN, LOW);
  } else if (cmd == "test basepump") {
    Serial.println("Testing base pump (Pump5) for 2 seconds...");
    digitalWrite(PUMP5_PIN, HIGH); delay(2000); digitalWrite(PUMP5_PIN, LOW);
  } else if (cmd == "test nutpump") {
    Serial.println("Testing nutrient pump (Pump6) for 2 seconds...");
    digitalWrite(PUMP6_PIN, HIGH); delay(2000); digitalWrite(PUMP6_PIN, LOW);
  } else if (cmd == "calibrate ph") {
    Serial.print("Raw pH ADC: "); Serial.println(analogRead(PH_PIN));
  } else if (cmd == "calibrate ec") {
    Serial.print("Raw EC ADC: "); Serial.println(analogRead(EC_PIN));
  } else if (cmd == "exportcsv") {
    printSensorCSV();
    Serial.println("CSV data printed.");
  } else if (cmd == "setrtc") {
    promptForRTC = true;
    Serial.println("Enter date/time as YYYY-MM-DD HH:MM:SS and press Enter:");
  } else if (cmd == "heater on") {
    digitalWrite(HEATER_PIN, HIGH); Serial.println("Heater ON (manual)");
  } else if (cmd == "heater off") {
    digitalWrite(HEATER_PIN, LOW); Serial.println("Heater OFF (manual)");
  }
}

void handleRTCInput(String input) {
  int year, month, day, hour, minute, second;
  if (sscanf(input.c_str(), "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second) == 6) {
    rtc.setTime(year, month, day, hour, minute, second);  // Correct DFRobot_SD3031 usage!
    Serial.print("RTC set to: "); Serial.println(input);
  } else {
    Serial.println("Invalid format. Use YYYY-MM-DD HH:MM:SS");
  }
  promptForRTC = false;
}

// ======================== SENSOR STATUS PRINT ========================
void printSensorStatus() {
  sTimeData_t now = rtc.getRTCTime();
  Serial.print("[RTC] ");
  Serial.print(now.year); Serial.print("-");
  Serial.print(now.month); Serial.print("-");
  Serial.print(now.day); Serial.print(" ");
  Serial.print(now.hour); Serial.print(":");
  Serial.print(now.minute); Serial.print(":");
  Serial.print(now.second);

  uint16_t eco2 = ens160.getECO2();
  uint16_t tvoc = ens160.getTVOC();
  Serial.print(" | ENS160 eCO2: ");
  Serial.print(eco2);
  Serial.print(" ppm, TVOC: ");
  Serial.print(tvoc);
  Serial.print(" ppb | ");

  float temp = dht.readTemperature();
  Serial.print(" DHT22 Temp: ");
  if (isnan(temp)) {
    Serial.print("Sensor error");
  } else {
    Serial.print(temp, 1); Serial.print(" C, ");
  }
  float humidity = dht.readHumidity();
  Serial.print("Humidity: ");
  if (isnan(humidity)) {
    Serial.print("Sensor error");
  } else {
    Serial.print(humidity, 1); Serial.print(" %");
  }
  Serial.print(" | ");

  int ldrValue = analogRead(LDR_PIN);
  Serial.print("LDR: "); Serial.print(ldrValue); Serial.print(" | ");

  int phRaw = analogRead(PH_PIN);
  Serial.print("pH ADC: "); Serial.print(phRaw); Serial.print(" | ");

  int ecRaw = analogRead(EC_PIN);
  float ecVout = ecRaw * (5.0 / 1023.0);
  float ecRprobe = (ecVout * EC_R_KNOWN) / (5.0 - ecVout);
  float ecMicroSiemens = 0;
  if (ecRprobe > 0) ecMicroSiemens = 1000000.0 / ecRprobe;
  Serial.print("EC ADC: ");
  Serial.print(ecRaw);
  Serial.print(" Vout: ");
  Serial.print(ecVout, 3);
  Serial.print(" V R_probe: ");
  Serial.print(ecRprobe, 1);
  Serial.print(" ohms EC: ");
  Serial.print(ecMicroSiemens, 1);
  Serial.print(" uS/cm | ");

  printUltrasonicLevels();
  Serial.println();
}

// ======================== SENSOR CSV EXPORT ========================
void printSensorCSV() {
  sTimeData_t now = rtc.getRTCTime();
  float temp = dht.readTemperature();
  float humidity = dht.readHumidity();
  int ldrValue = analogRead(LDR_PIN);
  int phRaw = analogRead(PH_PIN);
  int ecRaw = analogRead(EC_PIN);
  uint16_t eco2 = ens160.getECO2();
  uint16_t tvoc = ens160.getTVOC();

  Serial.print(now.year); Serial.print("-");
  Serial.print(now.month); Serial.print("-");
  Serial.print(now.day); Serial.print(" ");
  Serial.print(now.hour); Serial.print(":");
  Serial.print(now.minute); Serial.print(":");
  Serial.print(now.second); Serial.print(",");
  Serial.print(temp); Serial.print(",");
  Serial.print(humidity); Serial.print(",");
  Serial.print(ldrValue); Serial.print(",");
  Serial.print(phRaw); Serial.print(",");
  Serial.print(ecRaw); Serial.print(",");
  Serial.print(eco2); Serial.print(",");
  Serial.print(tvoc);
  Serial.println();
}

// ======================== ULTRASONIC LEVELS ========================
void printUltrasonicLevels() {
  float waterDist    = readUltrasonicCM(TRIG_WATER, ECHO_WATER);
  float acidDist     = readUltrasonicCM(TRIG_ACID, ECHO_ACID);
  float baseDist     = readUltrasonicCM(TRIG_BASE, ECHO_BASE);
  float nutrDist     = readUltrasonicCM(TRIG_NUTRIENT, ECHO_NUTRIENT);
  Serial.print("Water: "); Serial.print(waterDist, 1); Serial.print("cm | ");
  Serial.print("Acid: "); Serial.print(acidDist, 1); Serial.print("cm | ");
  Serial.print("Base: "); Serial.print(baseDist, 1); Serial.print("cm | ");
  Serial.print("Nutrient: "); Serial.print(nutrDist, 1); Serial.print("cm | ");
}

float readUltrasonicCM(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, 30000);
  if (duration == 0) return -1.0;
  float distanceCM = (duration / 2.0) * 0.0343;
  return distanceCM;
}

bool tankLow(float sensorDist, float tankHeight) {
  return (sensorDist < 0) || ((tankHeight - sensorDist) < 3.0);
}
bool tankOverflow(float sensorDist, float tankHeight) {
  return (sensorDist < 0) || ((tankHeight - sensorDist) > tankHeight - 1.0);
}

// ======================== CLIMATE CONTROL ========================
const float RH_HIGH_THRESHOLD = 80.0;
const float RH_LOW_THRESHOLD = 55.0;
const float TEMP_HIGH_THRESHOLD = 26.0;
const float TEMP_LOW_THRESHOLD = 18.0;
const float TEMP_HEAT_THRESHOLD = 17.5; // Add a small buffer to reduce relay chatter
const float TEMP_HEAT_OFF_THRESHOLD = 18.5; // Hysteresis for heater
const uint16_t CO2_LOW_THRESHOLD = 600;

void updateClimateControl() {
  float temp = dht.readTemperature();
  float RH = dht.readHumidity();
  uint16_t eco2 = ens160.getECO2();

  bool rhHigh = RH > RH_HIGH_THRESHOLD;
  bool rhLow = RH < RH_LOW_THRESHOLD;
  bool eco2Low = eco2 < CO2_LOW_THRESHOLD;
  bool tempHigh = temp > TEMP_HIGH_THRESHOLD;
  bool tempLow = temp < TEMP_LOW_THRESHOLD;

  // --- Heating Addition ---
  static bool heaterOn = false;
  // Use hysteresis to prevent relay chatter
  if (!heaterOn && temp < TEMP_HEAT_THRESHOLD) {
    digitalWrite(HEATER_PIN, HIGH);
    heaterOn = true;
    Serial.println("[CLIMATE] Heater ON (temperature low)");
  } else if (heaterOn && temp > TEMP_HEAT_OFF_THRESHOLD) {
    digitalWrite(HEATER_PIN, LOW);
    heaterOn = false;
    Serial.println("[CLIMATE] Heater OFF (temperature normal)");
  }

  bool hatchOpen = rhHigh || eco2Low || tempHigh;
  bool fanOn = hatchOpen;
  bool peltierCool = rhHigh || tempHigh;
  bool misterOn = rhLow;

  // Servo 1 rest at 90°, open at 180°, Servo 2 rest at 90°, open at 0°
  if (hatchOpen) { 
    servo1.write(0);   // Servo 1 open
    servo2.write(180); // Servo 2 open
  } else { 
    servo1.write(90);    // Servo 1 rest
    servo2.write(90);    // Servo 2 rest
  }
  analogWrite(FAN1_PWM_PIN, fanOn ? 255 : 0);
  analogWrite(FAN2_PWM_PIN, fanOn ? 255 : 0);

  // Use L298N to control Peltiers
  setL298N('A', peltierCool ? 'f' : 'o', peltierCool ? 255 : 0);
  setL298N('B', peltierCool ? 'f' : 'o', peltierCool ? 255 : 0);

  static unsigned long misterBurstStart = 0;
  static bool misterBursting = false;
  if (misterOn) {
    if (!misterBursting && millis() - lastMister > MISTER_OFF_TIME) {
      digitalWrite(MISTER_PIN, HIGH);
      misterBurstStart = millis();
      misterBursting = true;
    }
    if (misterBursting && millis() - misterBurstStart > MISTER_ON_TIME) {
      digitalWrite(MISTER_PIN, LOW);
      lastMister = millis();
      misterBursting = false;
    }
  } else {
    digitalWrite(MISTER_PIN, LOW);
    misterBursting = false;
  }
}

// ======================== HYDRAULIC CONTROL ========================
void updateHydraulicControl() {
  float waterDist    = readUltrasonicCM(TRIG_WATER, ECHO_WATER);
  float acidDist     = readUltrasonicCM(TRIG_ACID, ECHO_ACID);
  float baseDist     = readUltrasonicCM(TRIG_BASE, ECHO_BASE);
  float nutrDist     = readUltrasonicCM(TRIG_NUTRIENT, ECHO_NUTRIENT);

  if (tankLow(waterDist, WATER_TANK_HEIGHT_CM)) {
    waterError = true;
    Serial.println("[HYDRO] Water tank low! Pumps stopped!");
    digitalWrite(PUMP1_PIN, LOW); digitalWrite(PUMP2_PIN, LOW); digitalWrite(PUMP3_PIN, LOW);
    return;
  }
  if (tankLow(acidDist, ACID_TANK_HEIGHT_CM)) {
    acidError = true;
    Serial.println("[HYDRO] Acid tank low! Acid dosing disabled.");
  }
  if (tankLow(baseDist, BASE_TANK_HEIGHT_CM)) {
    baseError = true;
    Serial.println("[HYDRO] Base tank low! Base dosing disabled.");
  }
  if (tankLow(nutrDist, NUTRIENT_TANK_HEIGHT_CM)) {
    nutrError = true;
    Serial.println("[HYDRO] Nutrient tank low! Nutrient dosing disabled.");
  }
}

// ======================== DOSING CONTROL ========================
void runDosingControl() {
  if (!acidError) {
    switch (acidState) {
      case DOSING_IDLE:
        if (lastPH > target_pH_hi) {
          acidState = DOSING_ACTIVE;
          acidDosingStart = millis();
          digitalWrite(PUMP4_PIN, HIGH);
          Serial.println("[DOSE] Acid dosing started");
        }
        break;
      case DOSING_ACTIVE:
        if (millis() - acidDosingStart > DOSING_PULSE) {
          digitalWrite(PUMP4_PIN, LOW);
          acidState = DOSING_WAIT_MIX;
          acidDosingStart = millis();
          Serial.println("[DOSE] Acid dosing mixing...");
        }
        break;
      case DOSING_WAIT_MIX:
        if (millis() - acidDosingStart > DOSING_MIX) {
          if (lastPH > target_pH_hi) {
            acidState = DOSING_ACTIVE;
            acidDosingStart = millis();
            digitalWrite(PUMP4_PIN, HIGH);
            Serial.println("[DOSE] Acid dosing pulse again");
          } else {
            acidState = DOSING_IDLE;
            Serial.println("[DOSE] Acid dosing complete");
          }
        }
        break;
    }
  }

  if (!baseError) {
    switch (baseState) {
      case DOSING_IDLE:
        if (lastPH < target_pH_lo) {
          baseState = DOSING_ACTIVE;
          baseDosingStart = millis();
          digitalWrite(PUMP5_PIN, HIGH);
          Serial.println("[DOSE] Base dosing started");
        }
        break;
      case DOSING_ACTIVE:
        if (millis() - baseDosingStart > DOSING_PULSE) {
          digitalWrite(PUMP5_PIN, LOW);
          baseState = DOSING_WAIT_MIX;
          baseDosingStart = millis();
          Serial.println("[DOSE] Base dosing mixing...");
        }
        break;
      case DOSING_WAIT_MIX:
        if (millis() - baseDosingStart > DOSING_MIX) {
          if (lastPH < target_pH_lo) {
            baseState = DOSING_ACTIVE;
            baseDosingStart = millis();
            digitalWrite(PUMP5_PIN, HIGH);
            Serial.println("[DOSE] Base dosing pulse again");
          } else {
            baseState = DOSING_IDLE;
            Serial.println("[DOSE] Base dosing complete");
          }
        }
        break;
    }
  }

  if (!nutrError) {
    switch (nutrState) {
      case DOSING_IDLE:
        if (lastEC < target_ec) {
          nutrState = DOSING_ACTIVE;
          nutrDosingStart = millis();
          digitalWrite(PUMP6_PIN, HIGH);
          Serial.println("[DOSE] Nutrient dosing started");
        }
        break;
      case DOSING_ACTIVE:
        if (millis() - nutrDosingStart > DOSING_PULSE) {
          digitalWrite(PUMP6_PIN, LOW);
          nutrState = DOSING_WAIT_MIX;
          nutrDosingStart = millis();
          Serial.println("[DOSE] Nutrient dosing mixing...");
        }
        break;
      case DOSING_WAIT_MIX:
        if (millis() - nutrDosingStart > DOSING_MIX) {
          if (lastEC < target_ec) {
            nutrState = DOSING_ACTIVE;
            nutrDosingStart = millis();
            digitalWrite(PUMP6_PIN, HIGH);
            Serial.println("[DOSE] Nutrient dosing pulse again");
          } else {
            nutrState = DOSING_IDLE;
            Serial.println("[DOSE] Nutrient dosing complete");
          }
        }
        break;
    }
  }
}

// ======================== SAFETY CHECKS ========================
void runSafetyChecks() {
  anyFault = acidError || baseError || nutrError || waterError;

  if (readUltrasonicCM(TRIG_WATER, ECHO_WATER) < 0) {
    waterError = true;
    Serial.println("[FAULT] Water tank ultrasonic sensor error!");
  }
  if (readUltrasonicCM(TRIG_ACID, ECHO_ACID) < 0) {
    acidError = true;
    Serial.println("[FAULT] Acid tank ultrasonic sensor error!");
  }
  if (readUltrasonicCM(TRIG_BASE, ECHO_BASE) < 0) {
    baseError = true;
    Serial.println("[FAULT] Base tank ultrasonic sensor error!");
  }
  if (readUltrasonicCM(TRIG_NUTRIENT, ECHO_NUTRIENT) < 0) {
    nutrError = true;
    Serial.println("[FAULT] Nutrient tank ultrasonic sensor error!");
  }

  digitalWrite(LED_FAULT, anyFault ? HIGH : LOW);
}

// ======================== LIGHTING & UV CONTROL ========================
void updateLightingControl() {
  static unsigned long lightingTimer = 0;
  static bool lightsOn = false;
  int ldrVal = analogRead(LDR_PIN);

  if (lightsOn) {
    // If lights are ON, check if 1 min has passed
    if (millis() - lightingTimer >= 60000) { // 1 min
      analogWrite(LEDS_MOSFET_GATE_PIN, 0); // Turn OFF
      lightsOn = false;
      lightingTimer = millis(); // Start 1 sec OFF period
      Serial.println("[LIGHTS] Turned OFF for LDR check");
    }
  } else {
    // Lights are OFF; wait 1 second before checking LDR and deciding
    if (millis() - lightingTimer >= 1000) { // 1 sec
      if (ldrVal < targetLDR - 10) {
        analogWrite(LEDS_MOSFET_GATE_PIN, 255); // Turn ON
        lightsOn = true;
        lightingTimer = millis(); // Start 1 min ON period
        Serial.println("[LIGHTS] Turned ON (dark detected)");
      } else {
        // Remain OFF, keep checking every second
        lightingTimer = millis();
        Serial.println("[LIGHTS] Remain OFF (light OK)");
      }
    }
  }
}
void updateUVControl() {
  static unsigned long lastUV = 0;
  const unsigned long UV_ON_DURATION = 5 * 60 * 1000;
  const unsigned long UV_OFF_DURATION = 55 * 60 * 1000;
  static bool uvOn = false;
  unsigned long now = millis();

  if (!uvOn && now - lastUV > UV_OFF_DURATION) {
    digitalWrite(UV1_PIN, HIGH); digitalWrite(UV2_PIN, HIGH);
    uvOn = true;
    lastUV = now;
  } else if (uvOn && now - lastUV > UV_ON_DURATION) {
    digitalWrite(UV1_PIN, LOW);
    digitalWrite(UV2_PIN, LOW);
    uvOn = false;
    lastUV = now;
  }
}