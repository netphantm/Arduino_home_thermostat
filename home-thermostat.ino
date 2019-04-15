#include "FS.h"
#include <SPI.h>
#include <TFT_eSPI.h>      // Hardware-specific library
//// include third party libraries
// from thermostat.ino
#include <WiFiManager.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "SSD1306Wire.h"
#include <ArduinoJson.h>
#include "Free_Fonts.h" // Include the header file attached to this sketch
#include <time.h>

//// define variables / hardware
#define ONE_WIRE_BUS D3 // DS18B20 pin D4 = GPIO2
#define RELAISPIN D0 // or D0?
#define PBSTR "|||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||"
#define PBWIDTH 79

// This is the file name used to store the calibration data
// You can change this to create new calibration files.
#define CALIBRATION_FILE "/TouchCalData2" // SPIFFS file name must start with "/".

// Set REPEAT_CAL to true instead of false to run calibration
// again, otherwise it will only be done once.
// Repeat calibration if you change the screen rotation.
#define REPEAT_CAL false
#define FORMAT_SPIFFS false

// Using two fonts since numbers are nice when bold
#define Italic_FONT &FreeSansOblique12pt7b // Key label font 1
#define Bold_FONT &FreeSansBold12pt7b    // Key label font 2

// ----- NORMAL DISPLAY ----- //
// Keypad start position, key sizes and spacing
#define KEY_N_X 42 // Centre of key
#define KEY_N_Y 282 //302
#define KEY_N_W 67 // Width and height
#define KEY_N_H 30
#define KEY_N_SPACING_X 10 // X and Y gap
#define KEY_N_SPACING_Y 7
#define KEY_N_TEXTSIZE 1   // Font size multiplier

// Numeric display box N1 size and location
#define DISP1_N_X 70
#define DISP1_N_Y 55
#define DISP1_N_W 100
#define DISP1_N_H 45
#define DISP1_N_TSIZE 5
#define DISP1_N_TCOLOR TFT_CYAN

// Numeric display box N2 size and location
#define DISP2_N_X 95
#define DISP2_N_Y 155
#define DISP2_N_W 50
#define DISP2_N_H 45
#define DISP2_N_TSIZE 5
#define DISP2_N_TCOLOR TFT_CYAN
// ----- NORMAL DISPLAY END ----- //

// ----- SETUP DISPLAY ----- //
// Keypad start position, key sizes and spacing
#define KEY_S_X 42 // Centre of key
#define KEY_S_Y 171 //191
#define KEY_S_W 67 // Width and height
#define KEY_S_H 30
#define KEY_S_SPACING_X 10 // X and Y gap
#define KEY_S_SPACING_Y 7
#define KEY_S_TEXTSIZE 1   // Font size multiplier

// Numeric display box S1 size and location
#define DISP1_S_X 181
#define DISP1_S_Y 5
#define DISP1_S_W 53
#define DISP1_S_H 45
#define DISP1_S_TSIZE 3
#define DISP1_S_TCOLOR TFT_CYAN

// Numeric display box S2 size and location
#define DISP2_S_X 181
#define DISP2_S_Y 55
#define DISP2_S_W 53
#define DISP2_S_H 45
#define DISP2_S_TSIZE 3
#define DISP2_S_TCOLOR TFT_CYAN

// Numeric display box S3 size and location
#define DISP3_S_X 181
#define DISP3_S_Y 105
#define DISP3_S_W 53
#define DISP3_S_H 45
#define DISP3_S_TSIZE 3
#define DISP3_S_TCOLOR TFT_CYAN
TFT_eSPI tft = TFT_eSPI(); // Invoke custom library
// ----- SETUP DISPLAY END ----- //

// Number length, buffer for storing it and character index
#define NUM_LEN 12
uint8_t numberIndex = 0;

// We have a status line for messages
#define STATUS_X 120 // Centred on this
#define STATUS_Y 305

// from thermostat.ino
const size_t bufferSize = JSON_OBJECT_SIZE(6) + 160;
const static String sFile = "/settings.txt";  // SPIFFS file name must start with "/".
const static String configHost = "http://temperature.hugo.ro"; // chicken/egg situation, you have to get the initial config from somewhere
unsigned long uptime = (millis() / 1000 );
unsigned long status_timer = millis();
unsigned long prevTime = 0;
unsigned long prevTimeIP = 0;
bool emptyFile = false;
bool heater = true;
bool manual = false;
bool debug = true;
char lanIP[16];
String inetIP;
String str_c;
String mode;
String WiFi_Name;
String hostname = "Donbot";
//String outputJson;
uint8_t sha1[20];
float temp_c = 25;
float temp_dev;
String webString;
String relaisState;
String SHA1;
String loghost = "temperature.hugo.ro";
String epochTime;
uint16_t color;
int httpsPort = 443;
int interval = 300000;
int cursorX;
int cursorY;
int temp_min = 24;
int temp_max = 26;

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature DS18B20(&oneWire);
ESP8266WebServer server(80);

// new
int pressed = 0;
String numberBuffer1 = String(temp_min);
String numberBuffer2 = String(temp_max);
String numberBuffer3 = String(interval/60000);
bool display_changed = true;
bool setup_screen = false;
// Create 12 keys for the setup keypad
char keyLabel1[12][5] = {"min", "max", "int", "+", "+", "+", "-", "-", "-", "Save", "RST", "Exit"};
uint16_t keyColor1[15] = {
                        TFT_BLACK, TFT_BLACK, TFT_BLACK,
                        TFT_RED, TFT_RED, TFT_RED,
                        TFT_BLUE, TFT_BLUE, TFT_BLUE,
                        TFT_DARKGREEN, TFT_DARKGREEN, TFT_DARKGREEN
                        };
