// L298N Dual Channel Serial Control Test
// IN1, IN2: Channel A (Peltier #1)
// IN3, IN4: Channel B (Peltier #2)
// ENA and ENB are jumpered

#define IN1 8
#define IN2 9
#define IN3 11
#define IN4 12

void setup() {
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  Serial.begin(9600);
  Serial.println("L298N Peltier Serial Control");
  Serial.println("Commands:");
  Serial.println("  a+ : Peltier #1 forward  | a- : Peltier #1 reverse | a0 : Peltier #1 off");
  Serial.println("  b+ : Peltier #2 forward  | b- : Peltier #2 reverse | b0 : Peltier #2 off");
  Serial.println("  all+ : both forward | all- : both reverse | all0 : both off");
}

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd == "a+") {
      digitalWrite(IN1, HIGH);
      digitalWrite(IN2, LOW);
      Serial.println("Peltier #1: Forward");
    } else if (cmd == "a-") {
      digitalWrite(IN1, LOW);
      digitalWrite(IN2, HIGH);
      Serial.println("Peltier #1: Reverse");
    } else if (cmd == "a0") {
      digitalWrite(IN1, LOW);
      digitalWrite(IN2, LOW);
      Serial.println("Peltier #1: Off");
    } else if (cmd == "b+") {
      digitalWrite(IN3, HIGH);
      digitalWrite(IN4, LOW);
      Serial.println("Peltier #2: Forward");
    } else if (cmd == "b-") {
      digitalWrite(IN3, LOW);
      digitalWrite(IN4, HIGH);
      Serial.println("Peltier #2: Reverse");
    } else if (cmd == "b0") {
      digitalWrite(IN3, LOW);
      digitalWrite(IN4, LOW);
      Serial.println("Peltier #2: Off");
    } else if (cmd == "all+") {
      digitalWrite(IN1, HIGH);
      digitalWrite(IN2, LOW);
      digitalWrite(IN3, HIGH);
      digitalWrite(IN4, LOW);
      Serial.println("Both Peltiers: Forward");
    } else if (cmd == "all-") {
      digitalWrite(IN1, LOW);
      digitalWrite(IN2, HIGH);
      digitalWrite(IN3, LOW);
      digitalWrite(IN4, HIGH);
      Serial.println("Both Peltiers: Reverse");
    } else if (cmd == "all0") {
      digitalWrite(IN1, LOW);
      digitalWrite(IN2, LOW);
      digitalWrite(IN3, LOW);
      digitalWrite(IN4, LOW);
      Serial.println("Both Peltiers: Off");
    } else {
      Serial.println("Unknown command.");
    }
  }
}