#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <DHT.h>
#include <TinyGPS++.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <ThingSpeak.h>
#include <Wire.h>
#include <Adafruit_BMP085.h> 

// --- USER CONFIGURATION ---
const char* ssid = "Redmi Note 12 5G";
const char* password = "Bittu683";
const char* thingSpeakApiKey = "0GLEOK0LGVNBX8IA";
unsigned long myChannelNumber = 3272768; 
const char* botToken = "8562113222:AAHWW8ss07Iq8wyy2H1d3p6RV0KyLq48jR0";
const char* chatId = "6203499767";

// --- PIN DEFINITIONS ---
#define DHTPIN 4
#define DHTTYPE DHT11
#define MQ_PIN 34
#define BUTTON_PIN 12
#define BUZZER_PIN 13
#define GREEN_LED 14
#define RED_LED 27
#define RXD2 16 
#define TXD2 17 

// --- OBJECTS ---
DHT dht(DHTPIN, DHTTYPE);
TinyGPSPlus gps;
Adafruit_BMP085 bmp; 
HardwareSerial neogps(2);
WiFiClient client;
WiFiClientSecure secured_client;
UniversalTelegramBot bot(botToken, secured_client);


int Rates[] = {62, 67, 72, 77, 80, 85, 87, 92, 94, 96, 100, 102};
int totalValues = 12;

unsigned long lastTime = 0;
unsigned long ledTime = 0;
const unsigned long timerDelay = 20000; 
bool greenState = LOW;

void sendRescueAlert(float t, float h, int gas, int bpm, float press, float alt) {
  digitalWrite(GREEN_LED, LOW); // Stop green blinking
  
  // Red LED and Buzzer Alert sequence
  tone(BUZZER_PIN, 1000); 
  
  String msg = "🚨 SOLDIER RESCUE REQUIRED! 🚨\n\n";
  msg += "Temp: " + String(t) + "°C\n";
  msg += "Hum: " + String(h) + "%\n";
  msg += "Air Quality: " + String(gas) + "\n";
  msg += "Heart Rate: " + String(bpm) + " BPM\n";
  msg += "Pressure: " + String(press / 100.0) + " hPa\n";
  msg += "Altitude: " + String(alt) + " m\n";

  if (gps.location.isValid()) {
    msg += "\n📍 Location: https://www.google.com/maps?q=" + String(gps.location.lat(), 6) + "," + String(gps.location.lng(), 6);
  } else {
    msg += "\n📍 Location: Waiting for GPS fix...";
  }

  bot.sendMessage(chatId, msg, "");
  
  // Blink Red LED during the 3-second alert
  for(int i=0; i<6; i++) {
    digitalWrite(RED_LED, HIGH);
    delay(250);
    digitalWrite(RED_LED, LOW);
    delay(250);
  }
  
  noTone(BUZZER_PIN);
}

void setup() {
  Serial.begin(115200);
  neogps.begin(9600, SERIAL_8N1, RXD2, TXD2);
  
  dht.begin();
  Wire.begin(); 

  if (!bmp.begin()) {
    Serial.println("BMP180 sensor not found!");
  }

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, LOW);

  WiFi.begin(ssid, password);
  secured_client.setInsecure(); 
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected");
  
  ThingSpeak.begin(client);
  randomSeed(analogRead(35)); 
}

void loop() {
  while (neogps.available() > 0) {
    gps.encode(neogps.read());
  }

  // Normal Time: Blink Green LED every 1 second without blocking code
  if (millis() - ledTime > 1000) {
    ledTime = millis();
    greenState = !greenState;
    digitalWrite(GREEN_LED, greenState);
  }

  float currentTemp = dht.readTemperature();
  float currentHum = dht.readHumidity();  
  int currentHeartRate = Rates[random(0, totalValues)];
  int currentGas = random(600, 801); 
  float currentPressure = bmp.readPressure();
  float currentAltitude = bmp.readAltitude(); 

  // 1. Manual Alert Trigger (Button)
  if (digitalRead(BUTTON_PIN) == LOW) {
    sendRescueAlert(currentTemp, currentHum, currentGas, currentHeartRate, currentPressure, currentAltitude);
    delay(500); 
  }

  // 2. Cloud Update
  if ((millis() - lastTime) > timerDelay) {
    if (WiFi.status() == WL_CONNECTED) {
      ThingSpeak.setField(1, currentTemp);
      ThingSpeak.setField(2, currentHum);
      ThingSpeak.setField(3, (float)currentGas);       
      ThingSpeak.setField(4, (float)currentHeartRate); 
      ThingSpeak.setField(5, (float)(currentPressure / 100.0)); 
      ThingSpeak.setField(6, currentAltitude);       

      int x = ThingSpeak.writeFields(myChannelNumber, thingSpeakApiKey);

      if(x == 200) {
        Serial.println("ThingSpeak Update Successful.");
      } else {
        Serial.println("ThingSpeak Error: " + String(x));
      }
    }
    lastTime = millis();
  }
}