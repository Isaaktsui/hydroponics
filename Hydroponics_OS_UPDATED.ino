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
#define MISTER_PIN 41      // Stand-in for mister MOSFET
#define WIFI_RX_PIN 18     // WiFiBee ESP8266 RX
#define WIFI_TX_PIN 19     // WiFiBee ESP8266 TX

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

// Mister piecemeal logic
static unsigned long lastMister = 0;
static bool misterActive = false;
const unsigned long MISTER_ON_TIME = 3000;   // 3s burst
const unsigned long MISTER_OFF_TIME = 10000; // 10s off

// Hysteresis logic
bool rhWasHigh = false;
bool eco2WasLow = false;
bool tempWasHigh = false;

// Piecemeal dosing state for acid/base/nutrient
enum DosingState { DOSING_IDLE, DOSING_ACTIVE, DOSING_WAIT_MIX };
DosingState acidState = DOSING_IDLE, baseState = DOSING_IDLE, nutrState = DOSING_IDLE;
unsigned long dosingStart = 0;
const unsigned long DOSING_PULSE = 1000;     // ms pump ON per dose
const unsigned long DOSING_MIX = 12000;      // ms wait after dose

float lastPH = 7.0, lastEC = 1.0; // For feedback; should be actual readings

// Prompt/priming
bool tankPrompted = false, tankConfirmed = false, traysPrimed = false;

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
  pinMode(MISTER_PIN, OUTPUT); digitalWrite(MISTER_PIN, LOW);
}

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
  Serial1.begin(115200); // WiFiBee ESP8266 on Serial1
  Serial1.println("ESP8266 WiFiBee test message");
  Serial.println("Type 'setrtc' and press Enter to set RTC.");
  Serial.println("Please fill all tanks and premix the main reservoir. Type 'ready' to continue.");
  tankPrompted = true;
}

// ========================
// SECTION 3: MAIN LOOP (INIT, SERIAL & TESTS)
// ========================
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
    Serial.println("Tanks filled, beginning tray priming...");
    tankConfirmed = true;
  }
}

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
    return;
  }
  if (tankConfirmed && !traysPrimed) {
    Serial.println("Trays primed. Entering main operating cycle.");
    traysPrimed = true;
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
  }
}

// ========================
// SECTION 4: SENSOR/ACTUATOR STATUS PRINT (UNCHANGED)
// ========================
// ... (No changes to printSensorStatus, printUltrasonicLevels) ...
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
// SECTION 5: CLIMATE CONTROL LOGIC (HYSTERESIS, MISTER BURST)
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

  bool rhHigh = RH > RH_HIGH_THRESHOLD;
  bool rhLow = RH < RH_LOW_THRESHOLD;
  bool eco2Low = eco2 < CO2_LOW_THRESHOLD;
  bool tempHigh = temp > TEMP_HIGH_THRESHOLD;

  bool hatchOpen = rhHigh || eco2Low || tempHigh;
  bool fanOn = hatchOpen;
  bool peltierCool = rhHigh || tempHigh;
  bool misterOn = rhLow;

  // Actuate
  if (hatchOpen) { servo1.write(180); servo2.write(180); }
  else { servo1.write(0); servo2.write(0); }
  analogWrite(FAN1_PWM_PIN, fanOn ? 255 : 0);
  analogWrite(FAN2_PWM_PIN, fanOn ? 255 : 0);
  setL293D(PELTIER1_EN, PELTIER1_IN1, PELTIER1_IN2, peltierCool ? 'f' : 'o', peltierCool ? 255 : 0);
  setL293D(PELTIER2_EN, PELTIER2_IN3, PELTIER2_IN4, peltierCool ? 'f' : 'o', peltierCool ? 255 : 0);

  // Mister piecemeal burst logic
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
// ========================
// SECTION 6: HYDRAULIC CONTROL LOGIC (REMOVED - see Section 9)
// ========================

// ========================
// SECTION 7: SUPPORT ROUTINES (RTC/menu)
// ========================
// Already present and sufficient. Expand if more support commands are needed.

