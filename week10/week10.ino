#include <LiquidCrystal.h>

const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);


void setup() {
  Serial.begin(9600);
  // set up the LCD's number of columns and rows:
  lcd.begin(20, 4);
  // pinMode(LED_BUILTIN, OUTPUT);
  // pinMode(6, INPUT);
  // pinMode(A2, INPUT);
  // pinMode(13, OUTPUT);
}

void loop() {

}

void printAlphabet() {
  
}