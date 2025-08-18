#include <Servo.h>

// Pin assignments
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
#define MISTER_PIN 6 // Free pin for mister module

// --- L293D Shield Standard Pin Mapping for Motor A/B ---
// Motor A (Peltier 1)
#define PELTIER1_EN 3    // ENA (PWM)
#define PELTIER1_IN1 12  // IN1
#define PELTIER1_IN2 13  // IN2
// Motor B (Peltier 2)
#define PELTIER2_EN 11   // ENB (PWM)
#define PELTIER2_IN3 8   // IN3
#define PELTIER2_IN4 9   // IN4

#define FAN1_PIN 9  // DFR0332 Fan Module Signal pin (alias for compatibility)
#define FAN2_PIN 10

Servo servo1, servo2;
String inputBuffer = "";

// Helper: set L293D channel (EN, IN1, IN2)
void setL293D(int en, int inA, int inB, char dir, int pwm) {
  pwm = constrain(pwm, 0, 255);
  analogWrite(en, pwm);
  if (dir == 'f') {         // Forward
    digitalWrite(inA, HIGH);
    digitalWrite(inB, LOW);
  } else if (dir == 'r') {  // Reverse
    digitalWrite(inA, LOW);
    digitalWrite(inB, HIGH);
  } else {                  // Off/brake
    digitalWrite(inA, LOW);
    digitalWrite(inB, LOW);
    analogWrite(en, 0);
  }
}

void setup() {
  Serial.begin(115200);

  // LEDs via MOSFET
  pinMode(LEDS_MOSFET_GATE_PIN, OUTPUT);
  analogWrite(LEDS_MOSFET_GATE_PIN, 0);

  // UV LEDs
  pinMode(UV1_PIN, OUTPUT);
  pinMode(UV2_PIN, OUTPUT);
  digitalWrite(UV1_PIN, LOW);
  digitalWrite(UV2_PIN, LOW);

  // Fans
  pinMode(FAN1_PWM_PIN, OUTPUT);
  pinMode(FAN2_PWM_PIN, OUTPUT);
  analogWrite(FAN1_PWM_PIN, 0);
  analogWrite(FAN2_PWM_PIN, 0);

  // Also support simple ON/OFF for DFR0332 Fan Module
  pinMode(FAN1_PIN, OUTPUT);
  pinMode(FAN2_PIN, OUTPUT);
  digitalWrite(FAN1_PIN, LOW);
  digitalWrite(FAN2_PIN, LOW);

  // Servos
  servo1.attach(SERVO1_PIN);
  servo2.attach(SERVO2_PIN);
  servo1.write(90);
  servo2.write(90);

  // Pumps
  pinMode(PUMP1_PIN, OUTPUT);
  pinMode(PUMP2_PIN, OUTPUT);
  pinMode(PUMP3_PIN, OUTPUT);
  pinMode(PUMP4_PIN, OUTPUT);
  pinMode(PUMP5_PIN, OUTPUT);
  pinMode(PUMP6_PIN, OUTPUT);
  digitalWrite(PUMP1_PIN, LOW);
  digitalWrite(PUMP2_PIN, LOW);
  digitalWrite(PUMP3_PIN, LOW);
  digitalWrite(PUMP4_PIN, LOW);
  digitalWrite(PUMP5_PIN, LOW);
  digitalWrite(PUMP6_PIN, LOW);

  // Mister
  pinMode(MISTER_PIN, OUTPUT);
  digitalWrite(MISTER_PIN, LOW);

  // Peltiers (L293D Shield)
  pinMode(PELTIER1_EN, OUTPUT);
  pinMode(PELTIER1_IN1, OUTPUT);
  pinMode(PELTIER1_IN2, OUTPUT);
  setL293D(PELTIER1_EN, PELTIER1_IN1, PELTIER1_IN2, 'o', 0);

  pinMode(PELTIER2_EN, OUTPUT);
  pinMode(PELTIER2_IN3, OUTPUT);
  pinMode(PELTIER2_IN4, OUTPUT);
  setL293D(PELTIER2_EN, PELTIER2_IN3, PELTIER2_IN4, 'o', 0);

  Serial.println("Full actuator test console ready (peltiers on D3/D12/D13, D11/D8/D9).");
  Serial.println("Commands:");
  Serial.println(" leds [0-255]      (both LEDs via MOSFET)");
  Serial.println(" uv1 on/off, uv2 on/off");
  Serial.println(" fan1 [0-255], fan2 [0-255], fan1 on/off, fan2 on/off");
  Serial.println(" servo1 [0-180], servo2 [0-180]");
  Serial.println(" pumpN on/off (N=1-6)");
  Serial.println(" mister on/off");
  Serial.println(" peltierN [f/r/off] [0-255] (N=1/2)");
  Serial.println(" status, help");
}

