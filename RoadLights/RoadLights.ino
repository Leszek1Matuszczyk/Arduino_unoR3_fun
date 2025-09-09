#define RED 1      // Define pin number for the red light
#define YELLOW 2   // Define pin number for the yellow light
#define GREEN 3    // Define pin number for the green light

void setup() {
  pinMode(RED, OUTPUT);     // Set the red light pin as an output
  pinMode(YELLOW, OUTPUT);  // Set the yellow light pin as an output
  pinMode(GREEN, OUTPUT);   // Set the green light pin as an output
}

void loop() {
  // Step 1: Red ON, Yellow OFF, Green OFF (Stop)
  digitalWrite(RED, 1);
  digitalWrite(YELLOW, 0);
  digitalWrite(GREEN, 0);
  delay(2000);  // Wait 2 seconds
  
  // Step 2: Red ON, Yellow ON, Green OFF (Prepare to go)
  digitalWrite(RED, 1);
  digitalWrite(YELLOW, 1);
  digitalWrite(GREEN, 0);
  delay(2000);  // Wait 2 seconds
  
  // Step 3: Red OFF, Yellow OFF, Green ON (Go)
  digitalWrite(RED, 0);
  digitalWrite(YELLOW, 0);
  digitalWrite(GREEN, 1);
  delay(2000);  // Wait 2 seconds
  
  // Step 4: Red OFF, Yellow ON, Green OFF (Prepare to stop)
  digitalWrite(RED, 0);
  digitalWrite(YELLOW, 1);
  digitalWrite(GREEN, 0);
  delay(2000);  // Wait 2 seconds
}
