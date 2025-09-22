#include <LCD_I2C.h>
#include <Wire.h>
#include <VL53L0X.h>

LCD_I2C lcd(0x27, 16, 2);
VL53L0X sensor;

void setup() {
  Wire.begin();
  lcd.begin(false);  // If you are using more I2C devices using the Wire library use lcd.begin(false)
                     // this stop the library(LCD_I2C) from calling Wire.begin()

  sensor.setTimeout(500);
  if (!sensor.init()) {
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("VL53L0X  ");
    lcd.setCursor(0, 1);
    lcd.print("Not working");
    delay(3000);
    while (1) {}
  }
  sensor.startContinuous(10);

  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("VL53L0X  ");
  lcd.setCursor(0, 1);
  lcd.print("Range Finder");
  delay(3000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Leszek");
  lcd.setCursor(0, 1);
  lcd.print("Matuszczyk");
  delay(3000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Like and");
  lcd.setCursor(0, 1);
  lcd.print("Subscribe");
  delay(3000);
  lcd.clear();
}

void loop() {
  int distance = sensor.readRangeContinuousMillimeters();
  lcd.setCursor(0, 0);
  lcd.print("Distance");
  lcd.setCursor(0, 1);
  if (distance < 510) {
    lcd.print(distance);
    lcd.print(" mm");
  } else {
    lcd.print("OUT of Range");
  }
  delay(100);
  lcd.clear();
}
