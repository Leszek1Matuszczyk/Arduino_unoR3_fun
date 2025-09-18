#include <LCD_I2C.h>

LCD_I2C lcd(0x27, 16, 2);

void setup() {
  lcd.begin();
}

void loop() {
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("I2C LCD Example");
  delay(6000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Leszek");
  lcd.setCursor(0, 1);
  lcd.print("Matuszczyk ");
  delay(6000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Like and ");
  lcd.setCursor(0, 1);
  lcd.print("Subscribe");
  delay(6000);
  lcd.clear();
}