// Create 3 keys for the display keypad
char keyLabel2[3][6] = {"+", "-", "Setup"};
uint16_t keyColor2[15] = {
                        TFT_RED,
                        TFT_BLUE,
                        TFT_DARKGREEN
                        };
// Invoke the TFT_eSPI button class and create all the button objects
TFT_eSPI_Button key[12];

// from thermostat.ino
//// read temperature from sensor / switch relay on or off
void getTemperature() {
  Serial.print("= getTemperature: ");
  String str_last = str_c;
  uptime = (millis() / 1000 ); // Refresh uptime
  delay(10);
  DS18B20.requestTemperatures();  // initialize temperature sensor
  temp_c = float(DS18B20.getTempCByIndex(0)); // read sensor
  yield();
  temp_c = temp_c + temp_dev; // calibrating sensor
  Serial.println(temp_c);
}

void setClock() {
  configTime(2 * 3600, 0, "pool.ntp.org", "de.pool.ntp.org");

  Serial.print("Waiting for NTP time sync: ");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("");
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));
}

void switchRelais(String sw = "TOGGLE") { // if no parameter given, assume TOGGLE
  Serial.print("= switchRelais: ");
  if (sw == "TOGGLE") {
    if (digitalRead(RELAISPIN) == 1) {
      digitalWrite(RELAISPIN, 0);
      relaisState = "OFF";
    } else {
      digitalWrite(RELAISPIN, 1);
      relaisState = "ON";
    }
    Serial.println(relaisState);
    return;
  } else {
    if (sw == "ON") {
    digitalWrite(RELAISPIN, 1);
    relaisState = "ON";
    } else if (sw == "OFF") {
      digitalWrite(RELAISPIN, 0);
      relaisState = "OFF";
    }
    Serial.println(relaisState);
  }
}

void autoSwitchRelais() {
  Serial.print("= autoSwitchRelais: ");
  if (manual) {
    delay(10);
    return;
  } else {
    if (heater) {
      if (temp_c <= temp_min) {
        switchRelais("ON");
      } else if (temp_c >= temp_max) {
        switchRelais("OFF");
      }
    } else {
      if (temp_c >= temp_max) {
        switchRelais("ON");
      } else if (temp_c <= temp_min) {
        switchRelais("OFF");
      }
    }
  }
  delay(10);
}

//// settings read / write / clear SPIFFS
void clearSpiffs() {
  Serial.println("= clearSpiffs");
  Serial.println("Please wait for SPIFFS to be formatted");
  SPIFFS.format();
  yield();
  Serial.println("SPIFFS formatted");
  emptyFile = true; // mark file as empty
  server.send(200, "text/plain", "200: OK, SPIFFS formatted, settings cleared\n");
}

void deserializeJson(String json) {
  Serial.print(F("= deserializeJson: "));
  StaticJsonBuffer<512> jsonBuffer;
  JsonObject &root = jsonBuffer.parseObject(json);
  if (!root.success()) {
    Serial.println(F("Error deserializing json!"));
    emptyFile = true;
    return;
  } else {
    SHA1 = root["SHA1"].as<String>();
    loghost = root["loghost"].as<String>(), sizeof(loghost);
    httpsPort = root["httpsPort"].as<int>(), sizeof(httpsPort);
    interval = root["interval"].as<long>(), sizeof(interval);
    temp_min = root["temp_min"].as<int>(), sizeof(temp_min);
    temp_max = root["temp_max"].as<int>(), sizeof(temp_max);
    temp_dev = root["temp_dev"].as<float>(), sizeof(temp_dev);
    heater = root["heater"].as<bool>(), sizeof(heater);
    manual = root["manual"].as<bool>(), sizeof(manual);
    debug = root["debug"].as<bool>(), sizeof(debug);
    if (debug)
      Serial.println(F("OK."));
    emptyFile = false; // mark file as not empty
  }
}

void updateSettings() {
  Serial.println("\n= updateSettings");

  if (server.args() < 1 || server.args() > 10 || !server.arg("SHA1") || !server.arg("loghost")) {
    server.send(200, "text/html", "400: Invalid Request\n");
    return;
  }
  Serial.println("Got new settings from webform");

  SHA1 = server.arg("SHA1");
  loghost = server.arg("loghost");
  httpsPort = server.arg("httpsPort").toInt();
  interval = server.arg("interval").toInt();
  temp_min = server.arg("temp_min").toInt();
  temp_max = server.arg("temp_max").toInt();
  temp_dev = server.arg("temp_dev").toFloat();
  heater = server.arg("heater").toInt();
  manual = server.arg("manual").toInt();
  debug = server.arg("debug").toInt();

  writeSettingsFile();
  writeSettingsWeb();
  server.send(200, "text/html", webString);
}

void readSettingsFile() {
  Serial.println("= readSettingsFile");

  File f = SPIFFS.open(sFile, "r"); // open file for reading
  if (!f) {
    Serial.println(F("Settings file read open failed"));
    emptyFile = true;
    return;
  }
  while (f.available()) {
    String fileJson = f.readStringUntil('\n');
    deserializeJson(fileJson);
  }
  f.close();
}

