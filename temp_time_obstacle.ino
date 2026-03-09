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
#define LED_PIN 2
#define BUTTON_PIN 23
#define EXTRA_BUTTON_PIN 3

#define EXTRA_LED1_PIN 15
#define EXTRA_LED2_PIN 16
#define EXTRA_LED3_PIN 17
#define EXTRA_LED4_PIN 18
const char* ssid = "TiS";
const char* password = "teodorastefan";

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;

DHT dht(DHTPIN, DHTTYPE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

unsigned long lastUpdateMs = 0;
const unsigned long updateEvery = 500;

float lastTemp = NAN;
float lastHumidity = NAN;
bool lastObstacle = false;

float minTemp = NAN;
float maxTemp = NAN;
float sumTemp = 0.0;
float minHumidity = NAN;
float maxHumidity = NAN;
float sumHumidity = 0.0;
unsigned long sampleCount = 0;

unsigned long obstacleCount = 0;
bool previousObstacle = false;

bool silentMode = false;


bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
bool buttonPressed = false;
unsigned long buttonPressStart = 0;


bool lastExtraButtonState = HIGH;
unsigned long lastExtraDebounceTime = 0;
const unsigned long DEBOUNCE_MS = 80;
const unsigned long LONG_PRESS_MS = 1500;
const unsigned long VERY_LONG_PRESS_MS = 3000;
const unsigned long DOUBLE_CLICK_MS = 400;
const unsigned long EXTRA_LONG_PRESS_MS = 800;

bool extraLedsOn = false;
bool rainMode = false;
bool bounceMode = false;

bool extraClickPending = false;
unsigned long lastExtraClickTime = 0;
unsigned long extraPressStart = 0;

unsigned long lastRainStepMs = 0;
const unsigned long RAIN_STEP_MS = 120;
uint8_t rainIndex = 0;

unsigned long lastBounceStepMs = 0;
const unsigned long BOUNCE_STEP_MS = 120;
int8_t bounceIndex = 0;
int8_t bounceDir = 1;

uint8_t currentPage = 1;

const int TREND_SAMPLES = 5;
const float TREND_DELTA = 0.2;
float tempHistory[TREND_SAMPLES];
float humHistory[TREND_SAMPLES];
int trendCount = 0;
int tempTrend = 0;
int humTrend = 0;

String getTimeString();
void initDisplay();
void showMessage(const char* line1,
                 const char* line2 = "");
char getTrendChar(int trend);
void connectWiFi();
void syncTime();
void readSensors();
void updateDisplay();
void handleObstacleBuzzer();

void setup() {
  Serial.begin(115200);

  Wire.begin(22, 21);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("SSD1306 allocation failed");
    for (;;)
      ;
  }

  initDisplay();
  showMessage("Starting...", "");

  dht.begin();

  pinMode(OBSTACLE_PIN, INPUT_PULLUP);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(EXTRA_BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(EXTRA_LED1_PIN, OUTPUT);
  pinMode(EXTRA_LED2_PIN, OUTPUT);
  pinMode(EXTRA_LED3_PIN, OUTPUT);
  pinMode(EXTRA_LED4_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(EXTRA_LED1_PIN, LOW);
  digitalWrite(EXTRA_LED2_PIN, LOW);
  digitalWrite(EXTRA_LED3_PIN, LOW);
  digitalWrite(EXTRA_LED4_PIN, LOW);

  Serial.println("Pin setup:");
  Serial.print("  BUTTON_PIN (mode/page) on GPIO");
  Serial.println(BUTTON_PIN);
  Serial.print("  EXTRA_BUTTON_PIN (extra LEDs) on GPIO");
  Serial.println(EXTRA_BUTTON_PIN);
  Serial.println("  Waiting for button events...");

  connectWiFi();
  syncTime();
}

void loop() {
  unsigned long now = millis();


  int reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonState) {
    lastDebounceTime = now;
  }

  if ((now - lastDebounceTime) > DEBOUNCE_MS) {

    if (reading == LOW && !buttonPressed) {

      buttonPressed = true;
      buttonPressStart = now;
    }

    if (reading == HIGH && buttonPressed) {

      buttonPressed = false;
      unsigned long pressDuration = now - buttonPressStart;


      if (pressDuration >= LONG_PRESS_MS) {

        currentPage++;
        if (currentPage > 3) currentPage = 1;
        Serial.print("Long press, page=");
        Serial.println(currentPage);
      } else {

        silentMode = !silentMode;
        Serial.print("Short press, silentMode=");
        Serial.println(silentMode ? "LED only" : "LED+buzzer");
      }
    }
  }

  lastButtonState = reading;


  int extraReading = digitalRead(EXTRA_BUTTON_PIN);

  if (extraReading != lastExtraButtonState) {
    Serial.print("Extra button state changed (raw): ");
    Serial.println(extraReading == LOW ? "LOW" : "HIGH");


    if (lastExtraButtonState == HIGH && extraReading == LOW) {
      extraPressStart = now;
    }


    if (lastExtraButtonState == LOW && extraReading == HIGH) {
      unsigned long pressDuration = now - extraPressStart;


      if (pressDuration >= EXTRA_LONG_PRESS_MS) {
        Serial.print("Extra button LONG press (");
        Serial.print(pressDuration);
        Serial.println(" ms) -> toggle bounce mode");


        extraClickPending = false;

        bounceMode = !bounceMode;
        if (bounceMode) {

          rainMode = false;
          extraLedsOn = false;
          bounceIndex = 0;
          bounceDir = 1;
          lastBounceStepMs = now;

          digitalWrite(EXTRA_LED1_PIN, LOW);
          digitalWrite(EXTRA_LED2_PIN, LOW);
          digitalWrite(EXTRA_LED3_PIN, LOW);
          digitalWrite(EXTRA_LED4_PIN, LOW);
          Serial.println("Bounce (KITT) mode ON");
        } else {

          digitalWrite(EXTRA_LED1_PIN, LOW);
          digitalWrite(EXTRA_LED2_PIN, LOW);
          digitalWrite(EXTRA_LED3_PIN, LOW);
          digitalWrite(EXTRA_LED4_PIN, LOW);
          Serial.println("Bounce (KITT) mode OFF");
        }
      } else {

        if (!extraClickPending) {

          extraClickPending = true;
          lastExtraClickTime = now;
          Serial.println("Extra button first click (pending)...");
        } else {

          if (now - lastExtraClickTime <= DOUBLE_CLICK_MS) {
            extraClickPending = false;

            rainMode = !rainMode;
            bounceMode = false;
            extraLedsOn = false;
            if (rainMode) {
              Serial.println("Extra button DOUBLE click -> rain mode ON");
              rainIndex = 0;
              lastRainStepMs = now;
            } else {
              Serial.println("Extra button DOUBLE click -> rain mode OFF");
              digitalWrite(EXTRA_LED1_PIN, LOW);
              digitalWrite(EXTRA_LED2_PIN, LOW);
              digitalWrite(EXTRA_LED3_PIN, LOW);
              digitalWrite(EXTRA_LED4_PIN, LOW);
            }
          } else {

            Serial.println("Extra button single click (timeout) -> toggle LEDs");
            rainMode = false;
            bounceMode = false;
            extraLedsOn = !extraLedsOn;
            digitalWrite(EXTRA_LED1_PIN, extraLedsOn ? HIGH : LOW);
            digitalWrite(EXTRA_LED2_PIN, extraLedsOn ? HIGH : LOW);
            digitalWrite(EXTRA_LED3_PIN, extraLedsOn ? HIGH : LOW);
            digitalWrite(EXTRA_LED4_PIN, extraLedsOn ? HIGH : LOW);
            Serial.print("Extra LEDs now ");
            Serial.println(extraLedsOn ? "ON" : "OFF");


            extraClickPending = true;
            lastExtraClickTime = now;
          }
        }
      }
    }

    lastExtraButtonState = extraReading;
  }


  if (extraClickPending && (now - lastExtraClickTime > DOUBLE_CLICK_MS)) {
    extraClickPending = false;
    Serial.println("Extra button single click (resolved) -> toggle LEDs");
    rainMode = false;
    bounceMode = false;
    extraLedsOn = !extraLedsOn;
    digitalWrite(EXTRA_LED1_PIN, extraLedsOn ? HIGH : LOW);
    digitalWrite(EXTRA_LED2_PIN, extraLedsOn ? HIGH : LOW);
    digitalWrite(EXTRA_LED3_PIN, extraLedsOn ? HIGH : LOW);
    digitalWrite(EXTRA_LED4_PIN, extraLedsOn ? HIGH : LOW);
    Serial.print("Extra LEDs now ");
    Serial.println(extraLedsOn ? "ON" : "OFF");
  }


  if (rainMode && (now - lastRainStepMs >= RAIN_STEP_MS)) {
    lastRainStepMs = now;


    digitalWrite(EXTRA_LED1_PIN, LOW);
    digitalWrite(EXTRA_LED2_PIN, LOW);
    digitalWrite(EXTRA_LED3_PIN, LOW);
    digitalWrite(EXTRA_LED4_PIN, LOW);


    switch (rainIndex) {
      case 0: digitalWrite(EXTRA_LED1_PIN, HIGH); break;
      case 1: digitalWrite(EXTRA_LED2_PIN, HIGH); break;
      case 2: digitalWrite(EXTRA_LED3_PIN, HIGH); break;
      case 3: digitalWrite(EXTRA_LED4_PIN, HIGH); break;
    }

    rainIndex = (rainIndex + 1) % 4;
  }


  if (bounceMode && (now - lastBounceStepMs >= BOUNCE_STEP_MS)) {
    lastBounceStepMs = now;

    digitalWrite(EXTRA_LED1_PIN, LOW);
    digitalWrite(EXTRA_LED2_PIN, LOW);
    digitalWrite(EXTRA_LED3_PIN, LOW);
    digitalWrite(EXTRA_LED4_PIN, LOW);


    switch (bounceIndex) {
      case 0: digitalWrite(EXTRA_LED1_PIN, HIGH); break;
      case 1: digitalWrite(EXTRA_LED2_PIN, HIGH); break;
      case 2: digitalWrite(EXTRA_LED3_PIN, HIGH); break;
      case 3: digitalWrite(EXTRA_LED4_PIN, HIGH); break;
    }


    bounceIndex += bounceDir;
    if (bounceIndex >= 3) {
      bounceIndex = 3;
      bounceDir = -1;
    } else if (bounceIndex <= 0) {
      bounceIndex = 0;
      bounceDir = 1;
    }
  }

  if (now - lastUpdateMs >= updateEvery) {
    lastUpdateMs = now;

    if (WiFi.status() != WL_CONNECTED) {
      connectWiFi();
      syncTime();
    }

    readSensors();
    handleObstacleBuzzer();
    updateDisplay();
  }
}

void initDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.display();
}

