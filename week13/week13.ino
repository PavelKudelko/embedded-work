#include <LiquidCrystal.h>
#include <Keypad.h>

#include <PubSubClient.h>

#include <Ethernet.h>
#include <SPI.h>
#include <Wire.h>
// #define MAC_6    0x73
// Define custom pins if not using standard ones
#define ETHERNET_CS_PIN 10
// might need fixing
// static uint8_t mymac[6] = { 0x44, 0x76, 0x58, 0x10, 0x00, MAC_6 };




byte server[] = { 10,6,0,21 }; // MQTT-palvelimen IP-osoite 
unsigned int Port = 1883;  // MQTT-palvelimen portti 
EthernetClient ethClient; // Ethernet-kirjaston client-olio 
//void callback(char* topic, byte* payload, unsigned int length);
PubSubClient client(server, Port,  ethClient); // PubSubClient-olion luominen 
 
#define outTopic   "ICT4_out_2020" // Aihe, jolle viesti lähetetään 
 
static uint8_t mymac[6] = { 0x44,0x76,0x58,0x10,0x00,0x62 }; // MAC-osoite Ethernet-liitäntää varten 
 
char* clientId = "a731fsd4";
char* deviceId = "supersonic2025";
char* deviceSecret = "tamk";

// Sampling and averaging variables
unsigned long sampleInterval = 500; // 500ms = 2 samples per second
unsigned long lastSampleTime = 0;
unsigned long displayUpdateTime = 0;
const unsigned long averagePeriod = 5000; // 5 seconds

// Wind direction averaging variables
float windDirSamples[10]; // Enough for 10 samples in 5 seconds at 2 samples/second
int windDirSampleCount = 0;
float windDirTotal = 0.0;
float avgWindDirection = 0.0;

// Wind speed averaging variables
float windSpeedSamples[10]; // Same array size
int windSpeedSampleCount = 0;
float windSpeedTotal = 0.0;
float avgWindSpeed = 0.0;

const int rs = 7, en = 6, d4 = 5, d5 = 4, d6 = 9, d7 = 8;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);
const int signalPin = 3;
volatile int pressCount = 0;
unsigned long lastPressTime = 0;
unsigned long prevTime = 0;
float frequency = 0.0;
float windSpeed = 0.0;

volatile unsigned int buttonPressCount = 0; 
volatile unsigned long lastInterruptTime = 0; 
const unsigned long debounceDelay = 50;
volatile unsigned long pulseCount = 0;
const byte ROWS = 1;  
const byte COLS = 4;  
char hexaKeys[ROWS][COLS] = {
  {'1', '2', '3', 'A'}
};
byte rowPins[ROWS] = {A4};
byte colPins[COLS] = {A0, A1, A2, A3};  

Keypad customKeypad = Keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS);

char lastKeyPressed = '1';

const int windDirPin = A7;

float windDirVolts = 0.0;

IPAddress IP;


void signalISR() {
    pulseCount++;
}


void setup() {
  Serial.begin(9600);
  // set up the LCD's number of columns and rows:
  lcd.begin(20, 4);
  pulseCount = 0;
  frequency = 0.0;
  windSpeed = 0.0;

    // Clear sample arrays
  for (int i = 0; i < 10; i++) {
    windDirSamples[i] = 0.0;
    windSpeedSamples[i] = 0.0;
  }
  // pinMode(LED_BUILTIN, OUTPUT);
  // pinMode(6, INPUT);
  // pinMode(A2, INPUT);
  // pinMode(13, OUTPUT);

  pinMode(signalPin, INPUT);
  pinMode(windDirPin, INPUT);

  attachInterrupt(digitalPinToInterrupt(signalPin), signalISR, RISING);

  fetchIP();
  connect_MQTT_server();
}

