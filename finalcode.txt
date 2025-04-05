#include <LiquidCrystal.h>
#include <Keypad.h>
#include <PubSubClient.h>
#include <Ethernet.h>
#include <SPI.h>
#include <Wire.h>
#define MAC_6    0x69
// Define custom pins if not using standard ones
#define ETHERNET_CS_PIN 10
byte server[] = { 10,6,0,23 }; // MQTT-palvelimen IP-osoite
unsigned int Port = 1883;  // MQTT-palvelimen portti
EthernetClient ethClient; // Ethernet-kirjaston client-olio
void callback(char* topic, byte* payload, unsigned int length){
  Serial.println("Message receiveddddd");
};
PubSubClient client(server, Port,  callback, ethClient); // PubSubClient-olion luominen

#define outTopic   "ICT4_out_2020" // Aihe, jolle viesti lähetetään

static uint8_t mymac[6] = { 0x44,0x76,0x58,0x10,0x00, MAC_6 }; // MAC-osoite Ethernet-liitäntää varten

char* clientId = "a731fsd9";
char* deviceId = "sonic25";
char* deviceSecret = "tamk";

// Sampling and averaging variables
unsigned long sampleInterval = 500; // 500msa = 2 samples per second
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

//LCD SetUp
const int rs = 7, en = 6, d4 = 5, d5 = 4, d6 = 9, d7 = 8;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

const int signalPin = 3;
volatile unsigned long pulseCount = 0;
unsigned long prevTime = 0;
float frequency = 0.0;
float windSpeed = 0.0;


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

// Add these global variables
unsigned long lastMqttWindSpeedTime = 0;
unsigned long lastMqttWindDirTime = 0;
const unsigned long mqttSendInterval = 5000; // 5 seconds

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

  pinMode(signalPin, INPUT);
  pinMode(windDirPin, INPUT);

  attachInterrupt(digitalPinToInterrupt(signalPin), signalISR, RISING);

  fetchIP();
  connect_MQTT_server();
}

void loop() {
  unsigned long currentTime = millis();
  //check for keypad input first
  char customKey = customKeypad.getKey();
    if (customKey) {
      Serial.println(customKey);
      lastKeyPressed = customKey;
      lcd.clear(); // Clear the screen when a new key is pressed
      // lcd.setCursor(0, 0);
      // lcd.print("key pressed: ");
      // lcd.print(customKey);
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
 
    // update display was here
  }
       updateDisplay(currentTime);

}