void showMessage(const char* line1,
                 const char* line2) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(line1);
  if (line2 && line2[0] != '\0') {
    display.println(line2);
  }
  display.display();
}

void connectWiFi() {
  showMessage("Connecting WiFi...");
  WiFi.begin(ssid, password);

  const unsigned long start = millis();
  const unsigned long timeoutMs = 15000;

  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi connected. IP: ");
    Serial.println(WiFi.localIP());
    showMessage("WiFi connected", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("WiFi connect failed");
    showMessage("WiFi FAILED", "Check network");
  }
}

void syncTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  struct tm timeinfo;
  int retries = 0;
  const int maxRetries = 10;

  while (!getLocalTime(&timeinfo) && retries < maxRetries) {
    Serial.println("Waiting for NTP time...");
    showMessage("Syncing time...", "");
    delay(1000);
    retries++;
  }

  if (retries < maxRetries) {
    Serial.println("Time synced!");
    showMessage("Time synced!", "");
  } else {
    Serial.println("Time sync FAILED");
    showMessage("Time sync FAILED", "");
  }
}

String getTimeString() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return String("--:--:--");
  }

  char timeString[9];
  strftime(timeString, sizeof(timeString), "%H:%M:%S", &timeinfo);
  return String(timeString);
}

void readSensors() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (isnan(t) || isnan(h)) {
    Serial.println("Failed to read from DHT sensor!");
  } else {
    lastTemp = t;
    lastHumidity = h;

    sampleCount++;
    if (sampleCount == 1) {
      minTemp = t;
      maxTemp = t;
      sumTemp = t;
      minHumidity = h;
      maxHumidity = h;
      sumHumidity = h;
    } else {
      if (t < minTemp) minTemp = t;
      if (t > maxTemp) maxTemp = t;
      sumTemp += t;

      if (h < minHumidity) minHumidity = h;
      if (h > maxHumidity) maxHumidity = h;
      sumHumidity += h;
    }
  }

  if (!isnan(t) && !isnan(h)) {
    for (int i = TREND_SAMPLES - 1; i > 0; i--) {
      tempHistory[i] = tempHistory[i - 1];
      humHistory[i] = humHistory[i - 1];
    }
    tempHistory[0] = t;
    humHistory[0] = h;
    if (trendCount < TREND_SAMPLES) {
      trendCount++;
    }

    if (trendCount >= 2) {
      float dT = tempHistory[0] - tempHistory[trendCount - 1];
      float dH = humHistory[0] - humHistory[trendCount - 1];

      if (dT > TREND_DELTA) {
        tempTrend = 1;
      } else if (dT < -TREND_DELTA) {
        tempTrend = -1;
      } else {
        tempTrend = 0;
      }

      if (dH > TREND_DELTA) {
        humTrend = 1;
      } else if (dH < -TREND_DELTA) {
        humTrend = -1;
      } else {
        humTrend = 0;
      }
    } else {
      tempTrend = 0;
      humTrend = 0;
    }
  }

  int rawObstacle = digitalRead(OBSTACLE_PIN);

  bool currentObstacle = (rawObstacle == LOW);

  if (currentObstacle && !previousObstacle) {
    obstacleCount++;
    bool beepPlanned = (!silentMode && (obstacleCount % 3 == 1));
    Serial.print("Obstacle #");
    Serial.print(obstacleCount);
    Serial.print(" silentMode=");
    Serial.print(silentMode ? "LED" : "BUZZ");
    Serial.print(" beep=");
    Serial.println(beepPlanned ? "YES" : "NO");
  }

  lastObstacle = currentObstacle;
  previousObstacle = currentObstacle;
}