void writeSettingsFile() {
  Serial.println("= writeSettingsFile: ");

  String outputJson;
  StaticJsonBuffer<256> jsonBuffer;
  JsonObject &root = jsonBuffer.createObject();
  root["SHA1"] = SHA1;
  root["loghost"] = loghost;
  root["httpsPort"] = httpsPort;
  root["interval"] = interval;
  root["temp_min"] = temp_min;
  root["temp_max"] = temp_max;
  root["temp_dev"] = temp_dev;
  root["heater"] = heater;
  root["manual"] = manual;
  root["debug"] = debug;
  root.printTo(outputJson);
  //Serial.println("outputJson=|" + outputJson + "|");

  // write JSON to file
  File f = SPIFFS.open(sFile, "w"); // open file for writing
  if (!f) {
    Serial.println(F("Failed to create settings file"));
    server.send(200, "text/html", "200: OK, File write open failed! settings not saved\n");
    return;
  }
  if (f.print(outputJson) == 0) {
    Serial.println(F("Failed to write to file"));
    webString += "Failed to write to file\n";
  } else {
    Serial.println("OK");

    // prepare webpage for output
    webString = "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\">\n";
    webString += "<head>\n";
    webString += "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\" />\n<style>\n";
    webString += "\tbody { \n\t\tpadding: 3rem; \n\t\tfont-size: 16px;\n\t}\n";
    webString += "\tform { \n\t\tdisplay: inline; \n\t}\n</style>\n";
    webString += "</head>\n<body>\n";
    webString += "200: OK, Got new settings<br>\n";
    webString += "Settings file updated.\n<br>\nBack to \n";
    webString += "<form method='POST' action='https://temperature.hugo.ro'>";
    webString += "\n<button name='device' value='";
    webString += hostname;
    webString += "'>Graph</button>\n";
    webString += "</form>\n<br>\n";
    webString += "JSON root: \n<br>\n";
    webString += "<div id='debug'></div>\n";
    webString += "<script src='https://temperature.hugo.ro/prettyprint.js'></script>\n";
    webString += "<script>\n\tvar root = ";
    webString += outputJson;
    webString += ";\n\tvar tbl = prettyPrint(root);\n";
    webString += "\tdocument.getElementById('debug').appendChild(tbl);\n</script>\n";
    webString += "</body>\n";
    emptyFile = false; // mark file as not empty
  }
  f.close();
}

int readSettingsWeb() { // use plain http, as SHA1 fingerprint not known yet
  Serial.println("= readSettingsWeb");
  String pathQuery = "/settings-";
  pathQuery += hostname;
  pathQuery += ".json";
  if (debug) {
    Serial.print(F("Getting settings from "));
    Serial.print(configHost);
    Serial.print(pathQuery);
  }
  WiFiClient client;
  HTTPClient http;
  http.begin(client, configHost + pathQuery);
  http.addHeader("Content-Type", "application/json");
  int httpCode = http.GET();
  if(httpCode > 0) {
    String webJson = String(http.getString());
    if (debug) {
      Serial.print(F(" httpCode: "));
      Serial.println(httpCode);
      Serial.print(F("Got settings JSON from webserver: "));
      Serial.println(webJson);
    }
    deserializeJson(webJson);
  } else {
    Serial.print(F("HTTP GET ERROR: failed getting settings from web! Error: "));
    Serial.println(http.errorToString(httpCode).c_str());
  }
  http.end();
  return httpCode;
}

void writeSettingsWeb() {
  Serial.println("= writeSettingsWeb: ");

  String outputJson;
  StaticJsonBuffer<256> jsonBuffer;
  JsonObject &root = jsonBuffer.createObject();
  root["SHA1"] = SHA1;
  root["loghost"] = loghost;
  root["httpsPort"] = httpsPort;
  root["interval"] = interval;
  root["temp_min"] = temp_min;
  root["temp_max"] = temp_max;
  root["temp_dev"] = temp_dev;
  root["heater"] = heater;
  root["manual"] = manual;
  root["debug"] = debug;
  root.printTo(outputJson);
  //outputJson = serializeJson();
  //String uploadJson = urlEncode(outputJson);
  if (debug) {
    Serial.print(F("Connecting to https://"));
    Serial.println(loghost + ":" + httpsPort);
    //Serial.println("outputJson=|" + outputJson + "|");
  }

  HTTPClient http;
  //WiFiClientSecure client;
  BearSSL::WiFiClientSecure *client = new BearSSL::WiFiClientSecure ;

  bool mfln = client->probeMaxFragmentLength(loghost, 443, 1024);
  Serial.printf("Maximum fragment Length negotiation supported: %s\n", mfln ? "yes" : "no");
  if (mfln) {
    client->setBufferSizes(1024, 1024);
  }

  from_str();
  client->setFingerprint(sha1);
  //String msg = String("device=" + hostname + "&uploadJson=" + outputJson);
  String msg = String("device=" + hostname + "&uploadJson=" + urlEncode(outputJson));
  if (debug) {
    Serial.print(F("Posting data: "));
    Serial.println(msg);
  }

  if (http.begin(*client, loghost, httpsPort, "/index.php", true)) {
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    http.addHeader("User-Agent", "BuildFailureDetectorESP8266");
    http.addHeader("Host", String(loghost + ":" + httpsPort));
    http.addHeader("Content-Length", String(msg.length()));

    int  httpCode = http.POST(msg);
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTPS] code: %d\n", httpCode);

      // file found at server
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        String payload = http.getString();
        //Serial.println(payload);
      }
    } else {
      Serial.println("[HTTPS] failed, error: " + String(httpCode) + " = " +  http.errorToString(httpCode).c_str());
    }
    http.end();
  } else {
    Serial.printf("[HTTPS] Unable to connect\n");
  }
}

//// local webserver handlers / send data to logserver
void handleRoot() {
  server.send(200, "text/html", "<html><head></head><body><div align=\"center\"><h1>Nothing to see here! Move along...</h1></div></body></html>\n");
}

void handleNotFound(){
  server.send(404, "text/plain", "404: File not found!\n");
}