// ========================
// SECTION 8: ULTRASONIC PRINT (with overflow/underflow warnings)
// ========================
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
    if (waterLevel < 3.0) Serial.print(" [LOW]");
    if (waterLevel > WATER_TANK_HEIGHT_CM - 1.0) Serial.print(" [FULL/Overflow!]");
  }
  // ... repeat for acid, base, nutrient ...
  // (Optional: add [LOW] and [FULL] flags for each tank)
}

// ========================
// SECTION 9: HYDRAULIC CONTROL LOGIC (TRAY AND TANK PUMPS, WITH PRIMING AND OVERFLOW PROTECTION)
// ========================

enum HydroState {
  IDLE,
  FILL_TOP_TRAY,
  DRAIN_TOP_TO_BOTTOM,
  DRAIN_BOTTOM_TO_TANK,
  PRIMING_FILL_TOP,
  PRIMING_DRAIN_TOP,
  PRIMING_DRAIN_BOTTOM
};

HydroState hydroState = IDLE;
unsigned long hydroStateStart = 0;
const unsigned long TOP_TRAY_FILL_TIME = 30 * 1000;         // ms
const unsigned long TOP_TO_BOTTOM_DRAIN_TIME = 30 * 1000;   // ms
const unsigned long BOTTOM_TO_TANK_DRAIN_TIME = 30 * 1000;  // ms
const unsigned long HYDRO_IDLE_TIME = 10 * 60 * 1000;       // ms

// Add overflow/underflow protection
bool waterTankLow() {
  float w = readUltrasonicCM(TRIG_WATER, ECHO_WATER);
  return (w < 0) || ((WATER_TANK_HEIGHT_CM - w) < 3.0);
}
bool waterTankOverflow() {
  float w = readUltrasonicCM(TRIG_WATER, ECHO_WATER);
  return (w < 0) || ((WATER_TANK_HEIGHT_CM - w) > WATER_TANK_HEIGHT_CM - 1.0);
}

void startPrimingCycle() {
  hydroState = PRIMING_FILL_TOP;
  hydroStateStart = millis();
  Serial.println("[HYDRO] Priming: filling top tray.");
  digitalWrite(PUMP1_PIN, HIGH);
}