char getTrendChar(int trend) {
  if (trend > 0) return '^';
  if (trend < 0) return 'v';
  return '-';
}

void handleObstacleBuzzer() {
  if (lastObstacle) {
    digitalWrite(LED_PIN, HIGH);

    if (silentMode) {

      digitalWrite(BUZZER_PIN, LOW);
    } else if (obstacleCount > 0 && (obstacleCount % 3 == 1)) {

      digitalWrite(BUZZER_PIN, HIGH);
    } else {
      digitalWrite(BUZZER_PIN, LOW);
    }
  } else {
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(LED_PIN, LOW);
  }
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);

  if (currentPage == 1) {

    display.setCursor(0, 0);
    display.print("Time: ");
    display.println(getTimeString());

    display.setCursor(0, 16);
    display.print("T:");
    if (isnan(lastTemp)) {
      display.print("--");
    } else {
      display.print(lastTemp, 1);
    }
    display.print("C ");
    display.print(getTrendChar(tempTrend));
    display.print(" V:");
    if (isnan(lastHumidity)) {
      display.print("--");
    } else {
      display.print(lastHumidity, 1);
    }
    display.print("% ");
    display.println(getTrendChar(humTrend));

    display.setCursor(0, 32);
    display.print("Obs:");
    display.print(lastObstacle ? "YES " : "NO  ");
    display.print("Cnt:");
    display.println(obstacleCount);

    display.setCursor(0, 48);
    display.print("Mode:");
    display.print(silentMode ? "LED " : "BUZ3");
    display.print(" Pg:");
    display.println(currentPage);

  } else if (currentPage == 2) {

    display.setCursor(0, 0);
    display.println("Stats T/V");

    if (sampleCount == 0) {
      display.setCursor(0, 16);
      display.println("No data yet");
    } else {
      float avgT = sumTemp / sampleCount;
      float avgH = sumHumidity / sampleCount;

      display.setCursor(0, 16);
      display.print("T mn:");
      display.print(minTemp, 1);
      display.print(" mx:");
      display.print(maxTemp, 1);

      display.setCursor(0, 32);
      display.print("T avg:");
      display.print(avgT, 1);
      display.println("C");

      display.setCursor(0, 48);
      display.print("V mn:");
      display.print(minHumidity, 1);
      display.print(" mx:");
      display.print(maxHumidity, 1);
      display.print(" a:");
      display.print(avgH, 1);
      display.println("%");
    }
  } else {

    display.setCursor(0, 0);
    display.println("Debug/info");

    display.setCursor(0, 16);
    display.print("Obs:");
    display.print(lastObstacle ? "YES " : "NO  ");
    display.print("Cnt:");
    display.println(obstacleCount);

    display.setCursor(0, 32);
    display.print("Mode:");
    display.println(silentMode ? "LED" : "BUZ3");

    display.setCursor(0, 48);
    if (WiFi.status() == WL_CONNECTED) {
      display.print("WiFi OK ");
      display.print(WiFi.localIP());
    } else {
      display.print("WiFi OFF");
    }
  }

  display.display();
}