void logToWebserver() {
  Serial.println("= logToWebserver");

  // configure path + query for sending to logserver
  if (emptyFile) {
    Serial.println(F("Empty settings file, maybe you cleared it? Not updating logserver"));
    return;
  }
  String pathQuery = "/logtemp.php?&status=";
  pathQuery += relaisState;
  pathQuery += "&temperature=";
  pathQuery += temp_c;
  pathQuery += "&hostname=";
  pathQuery += hostname;
  pathQuery += "&temp_min=";
  pathQuery += temp_min;
  pathQuery += "&temp_max=";
  pathQuery += temp_max;
  pathQuery += "&temp_dev=";
  pathQuery += temp_dev;
  pathQuery += "&interval=";
  pathQuery += interval;
  pathQuery += "&heater=";
  pathQuery += heater;
  pathQuery += "&manual=";
  pathQuery += manual;
  pathQuery += "&debug=";
  pathQuery += debug;

  if (debug) {
    Serial.print(F("Connecting to https://"));
    Serial.print(loghost);
    Serial.println(pathQuery);
  }

  WiFiClientSecure webClient;
  from_str();
  webClient.setFingerprint(sha1);
  HTTPClient https;
  if (https.begin(webClient, loghost, httpsPort, pathQuery)) {
    int httpCode = https.GET();
    if (httpCode > 0) {
      if (debug) {
        Serial.print(F("HTTP GET OK: "));
        Serial.println(httpCode);
      }
      if (httpCode == HTTP_CODE_OK) {
          epochTime = https.getString();
          Serial.print("Timestamp on log update: ");
          Serial.println(epochTime);
      }
    } else {
      Serial.print(F("HTTP GET ERROR: failed logging to webserver! Error: "));
      Serial.println(https.errorToString(httpCode).c_str());
    }
    https.end();
  } else {
    Serial.println(F("HTTP ERROR: Unable to connect"));
  }
}

////// Miscellaneous functions

//// print variables for debug
void debug_vars() {
  Serial.println(F("# DEBUG:"));
  Serial.print(F("- hostname: "));
  Serial.println(hostname);
  Serial.print(F("- LAN IP: "));
  Serial.println(lanIP);
  Serial.print(F("- Inet IP: "));
  Serial.println(inetIP);
  Serial.print(F("- uptime: "));
  Serial.println(uptime);
  Serial.print(F("- Temperature: "));
  Serial.println(temp_c);
  Serial.print(F("- RelaisState: "));
  Serial.println(relaisState);
  Serial.print(F("- heater: "));
  Serial.println(heater);
  Serial.print(F("- manual: "));
  Serial.println(manual);
  if (emptyFile) {
    Serial.print(F("- emptyFile: "));
    Serial.println(emptyFile);
    return;
  }
  Serial.print(F("- SHA1: "));
  Serial.println(SHA1);
  Serial.print(F("- loghost: "));
  Serial.println(loghost);
  Serial.print(F("- httpsPort: "));
  Serial.println(httpsPort);
  Serial.print(F("- interval: "));
  Serial.println(interval);
  Serial.print(F("- temp_min: "));
  Serial.println(temp_min);
  Serial.print(F("- temp_max: "));
  Serial.println(temp_max);
  Serial.print(F("- temp_dev: "));
  Serial.println(temp_dev);
  Serial.print(F("- MEM free heap: "));
  Serial.println(system_get_free_heap_size());
}

//// transform SHA1 to hex format needed for setFingerprint (from aa:ab:etc. to 0xaa, 0xab, etc.)
void from_str() {
  int j = 0;
  for (int i = 0; i < 60; i = i + 3) {
    String x = ("0x" + SHA1.substring(i, i+2));
    sha1[j] = strtoul(x.c_str(), NULL, 16);
    j++;
  }
}

//// print progress bar to console
void printProgress (unsigned long percentage) {
  long val = (unsigned long) (percentage + 1);
  unsigned long lpad = (unsigned long) (val * PBWIDTH /100);
  unsigned long rpad = PBWIDTH - lpad;
  printf ("\r%3d%% [%.*s%*s]", val, lpad, PBSTR, rpad, "");
  fflush (stdout);
}

String urlEncode(String str)
{
  String encodedString="";
  char c;
  char code0;
  char code1;
  char code2;
  for (int i =0; i < str.length(); i++){
    c=str.charAt(i);
    if (c == ' '){
      encodedString+= '+';
    } else if (isalnum(c)){
      encodedString+=c;
    } else{
      code1=(c & 0xf)+'0';
      if ((c & 0xf) >9){
          code1=(c & 0xf) - 10 + 'A';
      }
      c=(c>>4)&0xf;
      code0=c+'0';
      if (c > 9){
          code0=c - 10 + 'A';
      }
      code2='\0';
      encodedString+='%';
      encodedString+=code0;
      encodedString+=code1;
      //encodedString+=code2;
    }
    yield();
  }
  return encodedString;
}

//// get internet IP (for display)
void getInetIP() {
  Serial.print(F("= getInetIP"));
  unsigned long presTimeIP = millis();
  unsigned long passedIP = presTimeIP - prevTimeIP;
  if (presTimeIP < 60000 || passedIP > 3600000) { // update every hour, so we don't piss of the guys @ ipinfo.io
    WiFiClient client;
    HTTPClient http;
    http.begin(client, "http://ipinfo.io/ip");
    http.addHeader("Content-Type", "application/text");
    int httpCode = http.GET();
    if(httpCode > 0) {
      inetIP = String(http.getString());
      inetIP.trim();
      Serial.print(F(": "));
      Serial.print(inetIP);
      prevTimeIP = presTimeIP; // save the last time Ip was updated
    } else {
      Serial.print(F("HTTPS GET ERROR: failed getting internet IP! Error: "));
      Serial.println(http.errorToString(httpCode).c_str());
      inetIP == "Error:" + httpCode;
    }
    http.end();
  }
  Serial.println();
}

