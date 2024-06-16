/* <project_fallguard> */

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include <UrlEncode.h>
#include <Wire.h>
#include <SoftwareSerial.h>
#include <TinyGPS++.h>

#define BUZZER_PIN D7
#define LED_PIN D4
#define BUTTON_PIN D3
#define RX_PIN D6
#define TX_PIN D5


// Enter your personal WiFi SSID and Password here
const char *ssid = ".......";
const char *passwd = ".......";

// Enter your phone-number and the API-Key you get after setup here
// Info about the API-Setup on callmebot.com/blog/free-api-whatsapp-messages/
String phoneNumber = ".......";
String apiKey = ".......";

TinyGPSPlus gps;
SoftwareSerial gpsSerial(TX_PIN, RX_PIN);
String latitude, longitude, link;
boolean shareLocation = true; // Set this to false if you dont want your GPS information to be transmitted

const int MPU_addr = 0x68;
int16_t AcX, AcY, AcZ, Tmp, GyX, GyY, GyZ;
float ax = 0, ay = 0, az = 0, gx = 0, gy = 0, gz = 0;
boolean trigger1 = false, trigger2 = false, trigger3 = false;
byte trigger1count = 0, trigger2count = 0, trigger3count = 0;
int angleChange = 0;

boolean fall = false;

void setup() {

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // turns the LED on, this gets turned off after it connects to WiFi

  Serial.begin(9600);
  gpsSerial.begin(9600);

  Wire.begin();
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);

  // Establish WiFi connection
  WiFi.begin(ssid, passwd);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
  }
}

void loop() {

  while (WiFi.status() == WL_CONNECTED) { // only starts when it's connected to WiFi
    digitalWrite(LED_PIN, LOW);

    fetchGPSInfo();

    checkFalling();
    if (fall || !digitalRead(BUTTON_PIN)) { // trigger alarm if fall is detected OR button is pressed manually

      delay(500);

      boolean abort = false;
      for (int i = 0; i < 10; i++) {
        // stop alarm and whatsapp message, when button is pressed again 
        if (!digitalRead(BUTTON_PIN)) {
          abort = true;
          break;
        }
        // Alarm signal via LED and buzzer 
        digitalWrite(BUZZER_PIN, HIGH);
        digitalWrite(LED_PIN, HIGH);
        delay(1000);
        digitalWrite(BUZZER_PIN, LOW);
        digitalWrite(LED_PIN, LOW);
        delay(1000);
      }

      if (!abort) {
        fetchGPSInfo();
        
        // send Whatsapp containing SOS message and google maps link with GPS information (or "No Location available.")
        sendWhatsAppMessage("!FALL DETECTION!\n" + (shareLocation ? link : ""));

        // to disable the sharing of GPS information, delete the previous 4 lines and uncomment the following
        // sendWhatsAppMessage("!FALL DETECTION!");
      } else {
        // signal that alarm was aborted
        for (int i = 0; i < 5; i++) {
          digitalWrite(BUZZER_PIN, HIGH);
          digitalWrite(LED_PIN, HIGH);
          delay(200);
          digitalWrite(BUZZER_PIN, LOW);
          digitalWrite(LED_PIN, LOW);
          delay(200);
        }
      } 

      fall = false; // reset falling
    }

    delay(100);
  }
  delay(1000);
  digitalWrite(LED_PIN, HIGH); // LED is on when not connected to WiFi
}


// Fetch current coordinates with GPS sensor, in case GPS is available
void fetchGPSInfo() {
 while (gpsSerial.available() > 0) {
    if (gps.encode(gpsSerial.read())) {
      if (gps.location.isValid()) {
        latitude = String(gps.location.lat(), 6);
        longitude = String(gps.location.lng(), 6);
        link = "http://maps.google.com/maps?&z=15&mrt=yp&t=k&q=" + latitude + "+" + longitude;
      }
      else {
        link = "Location is not available.";
      }
    }
  }
}

// Sends whatsapp message to specified phone number by performing a post request to the callmebot api
// (if you want to use a different messenger, go to callmebot.com to find further information
void sendWhatsAppMessage(String message) {

  String url = "http://api.callmebot.com/whatsapp.php?phone=" + phoneNumber + "&apikey=" + apiKey + "&text=" + urlEncode(message);

  WiFiClient client;
  HTTPClient http;

  http.begin(client, url);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.POST(url);

  http.end();
}

// Sets fall=true, if a fall is detected (tinker with the values, in case more/less sensibility is required)
void checkFalling() {
  mpu_read(); // read in Accelerometer and Gyroscope Sensor data
  ax = (AcX - 2050) / 16384.00;
  ay = (AcY - 77) / 16384.00;
  az = (AcZ - 1947) / 16384.00;
  gx = (GyX + 270) / 131.07;
  gy = (GyY - 351) / 131.07;
  gz = (GyZ + 136) / 131.07;

  float raw_amplitude = pow(pow(ax, 2) + pow(ay, 2) + pow(az, 2), 0.5);
  int amplitude = raw_amplitude * 10;

  if (amplitude <= 2 && trigger2 == false) {
    trigger1 = true; // set trigger1
  }

  if (trigger1) {
    trigger1count++;
    if (amplitude >= 5) {
      trigger2 = true; // set trigger2
      trigger1 = false;
      trigger1count = 0;
    }
  }

  if (trigger2) {
    trigger2count++;
    angleChange = pow(pow(gx, 2) + pow(gy, 2) + pow(gz, 2), 0.5);
    if (angleChange >= 30 && angleChange <= 400) {
      trigger3 = true; // set trigger3
      trigger2 = false;
      trigger2count = 0;
    }
  }

  if (trigger3) {
    trigger3count++;
    if (trigger3count >= 5) {
      fall = true; // set falling
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

// get the readings of the accelerometer
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