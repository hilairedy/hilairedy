#define BLYNK_TEMPLATE_ID   "YOUR_TEMPLATE_ID"
#define BLYNK_TEMPLATE_NAME "YOUR_TEMPLATE_NAME"
#define BLYNK_AUTH_TOKEN    "YOUR_AUTH_TOKEN"



#include "DFRobot_Heartrate.h"
#include <TFT_eSPI.h>
#include "MPU6050.h"
#include <Wire.h>
#include <math.h> 
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <TimeLib.h>

#define HEARTRATE_PIN 32
#define BUZZER_PIN 26
#define VIBRATION_MOTOR_PIN 25  

char ssid[] = "YOUR_WIFI_SSID";
char pass[] = "YOUR_WIFI_PASSWORD";

DFRobot_Heartrate heartrate(DIGITAL_MODE);
TFT_eSPI tft = TFT_eSPI();
MPU6050 mpu;

BlynkTimer timer; 

int ecgX = 20;
int prevY = 120;
bool pulseDetected = false;
bool fallDetected = false;
unsigned long fallTime = 0;
bool alarmActive = false;

const unsigned char heartSymbol[] PROGMEM = {
  B00001110, B01110000,
  B00011111, B11111000,
  B00111111, B11111100,
  B01111111, B11111110,
  B01111111, B11111110,
  B00111111, B11111100,
  B00011111, B11111000,
  B00001111, B11110000,
  B00000111, B11100000,
  B00000011, B11000000
};

void syncTime() {
  Blynk.sendInternal("rtc", "sync"); 
}

BLYNK_WRITE(InternalPinRTC) {
  long blynkTime = param.asLong();
  setTime(blynkTime);
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  mpu.initialize();
  tft.init();
  tft.setRotation(1);
  
  WiFi.begin(ssid, pass);
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  showStartupScreen(); 

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(VIBRATION_MOTOR_PIN, OUTPUT);

  resetMainScreen();

  timer.setInterval(10000L, syncTime); 
}

void loop() {
  Blynk.run();
  timer.run();

  if (!alarmActive) { 
    static uint8_t lastRate = 0;
    uint8_t rateValue;

    heartrate.getValue(HEARTRATE_PIN);
    rateValue = heartrate.getRate();

    if (rateValue) {
      pulseDetected = true;
      Serial.println(rateValue);

     if (rateValue != lastRate) {
    tft.setTextSize(2);
    tft.setTextColor(TFT_RED, TFT_BLACK);


    tft.fillRect(70, 20, tft.textWidth("BPM: 000"), 20, TFT_BLACK);
    
    tft.setCursor(70, 20);
    tft.print("BPM: ");
    tft.printf("%3d", rateValue); 
    lastRate = rateValue;

    Blynk.virtualWrite(V0, rateValue);
}
    } else {
      pulseDetected = false;
    }

    if (pulseDetected) {
      drawECGWave();
    }

    detectFall();
    displayTime();  
  }

uint16_t touchX, touchY;
if (alarmActive && tft.getTouch(&touchX, &touchY)) {
  stopAlarm(); 

  delay(50);
  }
}

void resetMainScreen() {
  tft.fillScreen(TFT_BLACK);
  drawHeartSymbol(40, 20);
  tft.setCursor(70, 20);
  tft.setTextSize(2);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.print("BPM: --");
  alarmActive = false;
}

void showStartupScreen() {
  tft.fillScreen(TFT_BLACK);
  drawHeartSymbol((tft.width() - 16) / 2, 40);

  tft.setTextSize(3);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  int titleX = (tft.width() - tft.textWidth("AetherPulse")) / 2;
  tft.setCursor(titleX, 80);
  tft.print("AetherPulse");

  tft.setTextSize(2);
  int versionX = (tft.width() - tft.textWidth("4.9")) / 2;
  tft.setCursor(versionX, 120);
  tft.print("6.0");

  delay(2000);
}

void drawECGWave() {
  int baseline = 120;
  int amplitude = 30;
  int waveSpeed = 6;

  int newY;
  int phase = ecgX % 50;

  if (phase < 10) newY = baseline - amplitude;      
  else if (phase < 20) newY = baseline + amplitude / 2;
  else if (phase < 30) newY = baseline - amplitude / 3;
  else newY = baseline;

  tft.drawLine(ecgX, prevY, ecgX + waveSpeed, newY, TFT_GREEN);
  prevY = newY;
  ecgX += waveSpeed;

  if (ecgX > tft.width()) {
    ecgX = 20;
    tft.fillRect(20, 90, tft.width() - 20, 60, TFT_BLACK);
  }
}

void detectFall() {
  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);

  float acceleration = sqrt(ax * ax + ay * ay + az * az) / 16384.0; 

  static bool freeFallDetected = false;
  static unsigned long fallStartTime = 0;

  if (acceleration < 0.5 && !freeFallDetected) {
    freeFallDetected = true;
    fallStartTime = millis();
  }

  if (freeFallDetected) {
    if (acceleration > 2.0) {
      fallDetected = true;
      activateAlert();
      freeFallDetected = false; 

      Blynk.virtualWrite(V1, 1);
      Blynk.logEvent("fall_detected", "⚠️ Fall detected! Check immediately!");
    }
  }

  if (freeFallDetected && millis() - fallStartTime > 1000) {
    freeFallDetected = false;
  }
}

void activateAlert() {
  tft.fillScreen(TFT_RED);
  tft.setTextSize(4);
  tft.setTextColor(TFT_WHITE, TFT_RED);
  
  int textX = (tft.width() - tft.textWidth("FALL DETECTED!")) / 2;
  tft.setCursor(textX, 80);
  tft.print("FALL DETECTED!");

  int btnX = (tft.width() - 100) / 2;
  int btnY = 160;
  tft.fillRoundRect(btnX, btnY, 100, 40, 10, TFT_WHITE);
  tft.setTextSize(3);
  tft.setTextColor(TFT_BLACK);
  int textStopX = btnX + (100 - tft.textWidth("STOP")) / 2;
  tft.setCursor(textStopX, btnY + 10);
  tft.print("STOP");

  digitalWrite(BUZZER_PIN, HIGH);
  digitalWrite(VIBRATION_MOTOR_PIN, HIGH);
  alarmActive = true;
}


void stopAlarm() {
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(VIBRATION_MOTOR_PIN, LOW);
  resetMainScreen();
  
  Blynk.virtualWrite(V1, 0);
}

void drawHeartSymbol(int x, int y) {
  tft.drawBitmap(x, y, heartSymbol, 16, 11, TFT_RED);
}

void displayTime() {
  static int lastHour = -1, lastMinute = -1, lastSecond = -1;
  if (alarmActive) return;

  int h = hour(), m = minute(), s = second();
  if (h != lastHour || m != lastMinute || s != lastSecond) {
    tft.setTextSize(4);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    int timeX = (tft.width() - tft.textWidth("00:00:00")) / 2;
    tft.fillRect(timeX, 160, tft.textWidth("00:00:00"), 35, TFT_BLACK);
    tft.setCursor(timeX, 160);
    tft.printf("%02d:%02d:%02d", h, m, s);
    
    lastHour = h;
    lastMinute = m;
    lastSecond = s;
  }
}