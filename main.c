#include <ChainableLED.h>
#include <Wire.h>
#include "rgb_lcd.h"

rgb_lcd lcd;

// LED chainable
ChainableLED myLED(2, 3, 1); // Clock=D2, Data=D3, 1 LED

// Dual Button
const int button1Pin = 4; // SIG1
const int button2Pin = 5; // SIG2

void setup() {
  myLED.init();
  myLED.setColorRGB(0, 0, 0, 0); // LED éteinte

  pinMode(button1Pin, INPUT_PULLUP);
  pinMode(button2Pin, INPUT_PULLUP);

  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
}

void loop() {
  // Bouton 1 → rouge
  if (digitalRead(button1Pin) == LOW) {
    myLED.setColorRGB(0, 255, 0, 0);
    lcd.setCursor(0, 1);
    lcd.print("Rouge   "); // espaces pour effacer l'ancien texte
  }
  // Bouton 2 → bleu
  else if (digitalRead(button2Pin) == LOW) {
    myLED.setColorRGB(0, 0, 0, 255);
    lcd.setCursor(0, 1);
    lcd.print("Bleu    ");
  }
  // Aucun bouton → LED éteinte
  else {
    myLED.setColorRGB(0, 0, 0, 0);
    lcd.setCursor(0, 1);
    lcd.print("Aucune  ");
  }
}
