#include <LiquidCrystal.h>

const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

void setup() {
  Serial.begin(9600);
  // set up the LCD's number of columns and rows:
  lcd.begin(20, 4);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(6, INPUT);
  pinMode(A2, INPUT);
  pinMode(13, OUTPUT);
  Serial.println("test");
  lcd.clear();
}

void loop() {
  int highPeriod = 60;
  int lowPeriod = 40;
  int entirePulse = highPeriod + lowPeriod;

  digitalWrite(13, 1);
  delay(highPeriod);

  digitalWrite(13, 0);
  delay(lowPeriod);

  int pin_state = digitalRead(6);
  Serial.print("pin State ");
  Serial.println(pin_state);
  
  float analog_state = analogRead(A2);
  Serial.println(analog_state);

  if (analog_state < 200.0){
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("warning, low value!!");
    delay(1000);
  } else if (analog_state > 1000.0) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("warning, highvalue!!"); 
    delay(1000);
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  // print the number of seconds since reset:

  lcd.print("Dig:");
  lcd.print(pin_state);
  lcd.print(" Ana:");
  lcd.print(analog_state);

  calculate(entirePulse, highPeriod);

  delay(500);
}

void calculate(int duration, int highPeriod) {
  float duty_cycle = (float)highPeriod / duration *100;
  Serial.print("duty cycle is ");
  Serial.println(duty_cycle);
  int blinking_frequency = 1000 / duration;
  Serial.print("blinking frequency is ");
  Serial.println(blinking_frequency);

  lcd.setCursor(0, 1);
  lcd.print("dut: ");
  lcd.print(duty_cycle);
  lcd.print("%");
  lcd.print(" fr: ");
  lcd.print(blinking_frequency);
  lcd.print("Hz");
}

