#include <Wire.h>
#include <DFRobot_PH.h>
#include <EEPROM.h>
#include <DHT.h>
#include <Servo.h>
#include <NewPing.h>
#include <RTClib.h>

// ==== Pin Definitions ====
// (same as before, update as needed)
#define PH_SENSOR_PIN          A0
#define ACID_PUMP_PIN          22
#define BASE_PUMP_PIN          23
#define NUTRIENT_PUMP_PIN      24

#define WATER_PUMP_TOPTRAY_PIN    36
#define WATER_PUMP_TOP2BOTTOM_PIN 37
#define WATER_PUMP_BOTTOM2RES_PIN 38

#define EC_SENSOR_PIN          A1

#define DHT_PIN                2
#define DHT_TYPE               DHT22

#define MIST_MODULE_PIN        25
#define PELTIER1_PIN           26
#define PELTIER2_PIN           27
#define FAN1_PIN               28
#define FAN2_PIN               29
#define HATCH_SERVO1_PIN       3
#define HATCH_SERVO2_PIN       4

#define LDR_PIN                A4
#define LED_STRIP1_PWM         30
#define LED_STRIP2_PWM         31

#define TRIG1                  5
#define ECHO1                  6
#define TRIG2                  7
#define ECHO2                  8
#define TRIG3                  9
#define ECHO3                  10
#define TRIG4                  11
#define ECHO4                  12
#define LOW_LEVEL_CM           10

#define UVC_LED1_PIN           34
#define UVC_LED2_PIN           35

#define LOG_INTERVAL           60000UL

#define PH_CHECK_INTERVAL      10000UL
#define EC_CHECK_INTERVAL      15000UL
#define DHT_CHECK_INTERVAL     7000UL
#define LIGHT_CHECK_INTERVAL   5000UL
#define LEVEL_CHECK_INTERVAL   20000UL
#define EBB_FLOW_INTERVAL      600000UL
#define EBB_FLOW_PUMP_TIME     120000UL

#define PH_MIN                 5.5
#define PH_MAX                 6.5
#define EC_MIN                 1.2
#define EC_MAX                 2.5
#define HUMIDITY_MIN           50.0
#define HUMIDITY_MAX           70.0
#define TEMP_MIN               18.0
#define TEMP_MAX               25.0
#define LUX_THRESHOLD          5000.0

// ==== Global objects and variables ====
// -- pH --
DFRobot_PH phSensor;
float phVoltage = 0, phValue = 0;
unsigned long lastPHCheck = 0;

// -- EC --
unsigned long lastECCheck = 0;

// -- DHT/Climate --
DHT dht(DHT_PIN, DHT_TYPE);
Servo hatchServo1, hatchServo2;
float lastHumidity = 0, lastTemp = 0;
bool hatchOpen = false;
unsigned long lastDHTCheck = 0;

// -- Lighting --
unsigned long lastLightCheck = 0;

// -- Water Level --
NewPing sonar1(TRIG1, ECHO1, 100);
NewPing sonar2(TRIG2, ECHO2, 100);
NewPing sonar3(TRIG3, ECHO3, 100);
NewPing sonar4(TRIG4, ECHO4, 100);
unsigned long lastLevelCheck = 0;

// -- Ebb & Flow --
unsigned long ebbFlowLastStart = 0;
int ebbFlowPhase = 0;
unsigned long ebbFlowPhaseStart = 0;

// -- Data Transmission --
unsigned long lastLogTime = 0;

// -- RTC --
RTC_DS3231 rtc;
DateTime now;

// ==== SETUP FUNCTIONS ====
void setupPH() {
  phSensor.begin();
  pinMode(ACID_PUMP_PIN, OUTPUT);
  pinMode(BASE_PUMP_PIN, OUTPUT);
  digitalWrite(ACID_PUMP_PIN, LOW);
  digitalWrite(BASE_PUMP_PIN, LOW);
}

void setupNutrient() {
  pinMode(NUTRIENT_PUMP_PIN, OUTPUT);
  digitalWrite(NUTRIENT_PUMP_PIN, LOW);
}

void setupEbbFlow() {
  pinMode(WATER_PUMP_TOPTRAY_PIN, OUTPUT);
  pinMode(WATER_PUMP_TOP2BOTTOM_PIN, OUTPUT);
  pinMode(WATER_PUMP_BOTTOM2RES_PIN, OUTPUT);
  digitalWrite(WATER_PUMP_TOPTRAY_PIN, LOW);
  digitalWrite(WATER_PUMP_TOP2BOTTOM_PIN, LOW);
  digitalWrite(WATER_PUMP_BOTTOM2RES_PIN, LOW);
}

