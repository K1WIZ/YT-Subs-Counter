/*
  ESP-01 pinout:

  GPIO 2 - DataIn
  GPIO 1 - LOAD/CS
  GPIO 0 - CLK

  ------------------------
  NodeMCU 1.0 pinout:

  D8 - DataIn
  D7 - LOAD/CS
  D6 - CLK
*/


#include "Arduino.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

#define NUM_MAX 4
#define ROTATE 90

// for ESP-01 module
//#define DIN_PIN 2 // D4
//#define CS_PIN  3 // D9/RX
//#define CLK_PIN 0 // D3

// for NodeMCU 1.0
#define DIN_PIN 15  // D8
#define CS_PIN  13  // D7
#define CLK_PIN 12  // D6

#include "max7219.h"
#include "fonts.h"

// =======================================================================
// Your config below!
// =======================================================================
const char* ssid     = "";               // SSID of local network
const char* password = "";             // Password on network
String ytApiV3Key    = "";                // YouTube Data API v3 key generated here: https://console.developers.google.com
String channelId     = "";   // YT channel id
// =======================================================================

void setup() 
{
  Serial.begin(115200);
  initMAX7219();
  sendCmdAll(CMD_SHUTDOWN,1);
  sendCmdAll(CMD_INTENSITY, 15); // Set brightness to maximum (15)
  Serial.print("Connecting WiFi ");
  WiFi.begin(ssid, password);
  printStringWithShift(" WiFi ...~",15,font,' ');
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("."); delay(500);
  }
  Serial.println("");
  Serial.print("Connected: "); Serial.println(WiFi.localIP());
  Serial.println("Getting data ...");
  printStringWithShift(" YT ...~",15,font,' ');
}

// =======================================================================

long viewCount, viewCount24h=-1, viewsGain24h;
long subscriberCount, subscriberCount1h=-1, subscriberCount24h=-1, subsGain1h=0, subsGain24h=0;
long videoCount;
int cnt = 0;
unsigned long time1h, time24h;
long localEpoc = 0;
long localMillisAtUpdate = 0;
int h, m, s;
String date;
int scrollCounter = 0;

void loop()
{
  if (scrollCounter == 0) {
    if (getYTData() == 0) {
      scrollCounter = 1; // Set scroll counter to 1 to indicate an initial API call
    }
  }
  else {
    if (scrollCounter >= 50) {
      if (getYTData() == 0) {
        scrollCounter = 1; // Reset the scroll counter
      }
    }
    scrollCounter++; // Increment the scroll counter
  }

  if (cnt <= 0) {
    if (subscriberCount1h < 0) {
      time1h = time24h = millis();
      subscriberCount1h = subscriberCount24h = subscriberCount;
      viewCount24h = viewCount;
    }
    if (millis() - time1h > 1000 * 60 * 60) {
      time1h = millis();
      subscriberCount1h = subscriberCount;
    }
    if (millis() - time24h > 1000 * 60 * 60 * 24) {
      time24h = millis();
      subscriberCount24h = subscriberCount;
      viewCount24h = viewCount;
    }
    subsGain1h = subscriberCount - subscriberCount1h;
    subsGain24h = subscriberCount - subscriberCount24h;
    viewsGain24h = viewCount - viewCount24h;
  }
  cnt--;

  int del = 5000;
  int scrollDel = 20;

  printStringWithShift("  YouTube Subscribers: ", scrollDel, font, ' ');
  printValueWithShift(subscriberCount, scrollDel, 0);
  delay(del);
  if (subsGain1h) {
    printStringWithShift("  Subscribers gain 1h: ", scrollDel, font, ' ');
    printValueWithShift(subsGain1h, scrollDel, 1);
    delay(del);
  }
  if (subsGain24h) {
    printStringWithShift("  Subscribers gain 24h: ", scrollDel, font, ' ');
    printValueWithShift(subsGain24h, scrollDel, 1);
    delay(del);
  }
  printStringWithShift("  Views: ", scrollDel, font, ' ');
  printValueWithShift(viewCount, scrollDel, 0);
  delay(del);
  if (viewsGain24h) {
    printStringWithShift("  Subscribers gain 24h: ", scrollDel, font, ' ');
    printValueWithShift(subsGain24h, scrollDel, 1);
    delay(del);
  }
  printStringWithShift("  Videos: ", scrollDel, font, ' ');
  printValueWithShift(videoCount, scrollDel, 0);
  delay(del);
}


// =======================================================================

int dualChar = 0;

// =======================================================================

int charWidth(char ch, const uint8_t *data)
{
  int len = pgm_read_byte(data);
  return pgm_read_byte(data + 1 + ch * len);
}

// =======================================================================

int showChar(char ch, const uint8_t *data)
{
  int len = pgm_read_byte(data);
  int i,w = pgm_read_byte(data + 1 + ch * len);
  scr[NUM_MAX*8] = 0;
  for (i = 0; i < w; i++)
    scr[NUM_MAX*8+i+1] = pgm_read_byte(data + 1 + ch * len + 1 + i);
  return w;
}

// =======================================================================