void loop() {

  send_MQTT_message();

  // unsigned long currentTime = millis();
  

  //   // Sample at the defined interval (500ms = 2 samples/second)
  // if (currentTime - lastSampleTime >= sampleInterval) {
  //   measureHz();
  //   measureWindDirection();
  //   addSample();
  //   lastSampleTime = currentTime;
  // }
  
  // // Update averages and display every 5 seconds
  // if (currentTime - displayUpdateTime >= averagePeriod) {
  //   calculateAverages();
  //   displayUpdateTime = currentTime;
  // }


  
  // char customKey = customKeypad.getKey();
  
  // if (customKey) {
  //   Serial.println(customKey);
  //   lastKeyPressed = customKey;
  //   lcd.clear(); // Clear the screen when a new key is pressed
  // }

  // updateDisplay();

  delay(5000);
}




void updateDisplay(){
    // Display based on the last key pressed
  if (lastKeyPressed == '1') {
    displayHz();
  }
  else if (lastKeyPressed == '2') {
    displayIP();
    displayAvgWindSpeed();
  }
  else if (lastKeyPressed == '3'){
    displayAvgWindDirection();
  }
  else if (lastKeyPressed == 'A') {
    displayWindVolts();
  }
  
}

void addSample() {
  // Add current wind speed to samples array
  windSpeedSamples[windSpeedSampleCount % 10] = windSpeed;
  windSpeedSampleCount++;
  
  // Add current wind direction to samples array
  float windDirDegree = getWindDirectionDegree(windDirVolts);
  windDirSamples[windDirSampleCount % 10] = windDirDegree;
  windDirSampleCount++;
}

void calculateAverages() {
  // Calculate average wind speed
  windSpeedTotal = 0.0;
  int speedCount = min(windSpeedSampleCount, 10);
  for (int i = 0; i < speedCount; i++) {
    windSpeedTotal += windSpeedSamples[i];
  }
  avgWindSpeed = (speedCount > 0) ? windSpeedTotal / speedCount : 0.0;
  
  // Calculate average wind direction (special handling for circular data)
  float sinSum = 0.0;
  float cosSum = 0.0;
  int dirCount = min(windDirSampleCount, 10);
  
  for (int i = 0; i < dirCount; i++) {
    // Convert to radians and accumulate vector components
    float radians = windDirSamples[i] * PI / 180.0;
    sinSum += sin(radians);
    cosSum += cos(radians);
  }
  
  // Calculate the average angle using atan2
  if (dirCount > 0) {
    float avgRadians = atan2(sinSum / dirCount, cosSum / dirCount);
    avgWindDirection = avgRadians * 180.0 / PI;
    if (avgWindDirection < 0) {
      avgWindDirection += 360.0;
    }
  } else {
    avgWindDirection = 0.0;
  }
  
  // Reset counters for next period
  windSpeedSampleCount = 0;
  windDirSampleCount = 0;
  
  Serial.print("5s Avg Wind Speed: ");
  Serial.print(avgWindSpeed);
  Serial.print(" m/s, Avg Direction: ");
  Serial.print(avgWindDirection);
  Serial.println("°");
}


void displayAvgWindSpeed() {
  lcd.setCursor(0, 1);
  lcd.print("Avg Wind Speed: ");
  lcd.print(avgWindSpeed);
  lcd.setCursor(0, 2);
  lcd.print("m/s (5s average)    ");
}

void displayAvgWindDirection() {
  lcd.setCursor(0, 1);
  lcd.print("Avg Wind Dir: ");
  lcd.print(avgWindDirection);
  lcd.print((char)223); // Degree symbol
  
  lcd.setCursor(0, 2);
  String directionStr = getDirectionString(avgWindDirection);
  lcd.print(directionStr);
  lcd.print(" (5s average)    ");
}

void measureWindDirection() {
  float windDirAnalog = analogRead(windDirPin);
  windDirVolts = windDirAnalog * (5.0 / 1023.0);
}

float getWindDirectionDegree(float voltage) {
  
  if (voltage < 1.44) {       
    return 0;                 // North (0°)
  } else if (voltage < 1.91){
    return 45;                // North East (45°)
  } else if (voltage < 2.39){ 
    return 90;                // East (90°)
  } else if (voltage < 2.86){ 
    return 135;               // South East (135°)
  } else if (voltage < 3.34){
    return 180;               // South (180°)
  } else if (voltage < 3.81){ 
    return 225;               // South West (225°)
  } else if (voltage < 4.29){ 
    return 270;               // West (270°)
  } else {                    
    return 315;               // North West (315°)
  }
  
}