void setupClimate() {
  dht.begin();
  pinMode(MIST_MODULE_PIN, OUTPUT);
  pinMode(PELTIER1_PIN, OUTPUT);
  pinMode(PELTIER2_PIN, OUTPUT);
  pinMode(FAN1_PIN, OUTPUT);
  pinMode(FAN2_PIN, OUTPUT);
  digitalWrite(MIST_MODULE_PIN, LOW);
  digitalWrite(PELTIER1_PIN, LOW);
  digitalWrite(PELTIER2_PIN, LOW);
  digitalWrite(FAN1_PIN, LOW);
  digitalWrite(FAN2_PIN, LOW);
  hatchServo1.attach(HATCH_SERVO1_PIN);
  hatchServo2.attach(HATCH_SERVO2_PIN);
  closeHatch();
}

void setupLighting() {
  pinMode(LED_STRIP1_PWM, OUTPUT);
  pinMode(LED_STRIP2_PWM, OUTPUT);
  pinMode(LDR_PIN, INPUT);
  analogWrite(LED_STRIP1_PWM, 0);
  analogWrite(LED_STRIP2_PWM, 0);
}

void setupUVC() {
  pinMode(UVC_LED1_PIN, OUTPUT);
  pinMode(UVC_LED2_PIN, OUTPUT);
  digitalWrite(UVC_LED1_PIN, HIGH);
  digitalWrite(UVC_LED2_PIN, HIGH);
}

