#include <Arduino.h>  // Core Arduino library for basic functions
#include <U8x8lib.h>  // Library for controlling OLED displays with U8x8 interface

// Create a display object for SH1107-based 128x128 OLED using software I2C
U8X8_SH1107_SEEED_128X128_SW_I2C u8x8(
  /* clock=*/SCL,           // I2C clock pin
  /* data=*/SDA,            // I2C data pin
  /* reset=*/U8X8_PIN_NONE  // No reset pin used
);

void setup() {
  u8x8.begin();          // Initialize the OLED display
  u8x8.setPowerSave(0);  // Turn off power-saving mode (turn display ON)
}

void loop() {
  // -------- First Screen --------
  u8x8.clearDisplay();                          // Clear the display before writing
  u8x8.setFont(u8x8_font_px437wyse700a_2x2_r);  // Set a large readable font
  u8x8.drawString(0, 0, "Amazing");             // Write text at column=0, row=0
  u8x8.drawString(0, 2, "Simple");
  u8x8.drawString(0, 4, "Fun");
  u8x8.drawString(0, 6, "Project");
  
  // Change to a different smaller font to fit more text
  u8x8.setFont(u8x8_font_amstrad_cpc_extended_f);
  u8x8.drawString(0, 8, "Leszek");
  u8x8.drawString(0, 9, "Matuszczyk");
  u8x8.drawString(0, 10, "Like");
  u8x8.drawString(0, 11, "And");
  u8x8.drawString(0, 12, "Subscribe");
  u8x8.drawString(0, 13, "Github");
  u8x8.drawString(0, 14, "link in");
  u8x8.drawString(0, 15, "Description");
  delay(6000);  // Wait 6 seconds before changing the screen

  // -------- Second Screen --------
  u8x8.clearDisplay();                          // Clear the screen
  u8x8.setFont(u8x8_font_px437wyse700a_2x2_r);  // Use the same large font
  u8x8.drawString(0, 0, "Amazing");
  u8x8.drawString(0, 2, "Oled");
  u8x8.drawString(0, 4, "Display");

  // Change to a different smaller font to fit more text
  u8x8.setFont(u8x8_font_amstrad_cpc_extended_f);
  u8x8.drawString(0, 6, "Leszek");
  u8x8.drawString(0, 7, "Matuszczyk");
  u8x8.drawString(0, 8, "Like");
  u8x8.drawString(0, 9, "And");
  u8x8.drawString(0, 10, "Subscribe");
  u8x8.drawString(0, 11, "Github");
  u8x8.drawString(0, 12, "link in");
  u8x8.drawString(0, 13, "Description");
  delay(6000);  // Keep this screen for 6 seconds

  // -------- Clear Display --------
  u8x8.clearDisplay();  // Clear display before loop restarts
}
