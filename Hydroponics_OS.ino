// ========================
// SECTION 1: INCLUDES, PIN DEFINITIONS, GLOBAL OBJECTS
// ========================
#include <Wire.h>
#include <Servo.h>
#include <DHT.h>
#include <Adafruit_Sensor.h>
#include "DFRobot_SD3031.h"
#include "DFRobot_ENS160.h"

// Ultrasonic Tank Sensors
#define TRIG_WATER      22
#define ECHO_WATER      23
#define TRIG_ACID       24
#define ECHO_ACID       25
#define TRIG_BASE       26   
#define ECHO_BASE       27
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
#define LEDS_MOSFET_GATE_PIN 5
#define UV1_PIN   7
#define UV2_PIN   8
#define FAN1_PWM_PIN 9
#define FAN2_PWM_PIN 10
#define SERVO1_PIN 11
#define SERVO2_PIN 12
#define PUMP1_PIN 31
#define PUMP2_PIN 32
#define PUMP3_PIN 33
#define PUMP4_PIN 34
#define PUMP5_PIN 35
#define PUMP6_PIN 36

// Peltiers (L293D)
#define PELTIER1_EN 44    // PWM
#define PELTIER1_IN1 37
#define PELTIER1_IN2 38
#define PELTIER2_EN 45    // PWM
#define PELTIER2_IN3 39
#define PELTIER2_IN4 40

Servo servo1, servo2;
DHT dht(DHTPIN, DHTTYPE);
DFRobot_SD3031 rtc;
DFRobot_ENS160_I2C ens160(&Wire, 0x53); // Default I2C address

String inputBuffer = "";
bool promptForRTC = false;

// Tank heights
#define WATER_TANK_HEIGHT_CM     30.0
#define ACID_TANK_HEIGHT_CM      25.0
#define BASE_TANK_HEIGHT_CM      25.0
#define NUTRIENT_TANK_HEIGHT_CM  20.0
const float EC_R_KNOWN = 10000.0; // 10k resistor (ohms)

// ========================
// SECTION 2: SETUP & ACTUATOR INIT
// ========================
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
  pinMode(PELTIER1_EN, OUTPUT); pinMode(PELTIER1_IN1, OUTPUT); pinMode(PELTIER1_IN2, OUTPUT);
  pinMode(PELTIER2_EN, OUTPUT); pinMode(PELTIER2_IN3, OUTPUT); pinMode(PELTIER2_IN4, OUTPUT);
  setL293D(PELTIER1_EN, PELTIER1_IN1, PELTIER1_IN2, 'o', 0);
  setL293D(PELTIER2_EN, PELTIER2_IN3, PELTIER2_IN4, 'o', 0);
}

// Helper: set L293D channel (EN, IN1, IN2)
void setL293D(int en, int inA, int inB, char dir, int pwm) {
  pwm = constrain(pwm, 0, 255);
  analogWrite(en, pwm);
  if (dir == 'f') { digitalWrite(inA, HIGH); digitalWrite(inB, LOW); }
  else if (dir == 'r') { digitalWrite(inA, LOW); digitalWrite(inB, HIGH); }
  else { digitalWrite(inA, LOW); digitalWrite(inB, LOW); analogWrite(en, 0); }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  Wire.begin();
  if (rtc.begin() != 0) { Serial.println("RTC not found!"); while(1); }
  Serial.println("RTC initialized.");
  if (ens160.begin() != 0) { Serial.println("ENS160 not found!"); while(1); }
  Serial.println("ENS160 sensor initialized.");
  dht.begin(); Serial.println("DHT22 sensor initialized.");
  setupUltrasonics(); Serial.println("Ultrasonic level sensors initialized.");
  pinMode(LDR_PIN, INPUT); pinMode(PH_PIN, INPUT); pinMode(EC_PIN, INPUT);
  Serial.println("LDR, pH, EC sensors initialized.");
  actuatorInit();
  Serial.println("Type 'setrtc' and press Enter to set RTC.");
}