void updateDisplay(unsigned long currentTime){
    // Always show IP address at top
    lcd.setCursor(0, 0);
    lcd.print("IP:");
    lcd.print(IP);

    // Display based on the last key pressed
    if (lastKeyPressed == '1') {
      displayMainInfo();
       if (currentTime - lastMqttWindSpeedTime >= mqttSendInterval) {
      send_MQTT_message_2_message();
      lastMqttWindSpeedTime = currentTime;
    }

    }
    else if (lastKeyPressed == '2') {
        displayAvgWindSpeed();
            if (currentTime - lastMqttWindSpeedTime >= mqttSendInterval) {
      send_MQTT_message_wind_speed();
      lastMqttWindSpeedTime = currentTime;
    }
     
    }
    else if (lastKeyPressed == '3'){
        displayAvgWindDirection();
       if (currentTime - lastMqttWindDirTime >= mqttSendInterval) {
      send_MQTT_message_wind_direction();
      lastMqttWindDirTime = currentTime;
    }
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

  Serial.print(" Avg Wind Speed: ");
  Serial.print(avgWindSpeed);
  Serial.print(" m/s, Avg Direction: ");
  Serial.print(avgWindDirection);
  Serial.println("°");
}


void displayAvgWindSpeed() {
  lcd.setCursor(0, 1);
  lcd.print("Speed: ");
  lcd.print(avgWindSpeed, 1);
  //show one decinmal place
  lcd.setCursor(14, 1);
  lcd.print("m/s");
}

void displayAvgWindDirection() {
  lcd.setCursor(0, 1);
  lcd.print("Direction: ");
  lcd.print(avgWindDirection, 1);
  lcd.setCursor(16, 1);
  lcd.print((char)223); // Degree symbol

  lcd.setCursor(0, 3);
  String directionStr = getDirectionString(avgWindDirection);
  lcd.print(directionStr);
  Serial.println("where is my northwest");
  Serial.println(directionStr);
}

void measureWindDirection() {
  float windDirAnalog = analogRead(windDirPin);
  windDirVolts = windDirAnalog * (5.0 / 1023.0);
}

float getWindDirectionDegree(float voltage) {

  if (voltage < 1.2) {
    return 0.0;                 // North (0°)
  } else if (voltage < 1.67){
    return 45.0;                // North East (45°)
  } else if (voltage < 2.15){
    return 90.0;                // East (90°)
  } else if (voltage < 2.63){
    return 135.0;               // South East (135°)
  } else if (voltage < 3.1){
    return 180.0;               // South (180°)
  } else if (voltage < 3.58){
    return 225.0;               // South West (225°)
  } else if (voltage < 4.405){
    return 270.0;               // West (270°)
  } else {
    return 315.0;               // North West (315°)
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

void displayMainInfo() {
  lcd.setCursor(0,1);
  lcd.print("frequency: ");
  lcd.print(frequency, 1);
  lcd.setCursor(17, 1);
  lcd.print("hz");

  lcd.setCursor(0,2);
  lcd.print("Speed: ");
  lcd.print(avgWindSpeed, 1);
  lcd.setCursor(11, 2);
  lcd.print("m/s");


  lcd.setCursor(0, 3);
  lcd.print("Direction: ");
  lcd.print(avgWindDirection, 2);
  lcd.setCursor(16, 3);
}

void measureHz() {
  unsigned long currentTime = millis();
  unsigned long actualInterval = currentTime - lastSampleTime;

  // Calculate frequency from pulse count
  noInterrupts();
  unsigned long localPulseCount = pulseCount;
  pulseCount = 0; // Reset counter
  interrupts();

  // Convert to frequency (pulses per second)
  // We're measuring for sampleInterval milliseconds, so scale to 1000ms
  frequency = localPulseCount * (1000.0 / actualInterval);

  // Convert frequency to wind speed 
  windSpeed = frequency * 0.699 - 0.24;

  // If no pulses for 3 seconds, consider it zero
  if (localPulseCount == 0 && (currentTime - lastSampleTime > 3000)) {
    frequency = 0.0;
    windSpeed = 0.0;
  }
}

// void printAlphabet() {
//   int row_length = 20;
//   for (int i = 0; i<100000; i++) {
//     int remainder_alpha = i % 26;
//     int alphabet = remainder_alpha + 65;
//     //the lettter to be printed is lcd.write(alpha)
//     int devided_row = i / row_length;
//     int remainder_column = i % row_length;
//     if (devided_row%2 == 1) {
//       lcd.setCursor(row_length - remainder_column - 1,1);
//       lcd.write (alphabet);
//     } else {
//       lcd.setCursor(remainder_column, 0);
//       lcd.write(alphabet);
//     }
//     delay(100);
//     lcd.clear();
//   }
// }

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
      bool publishResult = client.publish(outTopic, msg);
        if (publishResult) {
            Serial.println("Message sent to MQTT server about avgWindDirection");
        } else {
            Serial.println("Failed to publish message.");
        }
    } else {
        Serial.println("Unable to connect to MQTT server.");
    }
}
void send_MQTT_message_wind_speed() {

    char valueStr[20];
    dtostrf(avgWindSpeed, 4, 2, valueStr);
    char msg[50];
    sprintf(msg, "{Supersonic_wind_speed: %s m/s}", valueStr);
    Serial.println(msg);


    if (!client.connected()) {
        connect_MQTT_server();

    }

    if (client.connected()) {
         boolean publishResult = client.publish(outTopic,msg);
        if (publishResult) {
            Serial.println("Message sent to MQTT server about AvgWindSpeed");
        } else {
            Serial.println("Failed to publish message.");
        }
    } else {
        Serial.println("Unable to connect to MQTT server.");
    }
}
void  send_MQTT_message_2_message(){
    char valueStr_speed[20];
    char valueStr_direction[20];
    dtostrf(avgWindSpeed, 4, 2, valueStr_speed);
    dtostrf(avgWindDirection, 4, 2, valueStr_direction);
    char msg[128];
    sprintf(msg, "{\"Supersonic_wind_speed\": \"%s m/s\", \"Supersonic_wind_direction\": \"%s degree\"}", valueStr_speed, valueStr_direction);
    //sprintf(msg, "{Supersonic_wind_speed: %s m/s, Supersonic_wind_direction: %s degree}", valueStr_speed, valueStr_direction);
    Serial.println(msg);


    if (!client.connected()) {
        connect_MQTT_server();

    }

    if (client.connected()) {
         boolean publishResult = client.publish(outTopic,msg);
        if (publishResult) {
            Serial.println("Message sent to MQTT server about both messages");
        } else {
            Serial.println("Failed to publish message.");
        }
    } else {
        Serial.println("Unable to connect to MQTT server.");
    }

  }

void connect_MQTT_server() {
    Serial.println("Connecting to MQTT"); // Tulostetaan vähän info-viestiä
    if (client.connect(clientId, deviceId, deviceSecret)) { // Tarkistetaan saadaanko yhteys MQTT-brokeriin
        Serial.println("Connected OK"); // Yhdistetty onnistuneesti
    } else {
        Serial.println("Connection failed."); // Yhdistäminen epäonnistui
        Serial.println(client.state());
    }
}

//modified new fetchIP from teacher file
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
