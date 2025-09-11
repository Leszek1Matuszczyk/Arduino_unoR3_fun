#define PWM_LED 3   // Pin 3 used for PWM output (controls LED brightness)
#define ANALOG 0    // Analog input pin A0 for reading sensor/potentiometer value

void setup() {
  pinMode(PWM_LED, OUTPUT);  // Set pin 3 as an output for the LED
}

void loop() {
  // Read the analog input (0–1023), map it to PWM range (0–255),
  // and write it to the LED to control brightness
  analogWrite(PWM_LED, map(analogRead(ANALOG), 0, 1024, 0, 255));

  delay(50);  // Short delay to reduce flickering and stabilize updates
}