void setupRTC() {
  Wire.begin();
  rtc.begin();
  if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

// ==== SUPPORT FUNCTIONS ====
void openHatch() {
  hatchServo1.write(90);
  hatchServo2.write(90);
  hatchOpen = true;
}

void closeHatch() {
  hatchServo1.write(0);
  hatchServo2.write(0);
  hatchOpen = false;
}

// ==== CHECK FUNCTIONS ====
// -- pH --
void checkPH() {
  if (millis() - lastPHCheck >= PH_CHECK_INTERVAL) {
    lastPHCheck = millis();
    phVoltage = analogRead(PH_SENSOR_PIN) * (5.0 / 1023.0);
    phValue = phSensor.readPH(phVoltage, millis());
    if (phValue < PH_MIN) {
      digitalWrite(BASE_PUMP_PIN, HIGH);
      digitalWrite(ACID_PUMP_PIN, LOW);
    } else if (phValue > PH_MAX) {
      digitalWrite(ACID_PUMP_PIN, HIGH);
      digitalWrite(BASE_PUMP_PIN, LOW);
    } else {
      digitalWrite(ACID_PUMP_PIN, LOW);
      digitalWrite(BASE_PUMP_PIN, LOW);
    }
  }
}

// -- EC --
float readEC() {
  int raw = analogRead(EC_SENSOR_PIN);
  float voltage = raw * (5.0 / 1023.0);
  float ecValue = voltage * 2.0;
  return ecValue;
}
void checkEC() {
  if (millis() - lastECCheck >= EC_CHECK_INTERVAL) {
    lastECCheck = millis();
    float ec = readEC();
    if (ec < EC_MIN) {
      digitalWrite(NUTRIENT_PUMP_PIN, HIGH);
    } else {
      digitalWrite(NUTRIENT_PUMP_PIN, LOW);
    }
  }
}

// -- DHT/Climate --
void checkClimate() {
  if (millis() - lastDHTCheck >= DHT_CHECK_INTERVAL) {
    lastDHTCheck = millis();
    lastHumidity = dht.readHumidity();
    lastTemp = dht.readTemperature();

    if (lastHumidity < HUMIDITY_MIN) {
      digitalWrite(MIST_MODULE_PIN, HIGH);
      digitalWrite(PELTIER1_PIN, LOW);
      digitalWrite(PELTIER2_PIN, LOW);
      closeHatch();
      digitalWrite(FAN1_PIN, LOW);
      digitalWrite(FAN2_PIN, LOW);
    } else if (lastHumidity > HUMIDITY_MAX) {
      openHatch();
      digitalWrite(MIST_MODULE_PIN, LOW);
      digitalWrite(PELTIER1_PIN, HIGH);
      digitalWrite(PELTIER2_PIN, HIGH);
      if (hatchOpen) {
        digitalWrite(FAN1_PIN, HIGH);
        digitalWrite(FAN2_PIN, HIGH);
      }
    } else {
      digitalWrite(MIST_MODULE_PIN, LOW);
      digitalWrite(PELTIER1_PIN, LOW);
      digitalWrite(PELTIER2_PIN, LOW);
      digitalWrite(FAN1_PIN, LOW);
      digitalWrite(FAN2_PIN, LOW);
      closeHatch();
    }

    if (lastTemp < TEMP_MIN) {
      digitalWrite(PELTIER1_PIN, LOW);
      digitalWrite(PELTIER2_PIN, LOW);
      digitalWrite(FAN1_PIN, LOW);
      digitalWrite(FAN2_PIN, LOW);
      closeHatch();
    } else if (lastTemp > TEMP_MAX) {
      openHatch();
      digitalWrite(PELTIER1_PIN, HIGH);
      digitalWrite(PELTIER2_PIN, HIGH);
      if (hatchOpen) {
        digitalWrite(FAN1_PIN, HIGH);
        digitalWrite(FAN2_PIN, HIGH);
      }
    }
  }
}

// -- Lighting --
void checkLighting() {
  if (millis() - lastLightCheck >= LIGHT_CHECK_INTERVAL) {
    lastLightCheck = millis();
    int ldrValue = analogRead(LDR_PIN);
    uint8_t brightness = map(ldrValue, 0, 1023, 255, 0);
    analogWrite(LED_STRIP1_PWM, brightness);
    analogWrite(LED_STRIP2_PWM, brightness);
  }
}

// -- Water Level --
void sendLowLevelAlert(int tankID) {
  Serial.print("ALERT: Low level in tank ");
  Serial.println(tankID);
}
void checkWaterLevels() {
  if (millis() - lastLevelCheck >= LEVEL_CHECK_INTERVAL) {
    lastLevelCheck = millis();
    if (sonar1.ping_cm() < LOW_LEVEL_CM) sendLowLevelAlert(1);
    if (sonar2.ping_cm() < LOW_LEVEL_CM) sendLowLevelAlert(2);
    if (sonar3.ping_cm() < LOW_LEVEL_CM) sendLowLevelAlert(3);
    if (sonar4.ping_cm() < LOW_LEVEL_CM) sendLowLevelAlert(4);
  }
}

// -- Ebb & Flow --
void checkEbbFlow() {
  unsigned long nowMillis = millis();
  if (ebbFlowPhase == 0 && nowMillis - ebbFlowLastStart >= EBB_FLOW_INTERVAL) {
    ebbFlowPhase = 1;
    ebbFlowPhaseStart = nowMillis;
    digitalWrite(WATER_PUMP_TOPTRAY_PIN, HIGH);
    digitalWrite(WATER_PUMP_TOP2BOTTOM_PIN, LOW);
    digitalWrite(WATER_PUMP_BOTTOM2RES_PIN, LOW);
  }
  else if (ebbFlowPhase == 1 && nowMillis - ebbFlowPhaseStart >= EBB_FLOW_PUMP_TIME) {
    ebbFlowPhase = 2;
    ebbFlowPhaseStart = nowMillis;
    digitalWrite(WATER_PUMP_TOPTRAY_PIN, LOW);
    digitalWrite(WATER_PUMP_TOP2BOTTOM_PIN, HIGH);
    digitalWrite(WATER_PUMP_BOTTOM2RES_PIN, LOW);
  }
  else if (ebbFlowPhase == 2 && nowMillis - ebbFlowPhaseStart >= EBB_FLOW_PUMP_TIME) {
    ebbFlowPhase = 3;
    ebbFlowPhaseStart = nowMillis;
    digitalWrite(WATER_PUMP_TOPTRAY_PIN, LOW);
    digitalWrite(WATER_PUMP_TOP2BOTTOM_PIN, LOW);
    digitalWrite(WATER_PUMP_BOTTOM2RES_PIN, HIGH);
  }
  else if (ebbFlowPhase == 3 && nowMillis - ebbFlowPhaseStart >= EBB_FLOW_PUMP_TIME) {
    ebbFlowPhase = 0;
    ebbFlowLastStart = nowMillis;
    digitalWrite(WATER_PUMP_TOPTRAY_PIN, LOW);
    digitalWrite(WATER_PUMP_TOP2BOTTOM_PIN, LOW);
    digitalWrite(WATER_PUMP_BOTTOM2RES_PIN, LOW);
  }
}

// -- Data Logging with Timestamp --
void logAndTransmitData() {
  if (millis() - lastLogTime >= LOG_INTERVAL) {
    lastLogTime = millis();
    now = rtc.now();
    Serial.print("[");
    Serial.print(now.timestamp(DateTime::TIMESTAMP_FULL));
    Serial.print("] ");
    Serial.print("pH:"); Serial.print(phValue);
    Serial.print(",EC:"); Serial.print(readEC());
    Serial.print(",Humidity:"); Serial.print(lastHumidity);
    Serial.print(",Temp:"); Serial.print(lastTemp);
    Serial.print(",LDR:"); Serial.print(analogRead(LDR_PIN));
    Serial.println();
  }
}

// -- Example RTC Scheduled Tasks --
void checkScheduledTasks() {
  now = rtc.now();
  if (now.hour() == 8 && now.minute() == 0) {
    analogWrite(LED_STRIP1_PWM, 255);
    analogWrite(LED_STRIP2_PWM, 255);
  }
  if (now.hour() == 20 && now.minute() == 0) {
    analogWrite(LED_STRIP1_PWM, 0);
    analogWrite(LED_STRIP2_PWM, 0);
  }
}

// ==== ARDUINO SETUP AND LOOP ====
void setup() {
  Serial.begin(115200);
  setupPH();
  setupNutrient();
  setupEbbFlow();
  setupClimate();
  setupLighting();
  setupUVC();
  setupRTC();
}

void loop() {
  checkPH();
  checkEC();
  checkClimate();
  checkLighting();
  checkWaterLevels();
  checkEbbFlow();
  logAndTransmitData();
  checkScheduledTasks();
}