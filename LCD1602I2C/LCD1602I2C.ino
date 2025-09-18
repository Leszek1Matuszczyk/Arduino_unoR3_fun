#include <LCD_I2C.h>   // Include library for controlling LCD via I2C interface

LCD_I2C lcd(0x27, 16, 2); // Initialize LCD object at I2C address 0x27, display size 16x2 characters

void setup() {
  lcd.begin();  // Start the LCD
}

void loop() {
  lcd.backlight();           // Turn on the LCD backlight

  // Screen 1: Example text
  lcd.setCursor(0, 0);       // Set cursor to column 0, row 0
  lcd.print("I2C LCD Example");
  delay(6000);               // Wait 6 seconds
  lcd.clear();               // Clear the screen

  // Screen 2: Name
  lcd.setCursor(0, 0);       // Set cursor to the first line
  lcd.print("Leszek");
  lcd.setCursor(0, 1);       // Set cursor to the second line
  lcd.print("Matuszczyk");
  delay(6000);               // Wait 6 seconds
  lcd.clear();               // Clear the screen

  // Screen 3: Call to action
  lcd.setCursor(0, 0);       
  lcd.print("Like and");
  lcd.setCursor(0, 1);
  lcd.print("Subscribe");
  delay(6000);               // Wait 6 seconds
  lcd.clear();               // Clear the screen
}