void updateHydraulicControl() {
  unsigned long now = millis();

  // If water tank is low, stop all pumps and return
  if (waterTankLow()) {
    digitalWrite(PUMP1_PIN, LOW); digitalWrite(PUMP2_PIN, LOW); digitalWrite(PUMP3_PIN, LOW);
    if (hydroState != IDLE) Serial.println("[HYDRO] Water tank low; pumps stopped!");
    hydroState = IDLE;
    return;
  }

  // Priming sequence (run after user confirmation)
  if (!traysPrimed && tankConfirmed) {
    startPrimingCycle();
    traysPrimed = true;
    return;
  }

  // Priming state machine (runs once after startup)
  if (hydroState == PRIMING_FILL_TOP) {
    if (now - hydroStateStart >= TOP_TRAY_FILL_TIME || waterTankOverflow()) {
      digitalWrite(PUMP1_PIN, LOW);
      digitalWrite(PUMP2_PIN, HIGH);
      hydroState = PRIMING_DRAIN_TOP;
      hydroStateStart = now;
      Serial.println("[HYDRO] Priming: draining top tray to bottom tray.");
    }
    return;
  }
  if (hydroState == PRIMING_DRAIN_TOP) {
    if (now - hydroStateStart >= TOP_TO_BOTTOM_DRAIN_TIME) {
      digitalWrite(PUMP2_PIN, LOW);
      digitalWrite(PUMP3_PIN, HIGH);
      hydroState = PRIMING_DRAIN_BOTTOM;
      hydroStateStart = now;
      Serial.println("[HYDRO] Priming: draining bottom tray to tank.");
    }
    return;
  }
  if (hydroState == PRIMING_DRAIN_BOTTOM) {
    if (now - hydroStateStart >= BOTTOM_TO_TANK_DRAIN_TIME) {
      digitalWrite(PUMP3_PIN, LOW);
      hydroState = IDLE;
      Serial.println("[HYDRO] Priming complete.");
    }
    return;
  }

  // Main cycle state machine
  switch (hydroState) {
    case IDLE:
      if (now - hydroStateStart >= HYDRO_IDLE_TIME) {
        digitalWrite(PUMP1_PIN, HIGH);
        hydroState = FILL_TOP_TRAY;
        hydroStateStart = now;
        Serial.println("[HYDRO] Starting fill top tray.");
      }
      break;
    case FILL_TOP_TRAY:
      if (now - hydroStateStart >= TOP_TRAY_FILL_TIME || waterTankOverflow()) {
        digitalWrite(PUMP1_PIN, LOW);
        digitalWrite(PUMP2_PIN, HIGH);
        hydroState = DRAIN_TOP_TO_BOTTOM;
        hydroStateStart = now;
        Serial.println("[HYDRO] Draining top tray to bottom tray.");
      }
      break;
    case DRAIN_TOP_TO_BOTTOM:
      if (now - hydroStateStart >= TOP_TO_BOTTOM_DRAIN_TIME) {
        digitalWrite(PUMP2_PIN, LOW);
        digitalWrite(PUMP3_PIN, HIGH);
        hydroState = DRAIN_BOTTOM_TO_TANK;
        hydroStateStart = now;
        Serial.println("[HYDRO] Draining bottom tray to main tank.");
      }
      break;
    case DRAIN_BOTTOM_TO_TANK:
      if (now - hydroStateStart >= BOTTOM_TO_TANK_DRAIN_TIME) {
        digitalWrite(PUMP3_PIN, LOW);
        hydroState = IDLE;
        hydroStateStart = now;
        Serial.println("[HYDRO] Cycle complete, going idle.");
      }
      break;
    default:
      break;
  }
}
// ========================
// SECTION 10: PIECEWISE DOSING (ACID EXAMPLE)
// ========================
void runDosingControl() {
  if (acidState == DOSING_IDLE && lastPH > 7.1) {
    acidState = DOSING_ACTIVE;
    dosingStart = millis();
    digitalWrite(PUMP4_PIN, HIGH);
    Serial.println("[DOSE] Acid dosing started");
  } else if (acidState == DOSING_ACTIVE && millis() - dosingStart > DOSING_PULSE) {
    digitalWrite(PUMP4_PIN, LOW);
    acidState = DOSING_WAIT_MIX;
    dosingStart = millis();
    Serial.println("[DOSE] Acid dosing mixing...");
  } else if (acidState == DOSING_WAIT_MIX && millis() - dosingStart > DOSING_MIX) {
    if (lastPH > 7.1) {
      acidState = DOSING_ACTIVE;
      dosingStart = millis();
      digitalWrite(PUMP4_PIN, HIGH);
      Serial.println("[DOSE] Acid dosing pulse again");
    } else {
      acidState = DOSING_IDLE;
      Serial.println("[DOSE] Acid dosing complete");
    }
  }
  // Add similar for base/nutrient as needed
}

// ========================
// SECTION 11: SAFETY AND FAULT MONITORING (UNCHANGED)
// ========================
// ... (runSafetyChecks unchanged) ...
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
// SECTION 12: LIGHTING CONTROL (UNCHANGED)
// ========================
// ... (updateLightingControl unchanged) ...
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
// SECTION 13: UV STERILIZER CONTROL (UNCHANGED)
// ========================
// ... (updateUVControl unchanged) ...
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
// ========================
// SECTION 14: WATCHDOG TIMER
// ========================
#include <avr/wdt.h>
void enableWatchdog() {
  wdt_enable(WDTO_8S);
}
void resetWatchdog() {
  wdt_reset();
}
// In setup(), add: enableWatchdog();
// At end of loop(), add: resetWatchdog();