// ========================
// SECTION 3: MAIN LOOP (HIGH-LEVEL SCHEDULER)
// ========================
void loop() {
  // --- Serial input for RTC and debug commands ---
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (inputBuffer.length() > 0) {
        if (promptForRTC) {
          handleRTCInput(inputBuffer);
        } else if (inputBuffer.equalsIgnoreCase("setrtc")) {
          promptSetRTC();
        } // Add more serial commands here as needed
        inputBuffer = "";
      }
    } else if (isPrintable(c)) {
      inputBuffer += c;
    }
  }

  // --- Timed main loop logic ---
  static unsigned long lastPrint = 0;
  if (!promptForRTC && millis() - lastPrint >= 1000) {
    lastPrint = millis();

    // Print status
    printSensorStatus();

    // Run control logic
    updateClimateControl();
    updateHydraulicControl();
    // Add: updateLightingControl();
    // Add: updateNutrientDosing();
    // Add: runSafetyChecks();
  }
}

// ========================
// SECTION 4: SENSOR/ACTUATOR STATUS PRINT (REFERENCES REPO SENSOR PRINT ROUTINES)
// ========================
void printSensorStatus() {
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

  // DHT22 (Unified Sensor API)
  sensors_event_t event;
  dht.temperature().getEvent(&event);
  Serial.print(" DHT22 Temp: ");
  if (isnan(event.temperature)) {
    Serial.print("Sensor error");
  } else {
    Serial.print(event.temperature, 1); Serial.print(" C, ");
  }
  dht.humidity().getEvent(&event);
  Serial.print("Humidity: ");
  if (isnan(event.relative_humidity)) {
    Serial.print("Sensor error");
  } else {
    Serial.print(event.relative_humidity, 1); Serial.print(" %");
  }
  Serial.print(" | ");

  int ldrValue = analogRead(LDR_PIN);
  Serial.print("LDR: "); Serial.print(ldrValue); Serial.print(" | ");

  int phRaw = analogRead(PH_PIN);
  Serial.print("pH ADC: "); Serial.print(phRaw); Serial.print(" | ");

  int ecRaw = analogRead(EC_PIN);
  float ecVout = ecRaw * (5.0 / 1023.0); // ADC to voltage
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

// ========================
// SECTION 5: CLIMATE CONTROL LOGIC (WITH PELTIER, FAN, SERVO)
// ========================
const float RH_HIGH_THRESHOLD = 80.0;
const float RH_LOW_THRESHOLD = 55.0;
const float TEMP_HIGH_THRESHOLD = 26.0;
const float TEMP_LOW_THRESHOLD = 18.0;
const uint16_t CO2_LOW_THRESHOLD = 600;

void updateClimateControl() {
  sensors_event_t tempEvent, humEvent;
  dht.temperature().getEvent(&tempEvent);
  dht.humidity().getEvent(&humEvent);
  float RH = humEvent.relative_humidity;
  float temp = tempEvent.temperature;
  uint16_t eco2 = ens160.getECO2();

  bool hatchOpen = false;
  bool fanOn = false;
  bool peltierCool = false;
  bool misterOn = false;

  if (RH > RH_HIGH_THRESHOLD) {
    hatchOpen = true; fanOn = true; peltierCool = true;
  } else if (RH < RH_LOW_THRESHOLD) {
    misterOn = true; peltierCool = false;
  } else {
    peltierCool = false;
  }

  if (eco2 < CO2_LOW_THRESHOLD) {
    hatchOpen = true; fanOn = true;
  }

  if (temp > TEMP_HIGH_THRESHOLD) {
    hatchOpen = true; fanOn = true; peltierCool = true;
  } else if (temp < TEMP_LOW_THRESHOLD) {
    // Optionally: run heater here, or reverse Peltier if hardware allows.
  }

  if (hatchOpen) { servo1.write(180); servo2.write(180); }
  else { servo1.write(0); servo2.write(0); }
  if (fanOn) {
    analogWrite(FAN1_PWM_PIN, 255);
    analogWrite(FAN2_PWM_PIN, 255);
  } else {
    analogWrite(FAN1_PWM_PIN, 0);
    analogWrite(FAN2_PWM_PIN, 0);
  }
  if (peltierCool) {
    setL293D(PELTIER1_EN, PELTIER1_IN1, PELTIER1_IN2, 'f', 255);
    setL293D(PELTIER2_EN, PELTIER2_IN3, PELTIER2_IN4, 'f', 255);
  } else {
    setL293D(PELTIER1_EN, PELTIER1_IN1, PELTIER1_IN2, 'o', 0);
    setL293D(PELTIER2_EN, PELTIER2_IN3, PELTIER2_IN4, 'o', 0);
  }
  if (misterOn) {
    // Add mister control here
  }
}

// ========================
// SECTION 6: HYDRAULIC CONTROL LOGIC (STUB)
// ========================
void updateHydraulicControl() {
  // TODO: Implement tray fill/drain scheduling, pump activation, etc., as per system requirements.
}

// ========================
// SECTION 7: SUPPORT ROUTINES
// ========================
void promptSetRTC() {
  Serial.println();
  Serial.println("Set RTC Date and Time.");
  Serial.println("Send in this format: YYYY-MM-DD HH:MM:SS");
  Serial.println("Example: 2025-06-04 23:59:00");
  Serial.print("> ");
  promptForRTC = true;
}
void handleRTCInput(String line) {
  int y, mo, d, h, mi, s;
  if (sscanf(line.c_str(), "%d-%d-%d %d:%d:%d", &y, &mo, &d, &h, &mi, &s) == 6) {
    rtc.setTime(y, mo, d, h, mi, s);
    Serial.println("RTC set successfully!");
  } else {
    Serial.println("Invalid format! Please use: YYYY-MM-DD HH:MM:SS");
  }
  promptForRTC = false;
}

// ========================
// SECTION 8: ULTRASONIC PRINT
// ========================
float readUltrasonicCM(uint8_t trigPin, uint8_t echoPin) {
  digitalWrite(trigPin, LOW); delayMicroseconds(2);
  digitalWrite(trigPin, HIGH); delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, 30000); // 30ms timeout
  float distance = duration * 0.0343 / 2.0;
  return (duration == 0) ? -1 : distance;
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
}
// ========================
// SECTION 9: HYDRAULIC CONTROL LOGIC (TRAY AND TANK PUMPS)
// ========================
// Implements the water flow automation as described in your system architecture.
// - Acid, base, and nutrient pumps are controlled independently for dosing (expand logic as needed).
// - Main water tank->top tray->bottom tray->water tank loop is managed for ebb & flow cycles.

