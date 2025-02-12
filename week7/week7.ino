#include <LiquidCrystal.h>

const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

const int LED_pin = 7;

#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit)) // macro to clear bit in special function register
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit)) // macro to set bit in special function register


void setup() {
  Serial.begin(9600);
  // set up the LCD's number of columns and rows:
  lcd.begin(20, 4);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(6, INPUT);
  pinMode(A2, INPUT);
  pinMode(13, OUTPUT);

  // LED
  pinMode(LED_pin, OUTPUT);

  Serial.println("test");
  lcd.clear();
  sbi(ADCSRA, ADPS2);
  cbi(ADCSRA, ADPS1);
  cbi(ADCSRA, ADPS0);
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
  
  int initTime2 = micros();
  float analog_state = analogRead(A2);
  int endTime2 = micros();
  int elapsedTime2 = endTime2 - initTime2;
  Serial.println(analog_state);
  Serial.print("time:");
  Serial.println(elapsedTime2);

  // convert to volts
  float volts_potentiometer = map(analog_state, 0, 1023, 0, 5);
  Serial.println(volts_potentiometer);

  // convert to new val
  float new_val = map(volts_potentiometer, 0, 5, 50, 300);
  Serial.println(new_val);

  // LED
  if (volts_potentiometer > 4.0) {
    digitalWrite(LED_pin, 1);
  }
  else {
    digitalWrite(LED_pin, 0);
  }

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
  lcd.print(" Volts:");
  lcd.print(volts_potentiometer);

  lcd.setCursor(0, 2);
  lcd.print("ADC time:");
  lcd.print(AD_conversion(2));
  lcd.setCursor(0, 3);
  lcd.print("AR time:");
  lcd.print(elapsedTime2);

  calculate(entirePulse, highPeriod);

  delay(500);
}

int AD_conversion(byte ch)
{
  int initTime = micros();
  //        76543210
  //ADMUX=B01000101; // CH 4 ja 5V ref
  ADMUX=B01000000 | ch; // Reference voltage = 5V / input channel = ch

  //        76543210
  ADCSRA=B11000111;

  ADCSRA |= B01000000;

  //delay(500);
  while(ADCSRA & B01000000); // loop to wait until conversion is ready.

  int endTime = micros();
  int elapsedTime = endTime - initTime;
  Serial.println(elapsedTime);
  return elapsedTime; // function returns AD converted value
}

void printResults() {

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

