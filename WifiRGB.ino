#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>
#include <WiFiManager.h>
#include <NTPClient.h>

#include "names.h"
#include "web_admin.h"
#include "web_interface.h"
#include "web_iro_js.h"
#include "go_back.h"

IPAddress ip(192, 168, 2, 1);
IPAddress gateway(192, 168, 2, 1);
IPAddress subnet(255, 255, 255, 0);

#define BUILTIN_LED 2 // internal ESP-12 LED on GPIO2

// Which pin on the Arduino is connected to the NeoPixels?
// On a Trinket or Gemma we suggest changing this to 1:
#define LED_PIN     D6

// How many NeoPixels are attached to the Arduino?
#define LED_COUNT  60

// NeoPixel brightness, 0 (min) to 255 (max)
#define BRIGHTNESS 255  //(max = 255)
RGB current_color = {255,255,255};

// Declare our NeoPixel strip object:
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
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
const long utcOffsetInSeconds = 3600;

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

void setup(void) {
  Serial.begin(115200);
  // WiFiManager
  // Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  
  //wifiManager.setAPConfig(ip, gateway, subnet);
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
  server.on("/time", HTTP_POST, handleTime); //form action is handled here
 

  server.begin();
  Serial.println("WifiRGB HTTP server started");

  //Initialize LED strip
  strip.begin();           // INITIALIZE NeoPixel strip object (REQUIRED)
  strip.fill(strip.Color(255,   255,   255)); //Set LEDs to white
  strip.setBrightness(BRIGHTNESS);
  strip.show();            // Turn OFF all pixels ASAP
  
}

void loop(void) {
 server.handleClient();
   // check time every 30 seconds
  if ((millis() - seconds0) > 30000) {
    checkAlarm();
    seconds0 = millis();
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
  Serial.print("Reset");
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
    strip.fill(strip.Color(current_color.r, current_color.g, current_color.b)); //Set LEDs to color that was selected last
    strip.setBrightness(int(brightness));
    strip.show();
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
    strip.clear();
    server.send(200, "application/json", server.arg("plain"));
    return;
  }

  int brightness = root["brightness"];
  Serial.print("Brightness: ");
  Serial.println(brightness);

  // DEBUG: color
  Serial.print("Color: ");
  root["color"].printTo(Serial);
  Serial.println();

  RGB rgb = {0, 0, 0};
  JsonObject& color = root["color"];

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
  strip.fill(strip.Color(rgb.r,   rgb.g,   rgb.b)); //Set LEDs to white
  strip.setBrightness(brightness);
  strip.show();           
  server.send(200, "application/json", server.arg("plain"));
}

// this is a modified version of https://gist.github.com/hdznrrd/656996
RGB hsvToRgb(double h, double s, double v) {
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
