#include "HX711.h"
#include <LCD_I2C.h>

#define DATAPIN 6
#define CLOCKPIN 7

#define COL 16
#define ROW 2
#define ADDR 0x27

HX711 scale;
LCD_I2C lcd(ADDR, COL,ROW);

float weight;
void setup() {
  scale.begin(DATAPIN, CLOCKPIN);
  scale.set_scale(-418.415374); 
  scale.tare();
  lcd.begin();
  lcd.backlight();
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
  lcd.clear();      
}

void loop() {
  weight = scale.get_units(10);
lcd.setCursor(0, 0);
  lcd.print(weight);
  lcd.print(" g             ");
  lcd.setCursor(0, 1);
  lcd.print("Scale");
  delay(10); 
}
