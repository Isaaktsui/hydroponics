const int sensorPin = A0;      // Where probe junction connects
const float R_known = 10000.0; // 10k resistor (ohms)

void setup() {
  Serial.begin(9600);
  Serial.println("DIY EC probe ready");
}

void loop() {
  int raw = analogRead(sensorPin);
  float Vout = raw * (5.0 / 1023.0); // ADC to voltage

  // Voltage divider formula: Vout = 5V * (R_probe / (R_known + R_probe))
  // Rearranged to solve for R_probe:
  float R_probe = (Vout * R_known) / (5.0 - Vout);

  Serial.print("ADC: ");
  Serial.print(raw);
  Serial.print(" | Vout: ");
  Serial.print(Vout, 3);
  Serial.print(" V | R_probe: ");
  Serial.print(R_probe, 1);
  Serial.println(" ohms");

  // EC (µS/cm) is roughly proportional to 1/R_probe, but needs calibration!

  delay(1000);
}