#include <LiquidCrystal.h>

const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 9, d7 = 8;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);
const int signalPin = 3;
volatile int pressCount = 0;
unsigned long lastPressTime = 0;
unsigned long prevTime = 0;
float frequency = 0.0;
const int buttonPin = 2;
volatile unsigned int buttonPressCount = 0; 
volatile unsigned long lastInterruptTime = 0; 
const unsigned long debounceDelay = 50;
volatile unsigned long pulseCount = 0;

void buttonISR() {
    unsigned long currentTime = millis();
    if (currentTime - lastInterruptTime > debounceDelay) { 
        buttonPressCount++;
        lastInterruptTime = currentTime;
    }
}

void signalISR() {
    pulseCount++;
}


void setup() {
  Serial.begin(9600);
  // set up the LCD's number of columns and rows:
  lcd.begin(20, 4);
  // pinMode(LED_BUILTIN, OUTPUT);
  // pinMode(6, INPUT);
  // pinMode(A2, INPUT);
  // pinMode(13, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
    pinMode(signalPin, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(buttonPin), buttonISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(signalPin), signalISR, RISING);
}

void loop() {

  //printAlphabet();

  measureHz();

  displayInfo();

}

void displayInfo() {
  int buttonState = digitalRead(buttonPin);
  lcd.setCursor(0,0);
  lcd.print("Button is");
  lcd.setCursor(11,0);

  if (buttonState) {
    lcd.print("pressed");
  } else {
    lcd.print("not pressed");
  }
  lcd.setCursor(0,2);
  lcd.print("press count:");
  lcd.print(buttonPressCount);

  lcd.setCursor(0,3);
  lcd.print("frequency: ");
  lcd.print(frequency);
  lcd.setCursor(18, 3);
  lcd.print("hz");
}

void measureHz() {
  unsigned long currentTime = millis();
  
  if (currentTime - prevTime >= 1000) { 
    noInterrupts();
    frequency = pulseCount;
    pulseCount = 0;
    interrupts();

    prevTime = currentTime;

    Serial.print("Frequency: ");
    Serial.print(frequency);
    Serial.println(" Hz");
  }
}

void printAlphabet() {
  int row_length = 20;
  for (int i = 0; i<100000; i++) {
    int remainder_alpha = i % 26;
    int alphabet = remainder_alpha + 65;
    //the lettter to be printed is lcd.write(alpha)
    int devided_row = i / row_length;
    int remainder_column = i % row_length;
    if (devided_row%2 == 1) {
      lcd.setCursor(row_length - remainder_column - 1,1);
      lcd.write (alphabet);
    } else {
      lcd.setCursor(remainder_column, 0);
      lcd.write(alphabet);
    }
    delay(100);
    lcd.clear();
  }
}


// void printAlphabet() {
//   int asciiNum = 65;  // Starting with 'A'
//   int row = 4;
//   int col = 20;
  
//   for (int i = 0; i < row * col; i++) {
//     if (i < col) {
//       lcd.setCursor(i, 0);  // First row
//       lcd.write(asciiNum);
//     }
//     else if (i < col * 2) {
//       lcd.setCursor(i - col, 1);  // Second row
//       lcd.write(asciiNum);
//     }
//     else if (i < col * 3) {
//       lcd.setCursor(i - (col * 2), 2);  // Third row (previously skipped)
//       lcd.write(asciiNum);
//     }
//     else if (i < col * 4) {
//       lcd.setCursor(i - (col * 3), 3);  // Fourth row
//       lcd.write(asciiNum);
//     }
    
//     // Reset ASCII number to 'A' if it goes beyond 'Z'
//     if (asciiNum >= 90) {
//       asciiNum = 65;
//     }
//     else {
//       asciiNum++;
//     }
    
//     delay(100);
//     lcd.clear();
//   }
// }
