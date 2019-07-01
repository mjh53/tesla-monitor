
#include <ArduinoJson.h>


#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// SCL GPIO5
// SDA GPIO4
#define OLED_RESET 0  // GPIO0
Adafruit_SSD1306 display(OLED_RESET);

//Pin connected to ST_CP of 74HC595 - pin 12
int latchPin = D0; // 8
//Pin connected to SH_CP of 74HC595 - pin 11
int clockPin = D5; // 12
////Pin connected to DS of 74HC595  - pin 14
int dataPin = D6; // 11

#ifndef STASSID
#define STASSID "SSID"
#define STAPSK  "password"
#endif

const char* ssid = STASSID;
const char* password = STAPSK;

const char* host = "192.168.*.*";
const int httpsPort = 443;

extern "C" {
#include "user_interface.h"
}

os_timer_t myTimer;


// Use web browser to view and copy
// SHA1 fingerprint of the certificate
const char fingerprint[] PROGMEM = "9a a6 da e5 b9 c0 3c 02 d5 aa 31 68 66 74 f3 39 f7 6b 91 08";

int power;
int displayBits=B1010101;
int mask=1;
int frequency=400;
int called=0;
// start of timerCallback
void timerCallback(void *pArg) {
  called=1;
  os_intr_lock(); 
  // cycle up

  power-=20;
  
  if (power > 0) {
      mask = mask << 1;
      mask += 1;
      if (mask > 1023) { mask=0; } 
  }
  if (power < 0) {
      if (mask < 1) { mask=displayBits; } else { mask = mask >> 1; }
  }

  bar_display(displayBits & mask);
  os_intr_unlock();
} // End of timerCallback

void setup() {
  Serial.begin(115200);

  os_timer_setfn(&myTimer, timerCallback, NULL);
  os_timer_arm(&myTimer, frequency, true);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 64x48)
  // Clear the buffer.
  display.clearDisplay();

  // text display tests
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("Connecting\nPowerwall");
  display.display();

  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, OUTPUT);
  
  Serial.println();
  Serial.print("connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  
}

void bar_display(int numberToDisplay) {
  int blank=2048;
  digitalWrite(latchPin, LOW);
  // shift out the bits:
  shiftOut(dataPin, clockPin, LSBFIRST, blank);
  shiftOut(dataPin, clockPin, LSBFIRST, numberToDisplay);  

  //take the latch pin high so the LEDs will light up:
  digitalWrite(latchPin, HIGH);
 
}

void loop() {
  
  // Use WiFiClientSecure class to create TLS connection
  WiFiClientSecure client;
  Serial.print("connecting to ");
  Serial.println(host);

  Serial.printf("Using fingerprint '%s'\n", fingerprint);
  client.setFingerprint(fingerprint);

  if (!client.connect(host, httpsPort)) {
    Serial.println("connection failed");
    return;
  }

  String url = "/api/system_status/soe";
  Serial.print("requesting URL: ");
  Serial.println(url);

  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: TeslaMon\r\n" +
               "Connection: close\r\n\r\n");

  Serial.println("request sent");
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      Serial.println("headers received");
      break;
    }
  }
  String line = client.readStringUntil('\n');
  if (line.startsWith("{\"percentage\":")) {
    Serial.println("Powerwall charge read successfull!");
  } else {
    Serial.println("Charge read has failed");
  }
  String percentstr=line.substring(14,19);
  Serial.println(percentstr);
  int percent=percentstr.substring(0,2).toInt();
  Serial.println(percent);
  Serial.println("reply was:");
  Serial.println("==========");
  Serial.println(line);
  Serial.println("==========");
  Serial.println("closing connection");
  client.stop();

  Serial.print("connecting to ");
  Serial.println(host);

  WiFiClientSecure client2;

  Serial.printf("Using fingerprint '%s'\n", fingerprint);
  client2.setFingerprint(fingerprint);

  if (!client2.connect(host, httpsPort)) {
    Serial.println("connection failed");
    return;
  }
  url = "/api/meters/aggregates";
  Serial.print("requesting URL: ");
  Serial.println(url);

  client2.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: TeslaMon\r\n" +
               "Connection: close\r\n\r\n");

  Serial.println("request 2 sent");
  while (client2.connected()) {
    String line2 = client2.readStringUntil('\n');
    if (line2 == "\r") {
      Serial.println("headers received");
      break;
    }
  }
  String line2 = client2.readStringUntil('\n'); // discard random hex value??
  line2 = client2.readStringUntil('\n');

  Serial.println("reply was:");
  Serial.println("==========");
  Serial.println(line2);
  Serial.println("==========");
  Serial.println("closing connection");
  client2.stop();
  
  DynamicJsonDocument doc(4000);

  DeserializationError error = deserializeJson(doc, line2);
  Serial.println(line2);
  // Test if parsing succeeds.
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    // return
  }

  String powerstr=doc["battery"]["instant_power"];
  int power=powerstr.toInt();
  Serial.println("power read:");
  Serial.println(powerstr);


  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(2);
  display.println("Tesla");
  display.setTextSize(1);
  if (power < 0) {
    display.println("charging");
  } else {
    if (power == 0) {
      display.println("idle");
    } else {
    display.println("powering");
    }
  }
  display.println("");
  display.setTextSize(2);
  display.println(percentstr);
  display.display();



  int numberToDisplay=0; //B10000001;
  int divisor=10;
  while (percent > divisor) {
    numberToDisplay = numberToDisplay << 1;
    numberToDisplay +=1;
    percent -= divisor;
  }

  int blank=2048;
  displayBits=numberToDisplay;

  yield();
 
  
  delay(10000);
}