enum HydroState {
  IDLE,
  FILL_TOP_TRAY,
  DRAIN_TOP_TO_BOTTOM,
  DRAIN_BOTTOM_TO_TANK
};

HydroState hydroState = IDLE;
unsigned long hydroStateStart = 0;
const unsigned long TOP_TRAY_FILL_TIME = 30 * 1000;         // ms - adjust as needed for your pump speed
const unsigned long TOP_TO_BOTTOM_DRAIN_TIME = 30 * 1000;   // ms - adjust as needed
const unsigned long BOTTOM_TO_TANK_DRAIN_TIME = 30 * 1000;  // ms - adjust as needed
const unsigned long HYDRO_IDLE_TIME = 10 * 60 * 1000;       // ms (10 min between cycles)

void updateHydraulicControl() {
  unsigned long now = millis();

  switch (hydroState) {
    case IDLE:
      if (now - hydroStateStart >= HYDRO_IDLE_TIME) {
        // Start new cycle: fill top tray from water tank
        digitalWrite(PUMP1_PIN, HIGH);  // Water tank -> top tray ON
        hydroState = FILL_TOP_TRAY;
        hydroStateStart = now;
        Serial.println("[HYDRO] Starting fill top tray.");
      }
      break;

    case FILL_TOP_TRAY:
      if (now - hydroStateStart >= TOP_TRAY_FILL_TIME) {
        digitalWrite(PUMP1_PIN, LOW);   // Stop fill
        digitalWrite(PUMP2_PIN, HIGH);  // Top tray -> bottom tray ON
        hydroState = DRAIN_TOP_TO_BOTTOM;
        hydroStateStart = now;
        Serial.println("[HYDRO] Draining top tray to bottom tray.");
      }
      break;

    case DRAIN_TOP_TO_BOTTOM:
      if (now - hydroStateStart >= TOP_TO_BOTTOM_DRAIN_TIME) {
        digitalWrite(PUMP2_PIN, LOW);   // Stop drain
        digitalWrite(PUMP3_PIN, HIGH);  // Bottom tray -> water tank ON
        hydroState = DRAIN_BOTTOM_TO_TANK;
        hydroStateStart = now;
        Serial.println("[HYDRO] Draining bottom tray to main tank.");
      }
      break;

    case DRAIN_BOTTOM_TO_TANK:
      if (now - hydroStateStart >= BOTTOM_TO_TANK_DRAIN_TIME) {
        digitalWrite(PUMP3_PIN, LOW);   // Stop all movement
        hydroState = IDLE;
        hydroStateStart = now;
        Serial.println("[HYDRO] Cycle complete, going idle.");
      }
      break;
  }

  // Add acid/base/nutrient dosing logic here as needed.
  // Example: Call doseAcid(), doseBase(), doseNutrient() based on pH/EC readings and schedules.
}