//// convert sizes from bytes to KB and MB
String formatBytes(size_t bytes) {
  if (bytes < 1024) {
    return String(bytes) + "B";
  } else if (bytes < (1024 * 1024)) {
    return String(bytes / 1024.0) + "KB";
  } else if (bytes < (1024 * 1024 * 1024)) {
    return String(bytes / 1024.0 / 1024.0) + "MB";
  }
}

//// WiFi config mode
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println(F("Opening configuration portal"));
}

//void touch_calibrate() {
//}

// Print something in the mini status bar
void status(const char *msg) {
  tft.setTextPadding(240);
  //tft.setCursor(STATUS_X, STATUS_Y);
  tft.setTextColor(TFT_GREEN, TFT_DARKGREY);
  tft.setTextFont(0);
  tft.setTextDatum(TC_DATUM);
  tft.setTextSize(1);
  tft.drawString(msg, STATUS_X, STATUS_Y);
}

void status_clear() {
  if (status_timer <= (millis() - 1000)) {
    status(""); // Clear the old status
  }
}

//------------------------------------------------------------------------------------------

void setup() {
  // Use serial port
  Serial.begin(115200);

  // Initialise the TFT screen
  tft.init();

  // Set the rotation before we calibrate
  tft.setRotation(0);

  // Draw keypad background
  tft.fillRect(0, 0, 240, 320, TFT_DARKGREY);

  delay(10); // UI debouncing

  // from thermostat.ino
  DS18B20.begin();
  delay(10);
  pinMode(RELAISPIN, OUTPUT);

  WiFiManager wifiManager;
  wifiManager.setTimeout(300);
  wifiManager.setAPStaticIPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  wifiManager.setDebugOutput(false);
  wifiManager.setAPCallback(configModeCallback);
  if(!wifiManager.autoConnect("Joey","pass4esp")) {
    delay(1000);
    Serial.println(F("Failed to connect and hit timeout, restarting..."));
    ESP.reset();
  }
  Serial.print(F("Seconds in void setup() WiFi connection: "));
  int connRes = WiFi.waitForConnectResult();
  Serial.println(connRes);
  if (WiFi.status()!=WL_CONNECTED) {
      Serial.println(F("Failed to connect to WiFi, resetting in 5 seconds"));
      delay(5000);
      ESP.reset();
  } else {
    if (!MDNS.begin("esp8266"))
      Serial.println(F("Error setting up mDNS responder"));
  }
  IPAddress ip = WiFi.localIP();
  sprintf(lanIP, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);

  if (!SPIFFS.begin() || FORMAT_SPIFFS) { // initialize SPIFFS
    Serial.println(F("Formating file system"));
    SPIFFS.format();
    SPIFFS.begin();
  }
  Serial.println(F("SPIFFS started. Contents:"));
  Dir dir = SPIFFS.openDir("/");
  while (dir.next()) { // List the file system contents
    String fileName = dir.fileName();
    size_t fileSize = dir.fileSize();
    Serial.printf("\tFS File: %s, size: %s\r\n", fileName.c_str(), formatBytes(fileSize).c_str());
  }
  Serial.println("\n");

  // Calibrate the touch screen and retrieve the scaling factors
  uint16_t calData[5];
  uint8_t calDataOK = 0;

  // check if calibration file exists and size is correct
  if (SPIFFS.exists(CALIBRATION_FILE)) {
    if (REPEAT_CAL)
    {
      // Delete if we want to re-calibrate
      SPIFFS.remove(CALIBRATION_FILE);
    }
    else
    {
      File f = SPIFFS.open(CALIBRATION_FILE, "r");
      if (f) {
        if (f.readBytes((char *)calData, 14) == 14)
          calDataOK = 1;
        f.close();
      }
    }
  }

  if (calDataOK && !REPEAT_CAL) {
    // calibration data valid
    tft.setTouch(calData);
  } else {
    // data not valid so recalibrate
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(20, 0);
    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    tft.println("Touch corners as indicated");

    tft.setTextFont(1);
    tft.println();

    if (REPEAT_CAL) {
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.println("Set REPEAT_CAL to false to stop this running again!");
    }

    tft.calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 15);

    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.println("Calibration complete!");

    // store data
    File f = SPIFFS.open(CALIBRATION_FILE, "w");
    if (f) {
      f.write((const unsigned char *)calData, 14);
      f.close();
    }
  }

  switchRelais("OFF"); // start with relais OFF

  if (readSettingsWeb() != 200) // first, try reading settings from webserver
    readSettingsFile(); // if failed, read settings from SPIFFS
  getInetIP();
  getTemperature();
  autoSwitchRelais();

  // local webserver client handlers
  server.onNotFound(handleNotFound);
  server.on("/", handleRoot);
  server.on("/update", []() {
    updateSettings();
    getTemperature();
    autoSwitchRelais();
    if (debug)
      debug_vars();
  });
  server.on("/clear", clearSpiffs);

  server.begin();
} // void setup() END

//------------------------------------------------------------------------------------------

