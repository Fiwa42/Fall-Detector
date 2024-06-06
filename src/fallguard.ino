/*
* Fallguard
*/

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include <UrlEncode.h>
#include <Wire.h>
#include <SoftwareSerial.h>
#include <TinyGPS++.h>

#define LED_PIN D7
#define BUTTON_PIN D3
#define RX_PIN D6
#define TX_PIN D5

const char *ssid = "***";
const char *passwd = "***";

String phoneNumber = "***";
String apiKey = "***";

TinyGPSPlus gps;
SoftwareSerial SerialGPS(RX_PIN, TX_PIN);
String latitude, longitude;

const int MPU_addr = 0x68;
int16_t AcX, AcY, AcZ, Tmp, GyX, GyY, GyZ;
float ax = 0, ay = 0, az = 0, gx = 0, gy = 0, gz = 0;
boolean trigger1 = false, trigger2 = false, trigger3 = false;
byte trigger1count = 0, trigger2count = 0, trigger3count = 0;
int angleChange = 0;

boolean fall = false;

void setup() {

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  SerialGPS.begin(9600);

  Wire.begin();
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);

  WiFi.begin(ssid, passwd);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
  }
}

void loop() {

  checkFalling();
  if (fall || !digitalRead(BUTTON_PIN)) {

    delay(500);

    boolean abort = false;
    for (int i = 0; i < 10; i++) {
      if (!digitalRead(BUTTON_PIN)) {
        abort = true;
        break;
      }
      digitalWrite(LED_PIN, HIGH);
      delay(1000);
      digitalWrite(LED_PIN, LOW);
      delay(1000);
    }

    if (!abort) {
      fetchGPSInfo();
      String link = "http://maps.google.com/maps?&z=15&mrt=yp&t=k&q=" + latitude + "+" + longitude;

      sendWhatsAppMessage("!FALL DETECTION!\n" + link);
    } else {
      for (int i = 0; i < 5; i++) {
        digitalWrite(LED_PIN, HIGH);
        delay(200);
        digitalWrite(LED_PIN, LOW);
        delay(200);
      }
    }

    fall = false;
  }

  delay(100);
}

void fetchGPSInfo() {
  if (SerialGPS.available() > 0) {
    if (gps.encode(SerialGPS.read())) {
      if (gps.location.isValid()) {
        latitude = String(gps.location.lat(), 6);
        longitude = String(gps.location.lng(), 6);
      }
    }
  }
}

void sendWhatsAppMessage(String message) {

  String url = "http://api.callmebot.com/whatsapp.php?phone=" + phoneNumber + "&apikey=" + apiKey + "&text=" + urlEncode(message);

  WiFiClient client;
  HTTPClient http;

  http.begin(client, url);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.POST(url);

  http.end();
}

void checkFalling() {
  mpu_read();
  ax = (AcX - 2050) / 16384.00;
  ay = (AcY - 77) / 16384.00;
  az = (AcZ - 1947) / 16384.00;
  gx = (GyX + 270) / 131.07;
  gy = (GyY - 351) / 131.07;
  gz = (GyZ + 136) / 131.07;

  float raw_amplitude = pow(pow(ax, 2) + pow(ay, 2) + pow(az, 2), 0.5);
  int amplitude = raw_amplitude * 10;

  if (amplitude <= 2 && trigger2 == false) {
    trigger1 = true;
  }

  if (trigger1) {
    trigger1count++;
    if (amplitude >= 5) {
      trigger2 = true;
      trigger1 = false;
      trigger1count = 0;
    }
  }

  if (trigger2) {
    trigger2count++;
    angleChange = pow(pow(gx, 2) + pow(gy, 2) + pow(gz, 2), 0.5);
    if (angleChange >= 30 && angleChange <= 400) {
      trigger3 = true;
      trigger2 = false;
      trigger2count = 0;
    }
  }

  if (trigger3) {
    trigger3count++;
    if (trigger3count >= 5) {
      fall = true;
      trigger3 = false;
      trigger3count = 0;
    }
  }

  if (trigger2count >= 6) {
    trigger2 = false;
    trigger2count = 0;
  }
  if (trigger1count >= 6) {
    trigger1 = false;
    trigger1count = 0;
  }
}

void mpu_read() {

  Wire.beginTransmission(MPU_addr);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_addr, 14, true);
  AcX = Wire.read() << 8 | Wire.read();
  AcY = Wire.read() << 8 | Wire.read();
  AcZ = Wire.read() << 8 | Wire.read();
  Tmp = Wire.read() << 8 | Wire.read();
  GyX = Wire.read() << 8 | Wire.read();
  GyY = Wire.read() << 8 | Wire.read();
  GyZ = Wire.read() << 8 | Wire.read();
}