String getDirectionString(float degree) {
  // Round to nearest compass direction
  if (degree >= 337.5 || degree < 22.5) return "North";
  if (degree >= 22.5 && degree < 67.5) return "North East";
  if (degree >= 67.5 && degree < 112.5) return "East";
  if (degree >= 112.5 && degree < 157.5) return "South East";
  if (degree >= 157.5 && degree < 202.5) return "South";
  if (degree >= 202.5 && degree < 247.5) return "South West";
  if (degree >= 247.5 && degree < 292.5) return "West";
  if (degree >= 292.5 && degree < 337.5) return "North West";
  return "Unknown";
}

void displayWindVolts() {
  lcd.setCursor(0,1);
  lcd.print("Wind Volts: ");
  lcd.print(windDirVolts);
  lcd.setCursor(18, 1);
  lcd.print("V");
}


void displayHz() {
  lcd.setCursor(0,1);
  lcd.print("frequency: ");
  lcd.print(frequency);
  lcd.setCursor(18, 1);
  lcd.print("hz");
}

void displayIP() {
  lcd.setCursor(0, 0);
  lcd.print("IP:");
  lcd.print(IP);
}

void displayWindSpeed() {
  unsigned long currentTime = millis();
  lcd.setCursor(0,1);

    lcd.print("avg WindSpeed: ");
    lcd.print(avgWindSpeed);
    lcd.setCursor(17, 1);
    lcd.print("m/s");
    prevTime = currentTime;
  
}



void measureHz() {
  unsigned long currentTime = millis();
  
  // Use shorter measurement window to get more frequent updates
  if (currentTime - prevTime >= sampleInterval) { 
    noInterrupts();
    // Scale the pulse count to get Hz (pulses per second)
    // If we sample every 500ms, multiply by 2 to get Hz
    frequency = pulseCount * (1000.0 / sampleInterval);
    pulseCount = 0;
    interrupts();

    windSpeed = frequency * 0.7; // Convert to wind speed
    prevTime = currentTime;
    
    // If no signals for a longer period, reset frequency to zero
    if (frequency == 0 && (currentTime - lastSampleTime > 3000)) {
      frequency = 0.0;
      windSpeed = 0.0;
    }
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

void send_MQTT_message() { 
    if (!client.connected()) { // Tarkistetaan onko yhteys MQTT-brokeriin muodostettu 
        connect_MQTT_server(); // Jos yhteyttä ei ollut, kutsutaan yhdistä -funktiota 
    } 
    if (client.connected()) { // Jos yhteys on muodostettu 
        client.publish(outTopic, "Hello from MQTT!"); // Lähetetään viesti MQTT-brokerille 
        Serial.println("Message sent to MQTT server."); // Tulostetaan viesti onnistuneesta lähettämisestä 
    } else { 
        Serial.println("Failed to send message: not connected to MQTT server."); // Ei yhteyttä -> Yhteysvirheilmoitus 
    } 
}

void connect_MQTT_server() {  
    Serial.println("Connecting to MQTT"); // Tulostetaan vähän info-viestiä 
    if (client.connect(clientId, deviceId, deviceSecret)) { // Tarkistetaan saadaanko yhteys MQTT-brokeriin 
        Serial.println("Connected OK"); // Yhdistetty onnistuneesti 
    } else { 
        Serial.println("Connection failed."); // Yhdistäminen epäonnistui 
    }     
}

//modified new fetchIP from teacher file 
void fetchIP() {
  byte connection = 1;
  connection = Ethernet.begin(mymac);
  // lcd.setCursor(0, 1);
  // lcd.print(("\nW5100 Revision "));
  if (connection == 0) {
    //lcd.print(("Failed to access Ethernet controller"));
    Serial.println(F("Failed to access Ethernet controller"));
  }
  
  // lcd.print(("Setting up DHCP"));
  // lcd.print("Connected with IP: ");

  // IPAddress ip = Ethernet.localIP();
  // lcd.print(ip);
  
  Serial.println(F("Setting up DHCP"));
  Serial.print("Connected with IP: ");
  Serial.println(Ethernet.localIP());
  IP = Ethernet.localIP();

  delay(1500);
}