// ========================
// SECTION 10: DOSING CONTROL (STUBS FOR pH AND NUTRIENT)
// ========================
// These are placeholder routines. Fill in the logic based on your dosing requirements.

void doseAcid() {
  // Example: Pulse PUMP4_PIN for a fixed time if pH > max threshold
  // digitalWrite(PUMP4_PIN, HIGH); delay(500); digitalWrite(PUMP4_PIN, LOW);
}
void doseBase() {
  // Example: Pulse PUMP5_PIN for a fixed time if pH < min threshold
}
void doseNutrient() {
  // Example: Pulse PUMP6_PIN for a fixed time if EC < threshold
}

// ========================
// SECTION 11: SAFETY AND FAULT MONITORING (EXAMPLES)
// ========================
void runSafetyChecks() {
  // Example: Water tank low protection
  float waterSensorDist = readUltrasonicCM(TRIG_WATER, ECHO_WATER);
  if (waterSensorDist > WATER_TANK_HEIGHT_CM - 3.0) { // 3cm from bottom
    // Water tank nearly empty! Stop all pumps.
    digitalWrite(PUMP1_PIN, LOW);
    digitalWrite(PUMP2_PIN, LOW);
    digitalWrite(PUMP3_PIN, LOW);
    Serial.println("[SAFETY] Water tank LOW! All pumps OFF!");
    // Optionally: Blink LEDs, alarm, etc.
  }

  // Add more safety/fault logic: e.g., sensor error detection, overheat, etc.
}

// ========================
// SECTION 12: LIGHTING CONTROL (SIMPLE EXAMPLE)
// ========================
void updateLightingControl() {
  // Example: Turn on LEDs based on LDR (ambient light) or fixed schedule.
  int ldrVal = analogRead(LDR_PIN);
  if (ldrVal < 200) { // Dark, turn on LEDs
    analogWrite(LEDS_MOSFET_GATE_PIN, 255);
  } else {
    analogWrite(LEDS_MOSFET_GATE_PIN, 0);
  }
}

// ========================
// SECTION 13: UV STERILIZER CONTROL (BASIC EXAMPLE)
// ========================
void updateUVControl() {
  // Example: Run UV LEDs for a period each hour, or during circulation
  static unsigned long lastUV = 0;
  const unsigned long UV_ON_DURATION = 5 * 60 * 1000; // 5 min
  const unsigned long UV_OFF_DURATION = 55 * 60 * 1000; // 55 min
  static bool uvOn = false;
  unsigned long now = millis();

  if (!uvOn && now - lastUV > UV_OFF_DURATION) {
    digitalWrite(UV1_PIN, HIGH); digitalWrite(UV2_PIN, HIGH);
    uvOn = true;
    lastUV = now;
  } else if (uvOn && now - lastUV > UV_ON_DURATION) {
    digitalWrite(UV1_PIN, LOW); digitalWrite(UV2_PIN, LOW);
    uvOn = false;
    lastUV = now;
  }
}