void executeCommand(String cmd) {
  cmd.trim();
  cmd.toLowerCase();

  if (cmd.startsWith("leds ")) {
    int val = cmd.substring(5).toInt();
    val = constrain(val, 0, 255);
    analogWrite(LEDS_MOSFET_GATE_PIN, val);
    Serial.print("Both LEDs PWM set to "); Serial.println(val);
  }
  else if (cmd == "uv1 on") {
    digitalWrite(UV1_PIN, HIGH); Serial.println("UV1 ON");
  } else if (cmd == "uv1 off") {
    digitalWrite(UV1_PIN, LOW); Serial.println("UV1 OFF");
  } else if (cmd == "uv2 on") {
    digitalWrite(UV2_PIN, HIGH); Serial.println("UV2 ON");
  } else if (cmd == "uv2 off") {
    digitalWrite(UV2_PIN, LOW); Serial.println("UV2 OFF");
  }

  // --- DFR0332 Fan Module simple ON/OFF commands ---
  else if (cmd == "fan1 on") {
    digitalWrite(FAN1_PIN, HIGH);
    Serial.println("Fan1 ON");
  } else if (cmd == "fan1 off") {
    digitalWrite(FAN1_PIN, LOW);
    Serial.println("Fan1 OFF");
  } else if (cmd == "fan2 on") {
    digitalWrite(FAN2_PIN, HIGH);
    Serial.println("Fan2 ON");
  } else if (cmd == "fan2 off") {
    digitalWrite(FAN2_PIN, LOW);
    Serial.println("Fan2 OFF");
  }
  else if (cmd == "mister on") {
    digitalWrite(MISTER_PIN, HIGH);
    Serial.println("Mister ON");
  } else if (cmd == "mister off") {
    digitalWrite(MISTER_PIN, LOW);
    Serial.println("Mister OFF");
  }

  // PWM fan control
  else if (cmd.startsWith("fan1 ")) {
    int val = cmd.substring(5).toInt();
    val = constrain(val, 0, 255);
    analogWrite(FAN1_PWM_PIN, val);
    Serial.print("Fan1 PWM set to "); Serial.println(val);
  } else if (cmd.startsWith("fan2 ")) {
    int val = cmd.substring(5).toInt();
    val = constrain(val, 0, 255);
    analogWrite(FAN2_PWM_PIN, val);
    Serial.print("Fan2 PWM set to "); Serial.println(val);
  }

  else if (cmd.startsWith("servo1 ")) {
    int val = cmd.substring(7).toInt();
    val = constrain(val, 0, 180);
    servo1.write(val);
    Serial.print("Servo1 angle "); Serial.println(val);
  } else if (cmd.startsWith("servo2 ")) {
    int val = cmd.substring(7).toInt();
    val = constrain(val, 0, 180);
    servo2.write(val);
    Serial.print("Servo2 angle "); Serial.println(val);
  }
  else if (cmd == "pump1 on") { digitalWrite(PUMP1_PIN, HIGH); Serial.println("Pump1 ON"); }
  else if (cmd == "pump1 off") { digitalWrite(PUMP1_PIN, LOW); Serial.println("Pump1 OFF"); }
  else if (cmd == "pump2 on") { digitalWrite(PUMP2_PIN, HIGH); Serial.println("Pump2 ON"); }
  else if (cmd == "pump2 off") { digitalWrite(PUMP2_PIN, LOW); Serial.println("Pump2 OFF"); }
  else if (cmd == "pump3 on") { digitalWrite(PUMP3_PIN, HIGH); Serial.println("Pump3 ON"); }
  else if (cmd == "pump3 off") { digitalWrite(PUMP3_PIN, LOW); Serial.println("Pump3 OFF"); }
  else if (cmd == "pump4 on") { digitalWrite(PUMP4_PIN, HIGH); Serial.println("Pump4 ON"); }
  else if (cmd == "pump4 off") { digitalWrite(PUMP4_PIN, LOW); Serial.println("Pump4 OFF"); }
  else if (cmd == "pump5 on") { digitalWrite(PUMP5_PIN, HIGH); Serial.println("Pump5 ON"); }
  else if (cmd == "pump5 off") { digitalWrite(PUMP5_PIN, LOW); Serial.println("Pump5 OFF"); }
  else if (cmd == "pump6 on") { digitalWrite(PUMP6_PIN, HIGH); Serial.println("Pump6 ON"); }
  else if (cmd == "pump6 off") { digitalWrite(PUMP6_PIN, LOW); Serial.println("Pump6 OFF"); }

  // Peltiers (via L293D shield pins)
  else if (cmd.startsWith("peltier1 ")) {
    String arg = cmd.substring(9);
    if (arg.startsWith("f ")) {
      int pwmVal = arg.substring(2).toInt();
      setL293D(PELTIER1_EN, PELTIER1_IN1, PELTIER1_IN2, 'f', pwmVal);
      Serial.print("Peltier1 FORWARD, PWM "); Serial.println(pwmVal);
    } else if (arg.startsWith("r ")) {
      int pwmVal = arg.substring(2).toInt();
      setL293D(PELTIER1_EN, PELTIER1_IN1, PELTIER1_IN2, 'r', pwmVal);
      Serial.print("Peltier1 REVERSE, PWM "); Serial.println(pwmVal);
    } else if (arg.startsWith("off")) {
      setL293D(PELTIER1_EN, PELTIER1_IN1, PELTIER1_IN2, 'o', 0);
      Serial.println("Peltier1 OFF");
    }
  } else if (cmd.startsWith("peltier2 ")) {
    String arg = cmd.substring(9);
    if (arg.startsWith("f ")) {
      int pwmVal = arg.substring(2).toInt();
      setL293D(PELTIER2_EN, PELTIER2_IN3, PELTIER2_IN4, 'f', pwmVal);
      Serial.print("Peltier2 FORWARD, PWM "); Serial.println(pwmVal);
    } else if (arg.startsWith("r ")) {
      int pwmVal = arg.substring(2).toInt();
      setL293D(PELTIER2_EN, PELTIER2_IN3, PELTIER2_IN4, 'r', pwmVal);
      Serial.print("Peltier2 REVERSE, PWM "); Serial.println(pwmVal);
    } else if (arg.startsWith("off")) {
      setL293D(PELTIER2_EN, PELTIER2_IN3, PELTIER2_IN4, 'o', 0);
      Serial.println("Peltier2 OFF");
    }
  }
  else if (cmd == "status") {
    Serial.println("Status:");
    Serial.print("LEDs (MOSFET): "); Serial.println(analogRead(LEDS_MOSFET_GATE_PIN));
    Serial.print("UV1: "); Serial.println(digitalRead(UV1_PIN) ? "ON" : "OFF");
    Serial.print("UV2: "); Serial.println(digitalRead(UV2_PIN) ? "ON" : "OFF");
    for(int i=1;i<=6;i++){
      Serial.print("Pump"); Serial.print(i); Serial.print(": ");
      int pval = digitalRead(PUMP1_PIN + (i-1));
      Serial.println(pval ? "ON" : "OFF");
    }
    Serial.print("Fan1: "); Serial.println(analogRead(FAN1_PWM_PIN));
    Serial.print("Fan2: "); Serial.println(analogRead(FAN2_PWM_PIN));
    Serial.print("Servo1 angle: "); Serial.println(servo1.read());
    Serial.print("Servo2 angle: "); Serial.println(servo2.read());
    Serial.println("(Peltier status: check code for current control)");
  }
  else if (cmd == "help") {
    Serial.println("Commands:");
    Serial.println(" leds [0-255]      (both LEDs via MOSFET)");
    Serial.println(" uv1 on/off, uv2 on/off");
    Serial.println(" fan1 [0-255], fan2 [0-255], fan1 on/off, fan2 on/off");
    Serial.println(" servo1 [0-180], servo2 [0-180]");
    Serial.println(" pumpN on/off (N=1-6)");
    Serial.println(" mister on/off");
    Serial.println(" peltierN [f/r/off] [0-255] (N=1/2)");
    Serial.println(" status, help");
  }
  else {
    Serial.println("Unknown command. Type 'help' for commands.");
  }
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (inputBuffer.length() > 0) {
        executeCommand(inputBuffer);
        inputBuffer = "";
      }
    } else if (isPrintable(c)) {
      inputBuffer += c;
    }
  }
}