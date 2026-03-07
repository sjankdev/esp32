#include <WiFi.h>
#include "time.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

#define DHTPIN 4
#define DHTTYPE DHT11

#define OBSTACLE_PIN 5
#define BUZZER_PIN 19


const char* ssid = "TiS";
const char* password = "teodorastefan";

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;      
const int daylightOffset_sec = 3600;  

DHT dht(DHTPIN, DHTTYPE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

String getTime() {

  struct tm timeinfo;

  if (!getLocalTime(&timeinfo)) {
    return "--:--:--";
  }

  char timeString[9];
  strftime(timeString, sizeof(timeString), "%H:%M:%S", &timeinfo);

  return String(timeString);
}

void setup() {

  Serial.begin(115200);

  Wire.begin(22,21);

  display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("Connecting WiFi...");
  display.display();

  dht.begin();

  pinMode(OBSTACLE_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("WiFi Connected");

  display.clearDisplay();
  display.setCursor(0,0);
  display.println("WiFi Connected");
  display.display();


  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  struct tm timeinfo;

  while(!getLocalTime(&timeinfo)){
    Serial.println("Waiting for NTP time...");
    delay(1000);
  }

  Serial.println("Time synced!");
}

void loop() {

  float temperatura = dht.readTemperature();
  float vlaga = dht.readHumidity();

  int obstacle = digitalRead(OBSTACLE_PIN);

  display.clearDisplay();

  display.setTextSize(1);

  display.setCursor(0,0);
  display.print("Time: ");
  display.println(getTime());

  display.setCursor(0,20);
  display.print("Temp: ");
  display.print(temperatura);
  display.println(" C");

  display.setCursor(0,35);
  display.print("Vlaga: ");
  display.print(vlaga);
  display.println(" %");

  if (obstacle == LOW) {

    display.setCursor(0,55);
    display.println("Objekat detektovan!");

    digitalWrite(BUZZER_PIN, HIGH);

  } else {

    digitalWrite(BUZZER_PIN, LOW);

  }

  display.display();

  delay(500);
}