int pwmPin = 9; // Connect to MOSFET Gate

void setup() {
  pinMode(pwmPin, OUTPUT);
}

void loop() {
  analogWrite(pwmPin, 128); // 50% duty cycle, LED should be half-bright
  delay(2000);
  analogWrite(pwmPin, 255); // 100% duty cycle, LED fully on
  delay(2000);
  analogWrite(pwmPin, 0);   // 0% duty cycle, LED off
  delay(2000);
}