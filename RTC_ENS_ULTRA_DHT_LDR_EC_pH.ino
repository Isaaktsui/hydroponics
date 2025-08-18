#include <Wire.h>
#include "DFRobot_SD3031.h"
#include "DFRobot_ENS160.h"
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

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

// ----- DHT22 Pin & Type -----
#define DHTPIN 30        // DHT22 data pin connected to Mega pin 30 (change as needed)
#define DHTTYPE DHT22
DHT_Unified dht(DHTPIN, DHTTYPE);

// ----- LDR & Analog Sensors -----
#define LDR_PIN A0       // LDR connected to analog pin A0
#define PH_PIN  A1       // pH sensor on A1
#define EC_PIN  A2       // EC sensor on A2

// EC Circuit Constants
const float EC_R_KNOWN = 10000.0; // 10k resistor (ohms)

// Instantiate RTC and ENS160 objects
DFRobot_SD3031 rtc;
DFRobot_ENS160_I2C ens160(&Wire, ENS160_I2C_ADDR);

bool promptForRTC = false;
String inputBuffer = "";

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

  // DHT22 init (Unified API)
  dht.begin();
  Serial.println("DHT22 sensor initialized.");

  // Ultrasonic sensors init
  setupUltrasonics();
  Serial.println("Ultrasonic level sensors initialized.");

  // LDR, pH, EC pins init
  pinMode(LDR_PIN, INPUT);
  pinMode(PH_PIN, INPUT);
  pinMode(EC_PIN, INPUT);
  Serial.println("LDR, pH, and EC sensors initialized.");

  Serial.println("Type 'setrtc' and press Enter to set the RTC date and time.");
}

// ----- LOOP -----
void loop() {
  // -------- Serial input for RTC setting --------
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (inputBuffer.length() > 0) {
        if (promptForRTC) {
          handleRTCInput(inputBuffer);
        } else if (inputBuffer.equalsIgnoreCase("setrtc")) {
          promptSetRTC();
        }
        inputBuffer = "";
      }
    } else if (isPrintable(c)) {
      inputBuffer += c;
    }
  }

  // -------- Regular sensor data output --------
  static unsigned long lastPrint = 0;
  if (!promptForRTC && millis() - lastPrint >= 1000) {
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

    // LDR (Light Level)
    int ldrValue = analogRead(LDR_PIN); // Range: 0 (dark) to 1023 (bright)
    Serial.print("LDR: ");
    Serial.print(ldrValue);
    Serial.print(" | ");

    // pH Sensor
    int phRaw = analogRead(PH_PIN);
    Serial.print("pH ADC: ");
    Serial.print(phRaw);
    // Placeholder: convert to pH units if calibration known
    Serial.print(" | ");

    // EC Sensor
    int ecRaw = analogRead(EC_PIN);
    float ecVout = ecRaw * (5.0 / 1023.0); // ADC to voltage
    float ecRprobe = (ecVout * EC_R_KNOWN) / (5.0 - ecVout);
    float ecMicroSiemens = 0;
    if (ecRprobe > 0) ecMicroSiemens = 1000000.0 / ecRprobe; // Very rough!
    Serial.print("EC ADC: ");
    Serial.print(ecRaw);
    Serial.print(" Vout: ");
    Serial.print(ecVout, 3);
    Serial.print(" V R_probe: ");
    Serial.print(ecRprobe, 1);
    Serial.print(" ohms EC: ");
    Serial.print(ecMicroSiemens, 1);
    Serial.print(" uS/cm | ");

    // Ultrasonics (Water, Acid, Base, Nutrient)
    printUltrasonicLevels();
  }
}