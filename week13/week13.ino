#include <LiquidCrystal.h>
#include <Keypad.h>
#include <stdio.h>
#include <PubSubClient.h>
#include <Ethernet.h>
#include <SPI.h>
#include <Wire.h>

#define MAC_6 0x69
#define ETHERNET_CS_PIN 10

byte server[] = {10,6,0,23}; // MQTT server IP address
unsigned int Port = 1883;         // MQTT server port
EthernetClient ethClient;

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println("Message received");
}

PubSubClient client(server, Port, callback, ethClient);

#define outTopic "ICT4_out_2020"

static uint8_t mymac[6] = { 0x44, 0x76, 0x58, 0x10, 0x00, MAC_6 }; // MAC address for Ethernet

char* clientId = "a731fsd9";
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

// LCD setup
const int rs = 7, en = 6, d4 = 5, d5 = 4, d6 = 9, d7 = 8;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// Wind speed variables
const int signalPin = 3;
volatile unsigned long pulseCount = 0;
unsigned long prevTime = 0;
float frequency = 0.0;
float windSpeed = 0.0;

// Keypad setup
const byte ROWS = 1;
const byte COLS = 4;
char hexaKeys[ROWS][COLS] = {
  {'1', '2', '3', 'A'}
};
byte rowPins[ROWS] = {A4};
byte colPins[COLS] = {A0, A1, A2, A3};
Keypad customKeypad = Keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS);
char lastKeyPressed = '1';

// Wind direction setup
const int windDirPin = A7;
float windDirVolts = 0.0;

IPAddress IP;

// ISR for pulse counting
void signalISR() {
  pulseCount++;
}

void setup() {
  Serial.begin(9600);
  lcd.begin(20, 4);
  
  // Initialize variables
  pulseCount = 0;
  frequency = 0.0;
  windSpeed = 0.0;

  // Clear sample arrays
  for (int i = 0; i < 10; i++) {
    windDirSamples[i] = 0.0;
    windSpeedSamples[i] = 0.0;
  }

  // Pin setup
  pinMode(signalPin, INPUT);
  pinMode(windDirPin, INPUT);
  attachInterrupt(digitalPinToInterrupt(signalPin), signalISR, RISING);

  // Network setup
  fetchIP();
  connect_MQTT_server();
}

void loop() {
  unsigned long currentTime = millis();
  
  // Check for keypad input
  char customKey = customKeypad.getKey();
  if (customKey) {
    Serial.println(customKey);
    lastKeyPressed = customKey;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Key pressed:");
    lcd.print(customKey);
  }

  // Sample at the defined interval (500ms = 2 samples/second)
  if (currentTime - lastSampleTime >= sampleInterval) {
    measureHz();
    measureWindDirection();
    addSample();
    lastSampleTime = currentTime;
  }
  
  // Update averages and display every 5 seconds
  if (currentTime - displayUpdateTime >= averagePeriod) {
    calculateAverages();
    displayUpdateTime = currentTime;
    // Update display after calculating new averages
    updateDisplay();
  }

  delay(50);
}