void loop(void) {
  String IamHere = "loop()";
  int pressed = 0;
  if (display_changed && setup_screen) {
    // ----- SETUP DISPLAY INIT ----- //

    // Clear the screen
    tft.fillScreen(TFT_DARKGREY);

    // Draw number display area and frame
    tft.setTextDatum(TL_DATUM);    // Use top left corner as text coord datum
    tft.setFreeFont(Italic_FONT);  // Choose a nice font for the text
    tft.setTextColor(TFT_BLACK);

    tft.drawString("Temp min", 6, 20);
    tft.fillRect(DISP1_S_X, DISP1_S_Y, DISP1_S_W, DISP1_S_H, TFT_BLACK);
    tft.drawRect(DISP1_S_X, DISP1_S_Y, DISP1_S_W, DISP1_S_H, TFT_WHITE);
    tft.drawString("Temp max", 6, 70);
    tft.fillRect(DISP2_S_X, DISP2_S_Y, DISP2_S_W, DISP2_S_H, TFT_BLACK);
    tft.drawRect(DISP2_S_X, DISP2_S_Y, DISP2_S_W, DISP2_S_H, TFT_WHITE);
    tft.drawString("Interval", 6, 120);
    tft.fillRect(DISP3_S_X, DISP3_S_Y, DISP3_S_W, DISP3_S_H, TFT_BLACK);
    tft.drawRect(DISP3_S_X, DISP3_S_Y, DISP3_S_W, DISP3_S_H, TFT_WHITE);

    tft.fillRect(DISP1_S_X + 4, DISP1_S_Y + 1, DISP1_S_W - 5, DISP1_S_H - 2, TFT_BLACK);
    String numberBuffer1= String(temp_min);
    tft.fillRect(DISP2_S_X + 4, DISP2_S_Y + 1, DISP2_S_W - 5, DISP2_S_H - 2, TFT_BLACK);
    String numberBuffer2= String(temp_max);
    tft.fillRect(DISP3_S_X + 4, DISP3_S_Y + 1, DISP3_S_W - 5, DISP3_S_H - 2, TFT_BLACK);
    String numberBuffer3= String(interval/60000);

    // Update the number display field
    tft.setTextDatum(TL_DATUM);        // Use top left corner as text coord datum
    tft.setFreeFont(&FreeSans18pt7b);  // Choose a nice font that fits box
    tft.setTextColor(DISP1_S_TCOLOR);  // Set the font color
    // Draw the string, the value returned is the width in pixels
    int xwidth1 = tft.drawString(numberBuffer1, DISP1_S_X + 4, DISP1_S_Y + 9);
    // Now cover up the rest of the line up by drawing a black rectangle.  No flicker this way
    tft.fillRect(DISP1_S_X + 4 + xwidth1, DISP1_S_Y + 1, DISP1_S_W - xwidth1 - 5, DISP1_S_H - 2, TFT_BLACK);
    // Draw the string, the value returned is the width in pixels
    int xwidth2 = tft.drawString(numberBuffer2, DISP2_S_X + 4, DISP2_S_Y + 9);
    // Now cover up the rest of the line up by drawing a black rectangle.  No flicker this way
    tft.fillRect(DISP2_S_X + 4 + xwidth2, DISP2_S_Y + 1, DISP2_S_W - xwidth2 - 5, DISP2_S_H - 2, TFT_BLACK);
    // Draw the string, the value returned is the width in pixels
    int xwidth3 = tft.drawString(numberBuffer3, DISP3_S_X + 4, DISP3_S_Y + 9);
    // Now cover up the rest of the line up by drawing a black rectangle.  No flicker this way
    tft.fillRect(DISP3_S_X + 4 + xwidth3, DISP3_S_Y + 1, DISP3_S_W - xwidth3 - 5, DISP3_S_H - 2, TFT_BLACK);

    // Draw keypad Setup
    for (uint8_t row = 0; row < 4; row++) {
      for (uint8_t col = 0; col < 3; col++) {
        uint8_t b = col + row * 3;

        tft.setFreeFont(Bold_FONT);

        key[b].initButton(&tft, KEY_S_X + col * (KEY_S_W + KEY_S_SPACING_X),
                          KEY_S_Y + row * (KEY_S_H + KEY_S_SPACING_Y), // x, y, w, h, outline, fill, text
                          KEY_S_W, KEY_S_H, TFT_WHITE, keyColor1[b], TFT_WHITE,
                          keyLabel1[b], KEY_S_TEXTSIZE);
        key[b].drawButton();
      }
    // ----- SETUP DISPLAY INIT END ----- //
    }
    display_changed = false;
    IamHere = "display_changed && setup_screen";
  }

  if (display_changed && ! setup_screen) {
    // ----- DISPLAY ROUTINE INIT ----- //

    // Clear the screen
    tft.fillScreen(TFT_DARKGREY);

    // Draw number display area and frame
    tft.setTextDatum(TL_DATUM);    // Use top left corner as text coord datum
    tft.setFreeFont(Italic_FONT);  // Choose a nice font for the text
    tft.setTextColor(TFT_BLACK);

    tft.drawString("Room Temperature", 6, 20);
    tft.fillRect(DISP1_N_X, DISP1_N_Y, DISP1_N_W, DISP1_N_H, TFT_BLACK);
    tft.drawRect(DISP1_N_X, DISP1_N_Y, DISP1_N_W, DISP1_N_H, TFT_WHITE);
    tft.drawString("Target Temperature", 6, 120);
    tft.fillRect(DISP2_N_X, DISP2_N_Y, DISP2_N_W, DISP2_N_H, TFT_BLACK);
    tft.drawRect(DISP2_N_X, DISP2_N_Y, DISP2_N_W, DISP2_N_H, TFT_WHITE);

    tft.fillRect(DISP1_N_X + 4, DISP1_N_Y + 1, DISP1_N_W - 5, DISP1_N_H - 2, TFT_BLACK);
    String numberBuffer_a= String(temp_c);
    tft.fillRect(DISP2_N_X + 4, DISP2_N_Y + 1, DISP2_N_W - 5, DISP2_N_H - 2, TFT_BLACK);
    String numberBuffer_b= String(temp_min+((temp_max-temp_min)/2));

    // Update the number display field
    tft.setTextDatum(TL_DATUM);        // Use top left corner as text coord datum
    tft.setFreeFont(&FreeSans18pt7b);  // Choose a nice font that fits box
    tft.setTextColor(DISP1_N_TCOLOR);  // Set the font color
    // Draw the string, the value returned is the width in pixels
    int xwidth_a = tft.drawString(numberBuffer_a, DISP1_N_X + 4, DISP1_N_Y + 9);
    // Now cover up the rest of the line up by drawing a black rectangle.  No flicker this way
    tft.fillRect(DISP1_N_X + 4 + xwidth_a, DISP1_N_Y + 1, DISP1_N_W - xwidth_a - 5, DISP1_N_H - 2, TFT_BLACK);
    // Draw the string, the value returned is the width in pixels
    int xwidth_b = tft.drawString(numberBuffer_b, DISP2_N_X + 4, DISP2_N_Y + 9);
    // Now cover up the rest of the line up by drawing a black rectangle.  No flicker this way
    tft.fillRect(DISP2_N_X + 4 + xwidth_b, DISP2_N_Y + 1, DISP2_N_W - xwidth_b - 5, DISP2_N_H - 2, TFT_BLACK);

    // Draw keypad Display
    for (uint8_t row = 0; row < 1; row++) {
      for (uint8_t col = 0; col < 3; col++) {
        uint8_t b = col + row * 3;

        tft.setFreeFont(Bold_FONT);

        key[b].initButton(&tft, KEY_N_X + col * (KEY_N_W + KEY_N_SPACING_X),
                          KEY_N_Y + row * (KEY_N_H + KEY_N_SPACING_Y), // x, y, w, h, outline, fill, text
                          KEY_N_W, KEY_N_H, TFT_WHITE, keyColor2[b], TFT_WHITE,
                          keyLabel2[b], KEY_N_TEXTSIZE);
        key[b].drawButton();
      }
    }
    // ----- DISPLAY ROUTINE INIT END ----- //
    display_changed = false;
    IamHere = "display_changed && ! setup_screen";
  }

  // --- key was pressed --- //

  if (setup_screen) {
    // ----- SETUP ROUTINE ----- //
    uint16_t t_x = 0, t_y = 0; // To store the touch coordinates
    // Pressed will be set true is there is a valid touch on the screen
    pressed = tft.getTouch(&t_x, &t_y);

    // Check if any key coordinate boxes contain the touch coordinates
    for (uint8_t b = 3; b < 12; b++) {
      if (key[b].contains(t_x, t_y)) {
        key[b].press(true);  // tell the button it is pressed
      } else {
        key[b].press(false);  // tell the button it is NOT pressed
      }
    }

    // Check if any key has changed state
    for (uint8_t b = 3; b < 12; b++) {
      tft.setFreeFont(Bold_FONT);
      if (key[b].justReleased()) key[b].drawButton();     // draw normal
      if (key[b].justPressed()) {
        key[b].drawButton(true);  // draw invert

        // if a numberpad button, append + to the numberBuffer
        // 3/6
        if (b == 3 && temp_min <= 35) {
            temp_min++;
            status(""); // Clear the old status
        }
        if (b == 6 && temp_min >= 16) {
            temp_min--;
            status(""); // Clear the old status
        }

        // 4/7
        if (b == 4 && temp_max <= 37) {
            temp_max++;
            status(""); // Clear the old status
        }
        if (b == 7 && temp_max >= 16) {
            temp_max--;
            status(""); // Clear the old status
        }

        //5/8
        if (b == 5 && interval <= 840000) {
            interval = interval + 60000;
            status(""); // Clear the old status
        }
        if (b == 8 && interval >= 180000) {
            interval = interval - 60000;
            status(""); // Clear the old status
        }

        // New
        if (b == 10) {
          temp_min = 24;
          temp_max = 26;
          interval = 300000;
          status_timer = millis();
          status("Values cleared");
        }

        tft.fillRect(DISP1_S_X + 4, DISP1_S_Y + 1, DISP1_S_W - 5, DISP1_S_H - 2, TFT_BLACK);
        String numberBuffer1= String(temp_min);
        tft.fillRect(DISP2_S_X + 4, DISP2_S_Y + 1, DISP2_S_W - 5, DISP2_S_H - 2, TFT_BLACK);
        String numberBuffer2= String(temp_max);
        tft.fillRect(DISP3_S_X + 4, DISP3_S_Y + 1, DISP3_S_W - 5, DISP3_S_H - 2, TFT_BLACK);
        String numberBuffer3= String(interval/60000);

        // Update the number display field
        tft.setTextDatum(TL_DATUM);        // Use top left corner as text coord datum
        tft.setFreeFont(&FreeSans18pt7b);  // Choose a nice font that fits box
        tft.setTextColor(DISP1_S_TCOLOR);  // Set the font color
        // Draw the string, the value returned is the width in pixels
        int xwidth1 = tft.drawString(numberBuffer1, DISP1_S_X + 4, DISP1_S_Y + 9);
        // Now cover up the rest of the line up by drawing a black rectangle.  No flicker this way
        tft.fillRect(DISP1_S_X + 4 + xwidth1, DISP1_S_Y + 1, DISP1_S_W - xwidth1 - 5, DISP1_S_H - 2, TFT_BLACK);
        // Draw the string, the value returned is the width in pixels
        int xwidth2 = tft.drawString(numberBuffer2, DISP2_S_X + 4, DISP2_S_Y + 9);
        // Now cover up the rest of the line up by drawing a black rectangle.  No flicker this way
        tft.fillRect(DISP2_S_X + 4 + xwidth2, DISP2_S_Y + 1, DISP2_S_W - xwidth2 - 5, DISP2_S_H - 2, TFT_BLACK);
        // Draw the string, the value returned is the width in pixels
        int xwidth3 = tft.drawString(numberBuffer3, DISP3_S_X + 4, DISP3_S_Y + 9);
        // Now cover up the rest of the line up by drawing a black rectangle.  No flicker this way
        tft.fillRect(DISP3_S_X + 4 + xwidth3, DISP3_S_Y + 1, DISP3_S_W - xwidth3 - 5, DISP3_S_H - 2, TFT_BLACK);

        if (b == 9) {
          Serial.println("temp_min: " + numberBuffer1);
          Serial.println("temp_max: " + numberBuffer2);
          Serial.println("interval: " + numberBuffer3);
          status_timer = millis();
          writeSettingsFile();
          writeSettingsWeb();
          status("Values saved");
        }

        // Exit Setup
        if (b == 11) {
          status("Exit Setup screen");
          Serial.println(F("Exit Setup screen"));
          setup_screen = false;
          display_changed = true;
          delay(300);
        }

        delay(10); // UI debouncing
      }
    }
    pressed = 0;
    IamHere = "setup_screen && pressed";
    // ----- SETUP ROUTINE END ----- //
  }

  if (! setup_screen) {
    // ----- DISPLAY ROUTINE ----- //
    uint16_t t_x = 0, t_y = 0; // To store the touch coordinates
    // Pressed will be set true is there is a valid touch on the screen
    pressed = tft.getTouch(&t_x, &t_y);

    // Check if any key coordinate boxes contain the touch coordinates
    for (uint8_t b = 0; b < 3; b++) {
      if (key[b].contains(t_x, t_y)) {
        key[b].press(true);  // tell the button it is pressed
      } else {
        key[b].press(false);  // tell the button it is NOT pressed
      }
    }

    // Check if any key has changed state
    for (uint8_t b = 0; b < 3; b++) {
      tft.setFreeFont(Bold_FONT);
      if (key[b].justReleased()) key[b].drawButton();     // draw normal
      if (key[b].justPressed()) {
        key[b].drawButton(true);  // draw invert

        // if a numberpad button, append + to the numberBuffer
        // 0/1
        if (b == 0 && temp_max <= 29) {
            temp_max++;
            status_timer = millis();
            status("Temperature raised");
        }
        if (b == 1 && temp_max >= 19) {
            temp_max--;
            status_timer = millis();
            status("Temperature lowered");
        }
        // Enter Setup
        if (b == 2) {
          Serial.println(F("Enter Setup screen"));
          setup_screen = true;
          display_changed = true;
          status("Enter Setup screen");
          delay(300);
        }

        tft.fillRect(DISP1_N_X + 4, DISP1_N_Y + 1, DISP1_N_W - 5, DISP1_N_H - 2, TFT_BLACK);
        String numberBuffer_a= String(temp_c);
        tft.fillRect(DISP2_N_X + 4, DISP2_N_Y + 1, DISP2_N_W - 5, DISP2_N_H - 2, TFT_BLACK);
        String numberBuffer_b= String(temp_min+((temp_max-temp_min)/2));

        // Update the number display field
        tft.setTextDatum(TL_DATUM);        // Use top left corner as text coord datum
        tft.setFreeFont(&FreeSans18pt7b);  // Choose a nice font that fits box
        tft.setTextColor(DISP1_N_TCOLOR);  // Set the font color
        // Draw the string, the value returned is the width in pixels
        int xwidth_a = tft.drawString(numberBuffer_a, DISP1_N_X + 4, DISP1_N_Y + 9);
        // Now cover up the rest of the line up by drawing a black rectangle.  No flicker this way
        tft.fillRect(DISP1_N_X + 4 + xwidth_a, DISP1_N_Y + 1, DISP1_N_W - xwidth_a - 5, DISP1_N_H - 2, TFT_BLACK);
        // Draw the string, the value returned is the width in pixels
        int xwidth_b = tft.drawString(numberBuffer_b, DISP2_N_X + 4, DISP2_N_Y + 9);
        // Now cover up the rest of the line up by drawing a black rectangle.  No flicker this way
        tft.fillRect(DISP2_N_X + 4 + xwidth_b, DISP2_N_Y + 1, DISP2_N_W - xwidth_b - 5, DISP2_N_H - 2, TFT_BLACK);

        delay(10); // UI debouncing
      }
    }
    pressed = 0;
    IamHere = "! setup_screen && pressed";
    // ----- DISPLAY ROUTINE END ----- //
  }
  /*
  Serial.print("IamHere = ");
  Serial.println(IamHere);
  //Serial.println("");
  */
  delay(100);
  
  // from thermostat.ino
  unsigned long presTime = millis();
  unsigned long passed = presTime - prevTime;
  if (passed > interval) {
    display_changed = true;
    Serial.println(F("\nInterval passed"));
    getInetIP();
    if (readSettingsWeb() != 200) // first, try reading settings from webserver
      readSettingsFile(); // if failed, read settings from SPIFFS
    prevTime = presTime; // save the last time sensor was read

    if (emptyFile) { // settings file does not exist
      if (readSettingsWeb() != 200) { // first, try reading settings from webserver
        Serial.println(F("Switching relais off: no settings found on SPIFFS OR webserver!"));
        switchRelais("OFF");
        debug_vars();
        Serial.println(F("Waiting for settings to be sent..."));
        delay(500);
      } else {
        writeSettingsFile();
      }
    } else {
      getTemperature();
      autoSwitchRelais();
      if (debug)
        debug_vars();
      logToWebserver();
    }

    if (interval < 4999) // set a failsafe interval
      interval = 20000; // 20 secs
  } else {
    printProgress(passed * 100 / interval);
  }
  server.handleClient();
  status_clear();
  //setClock();
} // void loop() END
