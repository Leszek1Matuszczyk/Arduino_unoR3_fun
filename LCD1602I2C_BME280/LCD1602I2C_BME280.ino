#include <LCD_I2C.h>        // Library to control I2C LCD displays
#include "Seeed_BME280.h"   // Library for the BME280 sensor (temperature, humidity, pressure)
#include <Wire.h>           // Library for I2C communication

// Create an LCD object with I2C address 0x27, 16 columns, and 2 rows
LCD_I2C lcd(0x27, 16, 2);

// Create a BME280 sensor object
BME280 bme280;

void setup() {
  lcd.begin();       // Initialize the LCD
  lcd.backlight();   // Turn on LCD backlight

  // Try to initialize the BME280 sensor
  // If it fails, display "Sensor NOK" repeatedly
  while (!bme280.init()) {
    lcd.setCursor(0, 0);   // Set cursor to first line, first column
    lcd.print("Sensor NOK"); 
    delay(10);
  }

  // Display a welcome message
  lcd.setCursor(0, 0);
  lcd.print("Leszek");
  lcd.setCursor(0, 1);
  lcd.print("Matuszczyk");
  delay(4000);       // Show message for 4 seconds
  lcd.clear();       // Clear screen

  lcd.setCursor(0, 0);
  lcd.print("Fun And Simple");
  lcd.setCursor(0, 1);
  lcd.print("Project");
  delay(4000);       // Show message for 4 seconds
  lcd.clear();       // Clear screen
}

void loop() {
  float pressure;  // Variable to store measured pressure

  // --- First screen: Temperature and Humidity ---
  lcd.setCursor(0, 0);
  lcd.print("T: ");
  lcd.print(bme280.getTemperature());  // Read temperature
  lcd.print(" C      ");               // Add units and spaces to clear old data

  lcd.setCursor(0, 1);
  lcd.print("H: ");
  lcd.print(bme280.getHumidity());     // Read humidity
  lcd.print(" %      ");
  delay(3000);                         // Show for 3 seconds

  // --- Second screen: Pressure and Altitude ---
  lcd.setCursor(0, 0);
  lcd.print("P: ");
  lcd.print(pressure = bme280.getPressure()); // Read pressure and save it
  lcd.print(" Pa     ");                       // Display in Pascals

  lcd.setCursor(0, 1);
  lcd.print("A: ");
  lcd.print(bme280.calcAltitude(pressure));   // Calculate altitude from pressure
  lcd.print(" m     ");                       // Display in meters
  delay(3000);                                // Show for 3 seconds
}