void printCharWithShift(unsigned char c, int shiftDelay, const uint8_t *data, int offs) 
{
  if(c < offs || c > MAX_CHAR) return;
  c -= offs;
  int w = showChar(c, data);
  for (int i=0; i<w+1; i++) {
    delay(shiftDelay);
    scrollLeft();
    refreshAll();
  }
}

// =======================================================================

void printStringWithShift(const char *s, int shiftDelay, const uint8_t *data, int offs)
{
  while(*s) printCharWithShift(*s++, shiftDelay, data, offs);
}

// =======================================================================
// printValueWithShift():
// converts int to string
// centers string on the display
// chooses proper font for string/number length
// can display sign - or +
void printValueWithShift(long val, int shiftDelay, int sign)
{
  const uint8_t *digits = digits5x7;       // good for max 5 digits
  if(val>1999999) digits = digits3x7;      // good for max 8 digits
  else if(val>99999) digits = digits4x7;   // good for max 6-7 digits
  String str = String(val);
  if(sign) {
    if(val<0) str=";"+str; else str="<"+str;
  }
  const char *s = str.c_str();
  int wd = 0;
  while(*s) wd += 1+charWidth(*s++ - '0', digits);
  wd--;
  int wdL = (NUM_MAX*8 - wd)/2;
  int wdR = NUM_MAX*8 - wdL - wd;
  //Serial.println(wd); Serial.println(wdL); Serial.println(wdR);
  s = str.c_str();
  while(wdL>0) { printCharWithShift(':', shiftDelay, digits, '0'); wdL--; }
  while(*s) printCharWithShift(*s++, shiftDelay, digits, '0');
  while(wdR>0) { printCharWithShift(':', shiftDelay, digits, '0'); wdR--; }
}

// =======================================================================

const char *ytHost = "www.googleapis.com";

int getYTData()
{
  WiFiClientSecure client;
  Serial.print("Connecting to "); Serial.println(ytHost);
  client.setInsecure();

  if (!client.connect(ytHost, 443)) {
    Serial.println("Connection failed");
    return -1;
  }

  String cmd = String("GET /youtube/v3/channels?part=statistics&id=") + channelId + "&key=" + ytApiV3Key + " HTTP/1.1\r\n" +
               "Host: " + ytHost + "\r\nUser-Agent: ESP8266/1.1\r\nConnection: close\r\n\r\n";
  Serial.println(cmd);
  client.print(cmd);

  int repeatCounter = 10;
  while (!client.available() && repeatCounter--) {
    Serial.println("Waiting for response..."); delay(500);
  }

  // Skip the response headers
  while (client.available() && client.read() != '\r') {}

  // Find the starting position of the JSON sentence
  String jsonStartMarker = "{";
  String response = client.readString();
  int jsonStartIndex = response.indexOf(jsonStartMarker);
  if (jsonStartIndex == -1) {
    Serial.println("Invalid JSON format: JSON sentence not found");
    printStringWithShift("JSON error!", 30, font, ' ');
    delay(10);
    return -1;
  }

  // Extract the JSON sentence
  String jsonSentence = response.substring(jsonStartIndex);
  Serial.println("JSON response:");
  Serial.println(jsonSentence);

  client.stop();

  DynamicJsonDocument jsonDoc(4096);
  DeserializationError error = deserializeJson(jsonDoc, jsonSentence);
  if (error) {
    Serial.print("deserializeJson() failed with code ");
    Serial.println(error.code());
    Serial.println(error.c_str());
    printStringWithShift("JSON error!", 30, font, ' ');
    delay(10);
    return -1;
  }

  const JsonObject& root = jsonDoc.as<JsonObject>();
  if (!root.containsKey("items") || root["items"].isNull() || !root["items"].is<JsonArray>() || root["items"].size() == 0) {
    Serial.println("Invalid JSON format: 'items' array not found or empty");
    printStringWithShift("JSON error!", 30, font, ' ');
    delay(10);
    return -1;
  }

  const JsonObject& channel = root["items"][0].as<JsonObject>();
  if (!channel.containsKey("statistics")) {
    Serial.println("Invalid JSON format: 'statistics' field not found");
    printStringWithShift("JSON error!", 30, font, ' ');
    delay(10);
    return -1;
  }

  const JsonObject& statistics = channel["statistics"].as<JsonObject>();
  if (!statistics.containsKey("viewCount") || !statistics.containsKey("subscriberCount") || !statistics.containsKey("videoCount")) {
    Serial.println("Invalid JSON format: statistics fields not found");
    printStringWithShift("JSON error!", 30, font, ' ');
    delay(10);
    return -1;
  }

  viewCount = statistics["viewCount"].as<long>();
  subscriberCount = statistics["subscriberCount"].as<long>();
  videoCount = statistics["videoCount"].as<long>();

  Serial.print("Subscriber Count: ");
  Serial.println(subscriberCount);
  Serial.print("View Count: ");
  Serial.println(viewCount);
  Serial.print("Video Count: ");
  Serial.println(videoCount);

  return 0;
}



// =======================================================================
