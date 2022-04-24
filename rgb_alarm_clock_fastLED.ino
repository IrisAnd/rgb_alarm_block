#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include <EEPROM.h>
#include <WiFiManager.h>
#include <NTPClient.h>
#include <EEPROM.h>

#include "names.h"
#include "web_admin.h"
#include "web_interface.h"
#include "web_iro_js.h"
#include "go_back.h"

IPAddress ip(192, 168, 2, 208);
IPAddress gateway(192, 168, 2, 1);
IPAddress subnet(255, 255, 255, 0);

#define BUILTIN_LED 2 // internal ESP-12 LED on GPIO2

//Settings for capacity switch
#define AIN A0
int inputVal=0;

// Which pin on the Arduino is connected to the LEDs?
// On a Trinket or Gemma we suggest changing this to 1:
#define LED_PIN 6

// How many LEDs are attached to the Arduino?
#define LED_COUNT 23

// NeoPixel brightness, 0 (min) to 255 (max)
#define BRIGHTNESS 255  //(max = 255)

RGB_h current_color = {255,255,255};
int current_brightness = BRIGHTNESS;
bool stripOff = false;
RGB_h alarm_color = {255,255,255};

// Declare our FastLED strip object:
CRGB leds[LED_COUNT];

// Argument 1 = Number of pixels in NeoPixel strip
// Argument 2 = Arduino pin number (most are valid)
// Argument 3 = Pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
//   NEO_RGBW    Pixels are wired for RGBW bitstream (NeoPixel RGBW products)

ESP8266WebServer server(80);

//Alarm settings
int alarmMinutes;
int alarmHours;
bool alarmSet = false;
double wakeUpTime = 900.0; //in seconds

//Time settings
int seconds0;
int seconds1;
const long utcOffsetInSeconds = 3600;

//EEPROM Storage Settings
int alarm_hour_mem = 17;
int alarm_min_mem = 23;
int alarm_set_mem = 0;
int alarm_r_val_mem = 56;
int alarm_g_val_mem = 86;
int alarm_b_val_mem = 74;

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

void setup(void) {
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, LED_COUNT);
  
  Serial.begin(115200);
  // WiFiManager
  // Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  
  //wifiManager.setSTAStaticIPConfig(ip, gateway, subnet); // optional DNS 4th argument
  wifiManager.autoConnect("RGB_Lamp_AP");
  Serial.println("Connected.");
  
  timeClient.begin();
  server.begin();
  // Root and 404
  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);

  // iro.js User Interface and Javascript
  server.on("/ui", HTTP_GET, []() {
    server.send_P(200, "text/html", WEBINTERFACE);
  });
  server.on("/admin", HTTP_GET, []() {
    server.send_P(200, "text/html", WEBADMIN);
  });
  server.on("/iro.min.js", HTTP_GET, []() {
    server.send_P(200, "application/javascript", IRO_JS);
  });

  // REST-API
  server.on("/api/v1/state", HTTP_POST, handleApiRequest);
  
  server.on("/reset", HTTP_POST, resetAlarm);
  server.on("/alarm-color", HTTP_POST, setAlarmColor);
  server.on("/time", HTTP_POST, handleTime); //form action is handled here
 

  server.begin();
  Serial.println("WifiRGB HTTP server started");

  //Read alarm settings from memory
  EEPROM.begin(512);
  Serial.println("Reading Memory");
  alarmHours = EEPROM.read(alarm_hour_mem);
  alarmMinutes = EEPROM.read(alarm_min_mem);
  alarmSet = EEPROM.read(alarm_set_mem);
  Serial.println(alarmHours);
  Serial.println(alarmMinutes);
  Serial.println(alarmSet);
  int alarmColorR =  EEPROM.read(alarm_r_val_mem);
  int alarmColorG =  EEPROM.read(alarm_g_val_mem);
  int alarmColorB =  EEPROM.read(alarm_b_val_mem);
  alarm_color = {alarmColorR,   alarmColorG,   alarmColorB};

  
  //Initialize LED strip
  FastLED.setBrightness(BRIGHTNESS);
  for (int i = 0; i <= LED_COUNT; i++) {
    leds[i] = CRGB( 255, 255, 255);
    FastLED.show();
    delay(40);
  }

}

