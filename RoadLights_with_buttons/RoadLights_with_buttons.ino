#define RED 5       // Assign pin 5 to the red light
#define YELLOW 6    // Assign pin 6 to the yellow light
#define GREEN 7     // Assign pin 7 to the green light
#define BUTTON1 8   // Assign pin 8 to button 1
#define BUTTON2 9   // Assign pin 9 to button 2

int Time = 0;       // Variable to store delay time for light changes

void setup() {
  pinMode(RED, OUTPUT);       // Configure red light pin as output
  pinMode(YELLOW, OUTPUT);    // Configure yellow light pin as output
  pinMode(GREEN, OUTPUT);     // Configure green light pin as output
  pinMode(BUTTON1, INPUT_PULLUP); // Configure button 1 with internal pull-up resistor
  pinMode(BUTTON2, INPUT_PULLUP); // Configure button 2 with internal pull-up resistor
}

void loop() {
  // Set delay time based on button states:
  // BUTTON1 not pressed & BUTTON2 pressed → Time = 200 ms
  if (digitalRead(BUTTON1) == 1 && digitalRead(BUTTON2) == 0) {
    Time = 200;
  } 
  // BUTTON1 pressed & BUTTON2 not pressed → Time = 100 ms
  else if (digitalRead(BUTTON1) == 0 && digitalRead(BUTTON2) == 1) {
    Time = 100;
  } 
  // Both BUTTON1 and BUTTON2 pressed → Time = 50 ms
  else if (digitalRead(BUTTON1) == 0 && digitalRead(BUTTON2) == 0) {
    Time = 50;
  }

  // Step 1: Red ON, Yellow OFF, Green OFF (Stop)
  digitalWrite(RED, 1);
  digitalWrite(YELLOW, 0);
  digitalWrite(GREEN, 0);
  delay(Time);  // Wait based on button selection
  
  // Step 2: Red ON, Yellow ON, Green OFF (Get ready)
  digitalWrite(RED, 1);
  digitalWrite(YELLOW, 1);
  digitalWrite(GREEN, 0);
  delay(Time);  // Wait based on button selection
  
  // Step 3: Red OFF, Yellow OFF, Green ON (Go)
  digitalWrite(RED, 0);
  digitalWrite(YELLOW, 0);
  digitalWrite(GREEN, 1);
  delay(Time);  // Wait based on button selection
  
  // Step 4: Red OFF, Yellow ON, Green OFF (Prepare to stop)
  digitalWrite(RED, 0);
  digitalWrite(YELLOW, 1);
  digitalWrite(GREEN, 0);
  delay(Time);  // Wait based on button selection
}