void updateDisplay() {
  // Always show IP address at top
  lcd.setCursor(0, 0);
  lcd.print("IP:");
  lcd.print(IP);
  
  // Display based on the last key pressed
  if (lastKeyPressed == '1') {
    displayHz();
  }
  else if (lastKeyPressed == '2') {
    send_MQTT_message_wind_speed();
    displayAvgWindSpeed();
  }
  else if (lastKeyPressed == '3') {
    send_MQTT_message_wind_direction();
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
  
  float sinSum = 0.0;
  float cosSum = 0.0;
  int dirCount = min(windDirSampleCount, 10);
  
  for (int i = 0; i < dirCount; i++) {
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
  
  // Debug output
  Serial.print("5s Avg Wind Speed: ");
  Serial.print(avgWindSpeed);
  Serial.print(" m/s, Avg Direction: ");
  Serial.print(avgWindDirection);
  Serial.println("°");
}

void displayAvgWindSpeed() {
  lcd.setCursor(0, 1);
  lcd.print("Avg Wind Speed: ");
  lcd.print(avgWindSpeed, 1); // Show one decimal place
  lcd.setCursor(0, 2);
  lcd.print("m/s (5s average)    ");
}

void displayAvgWindDirection() {
  lcd.setCursor(0, 1);
  lcd.print("Avg Wind Dir: ");
  lcd.print(avgWindDirection, 1);
  lcd.print((char)223); // Degree symbol
  
  lcd.setCursor(0, 2);
  String directionStr = getDirectionString(avgWindDirection);
  lcd.print(directionStr);
  lcd.print(" (5s avg)    ");
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
  lcd.setCursor(0, 1);
  lcd.print("Wind Volts: ");
  lcd.print(windDirVolts, 2); // 2 decimal places
  lcd.setCursor(18, 1);
  lcd.print("V");
}

void displayHz() {
  lcd.setCursor(0, 1);
  lcd.print("Frequency: ");
  lcd.print(frequency, 1);
  lcd.setCursor(18, 1);
  lcd.print("Hz");
  
  lcd.setCursor(0, 2);
  lcd.print("Wind Speed: ");
  lcd.print(windSpeed, 1);
  lcd.print(" m/s");
}

void measureHz() {
  unsigned long currentTime = millis();
  
  // Calculate frequency from pulse count
  //noInterrupts();
  unsigned long localPulseCount = pulseCount;
  pulseCount = 0; // Reset counter
  //interrupts();
  
  frequency = localPulseCount * (1000.0 / sampleInterval);
  
  // Convert frequency to wind speed (0.699 is the calibration factor)
  windSpeed = frequency * 0.699 - 0.24;
  
  // If no pulses for 3 seconds, consider it zero
  if (localPulseCount == 0 && (currentTime - lastSampleTime > 3000)) {
    frequency = 0.0;
    windSpeed = 0.0;
  }
}  attachInterrupt(digitalPinToInterrupt(signalPin), signalISR, RISING);


void send_MQTT_message_wind_speed() { 
    if (!client.connected()) { 
      connect_MQTT_server();
    }
    if (client.connected()) { 
      // Create proper JSON format
      String jsonMessage = "{\"device\":\"supersonic2025\",\"wind_speed\":" + String(avgWindSpeed, 2) + "}";
      
      // Use the actual topic and JSON string
      boolean publishResult = client.publish("WindSpeed", jsonMessage.c_str());
      
      if (publishResult) {
          Serial.println("Wind speed sent to MQTT server");
          lcd.setCursor(0, 3);
          lcd.print("MQTT: Speed sent");
      } else {
          Serial.println("Failed to publish wind speed");
          lcd.setCursor(0, 3);
          lcd.print("MQTT: Send failed");
      }
    } else { 
        Serial.println("Unable to connect to MQTT server");
        lcd.setCursor(0, 3);
        lcd.print("MQTT: Not connected");
    } 
}

void send_MQTT_message_wind_direction() { 
    char valueStr[20];
    dtostrf(avgWindDirection, 4, 2, valueStr);
    char msg[50];
    sprintf(msg, "{Supersonic_wind_direction: %s degrees}", valueStr);
    Serial.println(msg);
    if (!client.connected()) {
        connect_MQTT_server();

    }

    if (client.connected()) {
      Serial.println("this is the jsonspeed info");
      bool publishResult = client.publish(outTopic, msg);
        if (publishResult) {
            Serial.println("Message sent to MQTT server.1");
        } else {
            Serial.println("Failed to publish message.");
        }
    } else {
        Serial.println("Unable to connect to MQTT server.");
    }
}

void connect_MQTT_server() {  
  Serial.println("Connecting to MQTT"); 
  if (client.connect(clientId, deviceId, deviceSecret)) { 
      Serial.println("Connected OK"); 
  } else { 
    Serial.println("Connection failed."); 
    Serial.println(client.state());
  }     
}

void fetchIP() {
  byte connection = 1;
  connection = Ethernet.begin(mymac);
  if (connection == 0) {
    Serial.println(F("Failed to access Ethernet controller"));
  }
  
  Serial.println(F("Setting up DHCP"));
  Serial.print("Connected with IP: ");
  Serial.println(Ethernet.localIP());
  IP = Ethernet.localIP();

  delay(1500);
}