void loop(void) {
 server.handleClient();
   // check time every 30 seconds
  if ((millis() - seconds0) > 30000) {
    checkAlarm();
    seconds0 = millis();
    }
  else if ((millis() - seconds1) > 1000) {
    //Check if switch was touched and turn off LEDs
    inputVal=analogRead(AIN);
    
    if(inputVal>=20)
    {if (stripOff == true){
        Serial.println("Turning on LEDs");
        FastLED.setBrightness(current_brightness);
        for (int i = 0; i <= LED_COUNT; i++) {
          leds[i] = CRGB(current_color.r,  current_color.g, current_color.b);
          FastLED.show();
        }
        stripOff = false;
      }
      else{
        Serial.println("Turning off LEDs");
        FastLED.clear();
        FastLED.show();
        stripOff = true;
      }
    }
  
    seconds1 = millis();
    }
}

//Callback for WifiManager
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());

  Serial.println(myWiFiManager->getConfigPortalSSID());
}

void handleRoot() {
  server.send(200, "text/plain", "hello from esp8266 wifi rgb!");
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void resetAlarm() {
  alarmSet = false;
  EEPROM.write(alarm_set_mem, alarmSet);
  EEPROM.commit();
  Serial.print("Reset");
  ///String s = "<a href='/ui'> Alarm was reset. Go Back </a>";
  server.send(200, "text/html", GO_BACK); //Send web page
}

void setAlarmColor(){
  alarm_color = current_color;
  EEPROM.write(alarm_r_val_mem, alarm_color.r);
  EEPROM.write(alarm_g_val_mem, alarm_color.g);
  EEPROM.write(alarm_b_val_mem, alarm_color.b);
  EEPROM.commit();
  Serial.println("Alarm Color Set");
  ///String s = "<a href='/ui'> Alarm was reset. Go Back </a>";
  server.send(200, "text/html", GO_BACK); //Send web page
}

void handleTime() {
  //int alarmMinutes;
  //int alarmHours;
  alarmSet = true;
  Serial.println(server.arg("alarm"));
  String input = server.arg("alarm");
  for (int i = 0; i < input.length(); i++) {
  if (input.substring(i, i+1) == ":") {
    alarmHours = input.substring(0, i).toInt();
    alarmMinutes = input.substring(i+1).toInt();
    break;
  }
}
  Serial.println(alarmHours);
  Serial.println(alarmMinutes);
  EEPROM.write(alarm_hour_mem, alarmHours);
  EEPROM.write(alarm_min_mem, alarmMinutes);
  EEPROM.write(alarm_set_mem, alarmSet);
  EEPROM.commit();
  //String s = "<a href='/ui'> Alarm was set to . Go Back </a>";
  server.send(200, "text/html", GO_BACK); //Send web page
}

void checkAlarm(){
  timeClient.update();
   // trigger alarm clock
  if ((timeClient.getHours() == alarmHours) && (timeClient.getMinutes() == alarmMinutes) && (alarmSet == true)) {
      Serial.println("ALARM");
      growLight();
  }
}

void growLight(){
  
  double step = 255.0/wakeUpTime;
  double brightness = 0;
  for (int i =0; i<int(wakeUpTime); i++){
    brightness = brightness + step;
    Serial.print("Brightness: ");
    Serial.println(brightness);
    FastLED.setBrightness(int(brightness));
    for (int i = 0; i <= LED_COUNT; i++) {
      leds[i] = CRGB(alarm_color.r,  alarm_color.g, alarm_color.b);
      FastLED.show();
      delay(40);
    }
    delay(1000);
  }  
}

void handleApiRequest() {

  Serial.println("### API Request:");
  if (server.hasArg("plain") == false) { //Check if body received
    server.send(200, "text/plain", "Body not received");
    return;
  }

  const size_t bufferSize = JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(4) + 70;
  DynamicJsonBuffer jsonBuffer(bufferSize);
  JsonObject& root = jsonBuffer.parseObject(server.arg("plain"));

  Serial.println("JSON Body: ");
  root.printTo(Serial);
  Serial.println();

  const char* state = root["state"]; // "ON" or "OFF"
  if (strcmp("OFF", state) == 0) {
    Serial.println("State OFF found: switching off");
    // Set Output
    FastLED.clear();
    FastLED.show();
    server.send(200, "application/json", server.arg("plain"));
    return;
  }

  int brightness = root["brightness"];
  Serial.print("Brightness: ");
  Serial.println(brightness);

  RGB_h rgb = {0, 0, 0};
  JsonObject& color = root["color"];

  // DEBUG: color
  Serial.print("Color: ");
  color.printTo(Serial);
  Serial.println();

  // If RGB mode: Parse RGB values
  if (color["mode"] == "rgb") {
    // Indeed, the JsonVariant returned by root["..."] has a special implementation of the == operator that knows how to compare string safely.
    // See https://arduinojson.org/faq/why-does-my-device-crash-or-reboot/
    Serial.println("Reading RGB values...");
    rgb.r = color["r"];
    rgb.b = color["b"];
    rgb.g = color["g"];
  }

  // If HSV mode: Parse HSV values
  if (color["mode"] == "hsv") {
    // Indeed, the JsonVariant returned by root["..."] has a special implementation of the == operator that knows how to compare string safely.
    // See https://arduinojson.org/faq/why-does-my-device-crash-or-reboot/
    Serial.println("Reading HSV values...");
    rgb = hsvToRgb(color["h"], color["s"], color["v"]);
  }

  // DEBUG: Parsed values
  Serial.println("Parsed Values:");
  Serial.print("r=");
  Serial.print(rgb.r);
  Serial.print(", g=");
  Serial.print(rgb.g);
  Serial.print(", b=");
  Serial.println(rgb.b);

  // TODO: support different modes
  const char* jsonrgbmode = root["mode"]; // "SOLID"
  
  current_color = rgb;
  current_brightness = brightness;
  FastLED.setBrightness(current_brightness);
  for (int i = 0; i <= LED_COUNT; i++) {
    leds[i] = CRGB(rgb.r,  rgb.g, rgb.b);
    FastLED.show();
 }       
  server.send(200, "application/json", server.arg("plain"));
}

// this is a modified version of https://gist.github.com/hdznrrd/656996
RGB_h hsvToRgb(double h, double s, double v) {
  int i;
  double f, p, q, t;
  byte r, g, b;

  h = max(0.0, min(360.0, h));
  s = max(0.0, min(100.0, s));
  v = max(0.0, min(100.0, v));

  s /= 100;
  v /= 100;

  if (s == 0) {
    // Achromatic (grey)
    r = g = b = round(v * 255);
    return {0, 0, 0};
  }

  h /= 60; // sector 0 to 5
  i = floor(h);
  f = h - i; // factorial part of h
  p = v * (1 - s);
  q = v * (1 - s * f);
  t = v * (1 - s * (1 - f));
  switch (i) {
    case 0:
      r = round(255 * v);
      g = round(255 * t);
      b = round(255 * p);
      break;
    case 1:
      r = round(255 * q);
      g = round(255 * v);
      b = round(255 * p);
      break;
    case 2:
      r = round(255 * p);
      g = round(255 * v);
      b = round(255 * t);
      break;
    case 3:
      r = round(255 * p);
      g = round(255 * q);
      b = round(255 * v);
      break;
    case 4:
      r = round(255 * t);
      g = round(255 * p);
      b = round(255 * v);
      break;
    default: // case 5:
      r = round(255 * v);
      g = round(255 * p);
      b = round(255 * q);
  }

  return {r, g, b};
}
