//------------------------------------------------------------------------------------------
// TODO - fix bug: logToWebserver exception: std::bad_alloc
//------------------------------------------------------------------------------------------
#include "FS.h"
#include <Time.h>
#include <Timezone.h>
#include <TimeLib.h>
#include <TimeAlarms.h>
#include <NTPClient.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <WiFiManager.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266mDNS.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "SSD1306Wire.h"
#include <ArduinoJson.h>
#include "Free_Fonts.h"

//// define variables / hardware
#define ONE_WIRE_BUS D3
#define RELAISPIN D0
#define PBSTR "|||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||"
#define PBWIDTH 79

// This is the file name used to store the calibration data
// You can change this to create new calibration files.
#define CALIBRATION_FILE "/TouchCalData" // SPIFFS file name must start with "/".

// Set REPEAT_CAL to true to run calibration again
// Repeat calibration if you change the screen rotation.
#define REPEAT_CAL false
#define FORMAT_SPIFFS false

// NTP settings
#define NTP_OFFSET   0 //60 * 60      // In seconds
#define NTP_INTERVAL 60 * 60 * 1000    // In miliseconds
#define NTP_ADDRESS  "de.pool.ntp.org"  // change this to whatever pool is closest (see ntp.org)

// Set fonts
#define Italic_FONT &FreeSansOblique12pt7b
#define Small_FONT &FreeSans9pt7b
#define Big_FONT &FreeSans24pt7b
#define Bold_FONT &FreeSansBold12pt7b

// ----- NORMAL DISPLAY ----- //
// Keypad +/- manual temperature set
#define KEY_N_X 40 // Centre of key
#define KEY_N_Y 285
#define KEY_N_W 75 // Width and height
#define KEY_N_H 30
#define KEY_N_SPACING_X 5 // X and Y gap
#define KEY_N_SPACING_Y 7

// Numeric display box temp_c
#define DISP1_N_X 25
#define DISP1_N_Y 30
#define DISP1_N_W 100
#define DISP1_N_H 50

// Numeric display box setTemp
#define DISP2_N_X 155
#define DISP2_N_Y 38
#define DISP2_N_W 48
#define DISP2_N_H 45
// ----- NORMAL DISPLAY END ----- //

// ----- SETUP DISPLAY ----- //
// Keypad 4x3
#define KEY_S_X 30 // Centre of key
#define KEY_S_Y 174
#define KEY_S_W 58 // Width and height
#define KEY_S_H 29
#define KEY_S_SPACING_X 2 // X and Y gap
#define KEY_S_SPACING_Y 7

// Numeric display box temp_min
#define DISP1_S_X 181
#define DISP1_S_Y 5
#define DISP1_S_W 53
#define DISP1_S_H 45

// Numeric display box temp_max
#define DISP2_S_X 181
#define DISP2_S_Y 55
#define DISP2_S_W 53
#define DISP2_S_H 45

// Numeric display box interval
#define DISP3_S_X 181
#define DISP3_S_Y 105
#define DISP3_S_W 53
#define DISP3_S_H 45
// ----- SETUP DISPLAY END ----- //

// ----- SCHEDULE DISPLAY ----- //
// Keypad timers start
#define KEY1_P_X 100 // Centre of key
#define KEY1_P_Y 75
#define KEY1_P_W 30 // Width and height
#define KEY1_P_H 25
#define KEY1_P_SPACING_X 3 // X and Y gap
#define KEY1_P_SPACING_Y 45

// Keypad temperatures
#define KEY2_P_X 188 // Centre of key
#define KEY2_P_Y 75
#define KEY2_P_W 30 // Width and height
#define KEY2_P_H 25
#define KEY2_P_SPACING_X 3 // X and Y gap
#define KEY2_P_SPACING_Y 45

// Numeric display of scheduled programs
#define DISP_P_X 92
#define DISP_P_Y 40
#define DISP_P_W 50
#define DISP_P_H 20
// ----- SCHEDULE DISPLAY END ----- //

#define TFT_DARKERGREEN 0x0547

// Status line for messages
#define STATUS_X 120 // Centred on this
#define STATUS_Y 307

// Button and clock colors
#define BUTTON_LABEL 0x7BEF
#define BUTTON_PLUS 0xFB21
#define BUTTON_MINUS 0x353E
#define EXIT_MENU TFT_DARKGREEN //0x07E0
#define COLOR_CLOCK 0x3ED7

// from thermostat.ino
const size_t jsonCapacity = JSON_OBJECT_SIZE(19) + 320;
const static String sFile = "/settings.txt";  // SPIFFS file name must start with "/".
const static String configHost = "temperature.hugo.ro"; // chicken/egg situation, you have to get the initial config from somewhere
unsigned long uptime = (millis() / 1000 );
unsigned long status_timer = millis();
unsigned long setupStarted = millis();
unsigned long scheduleStarted = millis();
unsigned long prevTime = 0;
unsigned long clockTime = 0;
bool emptyFile = false;
bool heater = true;
bool manual = false;
bool debug = true;
char lanIP[16];
String webString, relaisState, SHA1, loghost, epochTime;
String hostname = "Donbot";
uint8_t sha1[20];
float temp_c;
float temp_dev;
char temp_short[8];
int wRelais, wState, wComma;
int httpsPort = 443;
int interval = 300000;
int temp_min = 24;
int temp_max = 26;
int setTemp = temp_min+(temp_max-temp_min)/2;
int textLineY = 92;
int textLineX = 135;

// new
bool display_changed = true;
bool setup_screen = false;
bool program_screen = false;
bool statusCleared = true;
int pressed = 0;
int tempP0 = 18;
int tempP1 = 19;
int tempP2 = 20;
int hourP0 = 7;
int minuteP0 = 0;
int hourP1 = 9;
int minuteP1 = 0;
int hourP2 = 21;
int minuteP2 = 30;
time_t local, utc;
char exitButton [] = "Exit";

// Create 16 keys for the setup keypad
char keyLabelSetup[16][5] = {"Heat", "min", "max", "int", "Auto", "+", "+", "+", "Man", "-", "-", "-", "Prog", "RST", "Save", "Exit"};
uint16_t keyColorSetup[16] = {
                        BUTTON_LABEL, BUTTON_LABEL, BUTTON_LABEL, BUTTON_LABEL,
                        BUTTON_PLUS, BUTTON_PLUS, BUTTON_PLUS, BUTTON_PLUS,
                        BUTTON_MINUS, BUTTON_MINUS, BUTTON_MINUS, BUTTON_MINUS,
                        EXIT_MENU, EXIT_MENU, EXIT_MENU, EXIT_MENU
                        };
// Create 3 keys for the display keypad
char keyLabelDisplay[3][6] = {"+", "-", "Setup"};
uint16_t keyColorDisplay[3] = {
                        BUTTON_PLUS,
                        BUTTON_MINUS,
                        EXIT_MENU
                        };
// Create 18 keys for the schedule keypad
char keyLabelSchedule[6][2] = {"-", "+", "-", "+", "-", "+"};
uint16_t keyColorSchedule[7] = {
                        BUTTON_MINUS, BUTTON_PLUS,
                        BUTTON_MINUS, BUTTON_PLUS,
                        BUTTON_MINUS, BUTTON_PLUS,
                        EXIT_MENU
                        };

// Invoke the TFT_eSPI button class and create all the button objects
TFT_eSPI_Button key[16];

// Set up the NTP UDP client
String date, timeNow, timeOld;
const char * days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"} ;
const char * months[] = {"Jan", "Feb", "Mar", "Apr", "May", "June", "July", "Aug", "Sep", "Oct", "Nov", "Dec"} ;
int minuteNextAlarm, hourNextAlarm, minuteNow, hourNow, minuteOld, hourOld, clockX, clockY, clockRadius, x2, y2, x3, y3, x4, y4, x5, y5, x6, y6, x4_old, y4_old, x5_old, y5_old;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, NTP_OFFSET, NTP_INTERVAL);

// from thermostat.ino
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature DS18B20(&oneWire);
ESP8266WebServer server(80);
TFT_eSPI tft = TFT_eSPI(); // Invoke custom TFT library

//------------------------------------------------------------------------------------------

//// read temperature from sensor / switch relay on or off
void getTemperature() {
  Serial.print("= getTemperature: ");
  float last_temp_c = temp_c;
  uptime = (millis() / 1000 ); // Refresh uptime
  DS18B20.requestTemperatures();  // initialize temperature sensor
  temp_c = float(DS18B20.getTempCByIndex(0)); // read sensor
  yield();
  if (temp_c < -120)
    temp_c = last_temp_c;
  temp_c = temp_c + temp_dev; // calibrating sensor
  Serial.println(temp_c);
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
    digitalWrite(RELAISPIN, 0);
    relaisState = "ON";
    } else if (sw == "OFF") {
      digitalWrite(RELAISPIN, 1);
      relaisState = "OFF";
    }
    Serial.println(relaisState);
  }
}

void autoSwitchRelais() {
  Serial.print("= autoSwitchRelais: ");
  if (temp_c <= temp_min) {
    switchRelais("ON");
  } else if (temp_c >= temp_max) {
    switchRelais("OFF");
    Alarm.delay(10);
  }
  if (! setup_screen && ! program_screen)
    updateDisplayN();
}

//// SPIFFS settings read / write / clear
void clearSpiffs() {
  Serial.println("= clearSpiffs");
  Serial.println("Please wait for SPIFFS to be formatted");
  SPIFFS.format();
  yield();
  Serial.println("SPIFFS formatted");
  emptyFile = true; // mark file as empty
  server.send(200, "text/plain", "200: OK, SPIFFS formatted, settings cleared\n");
}

void deserializeJsonDynamic(String json) {
  Serial.print(F("= deserializeJsonDynamic: "));
  DynamicJsonBuffer jsonBuffer(jsonCapacity);
  JsonObject& root = jsonBuffer.parseObject(json);
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
    tempP0 = root["tempP0"].as<int>(), sizeof(tempP0);
    hourP0 = root["hourP0"].as<int>(), sizeof(hourP0);
    minuteP0 = root["minuteP0"].as<int>(), sizeof(minuteP0);
    tempP1 = root["tempP1"].as<int>(), sizeof(tempP1);
    hourP1 = root["hourP1"].as<int>(), sizeof(hourP1);
    minuteP1 = root["minuteP1"].as<int>(), sizeof(minuteP1);
    tempP2 = root["tempP2"].as<int>(), sizeof(tempP2);
    hourP2 = root["hourP2"].as<int>(), sizeof(hourP2);
    minuteP2 = root["minuteP2"].as<int>(), sizeof(minuteP2);
    Serial.println(F("OK."));
    emptyFile = false; // mark file as not empty
  }
}

String serializeJsonDynamic() {
  Serial.print(F("= serializeJsonDynamic: "));
  DynamicJsonBuffer jsonBuffer(jsonCapacity);
  JsonObject& root = jsonBuffer.createObject();
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
  root["tempP0"] = tempP0;
  root["hourP0"] = hourP0;
  root["minuteP0"] = minuteP0;
  root["tempP1"] = tempP1;
  root["hourP1"] = hourP1;
  root["minuteP1"] = minuteP1;
  root["tempP2"] = tempP2;
  root["hourP2"] = hourP2;
  root["minuteP2"] = minuteP2;
  String outputJson;
  root.printTo(outputJson);
  Serial.println(F("OK."));
  if (debug) {
    Serial.println(outputJson);
  }
  return outputJson;
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
    deserializeJsonDynamic(fileJson);
  }
  f.close();
}

void writeSettingsFile() {
  Serial.println("= writeSettingsFile: ");

  String outputJson = serializeJsonDynamic();

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
  yield();
  f.close();
}

int readSettingsWeb() { // use plain http, as SHA1 fingerprint not known yet
  Serial.println("= readSettingsWeb");
  String pathQuery = "/settings-";
  pathQuery += hostname;
  pathQuery += ".json";
  if (debug) {
    Serial.print(F("Getting settings from http://"));
    Serial.print(configHost);
    Serial.println(pathQuery);
  }
  WiFiClient client;
  HTTPClient http;
  http.begin(client, "http://" + configHost + pathQuery);
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
    deserializeJsonDynamic(webJson);
  } else {
    Serial.print(F("HTTP GET ERROR: failed getting settings from web! Error: "));
    Serial.println(http.errorToString(httpCode).c_str());
  }
  Alarm.delay(10);
  http.end();
  client.flush();
  return httpCode;
}

void writeSettingsWeb() {
  Serial.println("= writeSettingsWeb: ");

  String outputJson = serializeJsonDynamic();

  HTTPClient http;
  BearSSL::WiFiClientSecure *client = new BearSSL::WiFiClientSecure ;

  bool mfln = client->probeMaxFragmentLength(configHost, 443, 1024);
  Serial.printf("Maximum fragment Length negotiation supported: %s\n", mfln ? "yes" : "no");
  if (mfln) {
    client->setBufferSizes(1024, 1024);
  }

  fingerprint2Hex();
  client->setFingerprint(sha1);
  String msg = String("device=" + hostname + "&uploadJson=" + urlEncode(outputJson));
  if (debug) {
    Serial.print(F("Posting data: "));
    Serial.println(msg);
  }

  if (http.begin(*client, configHost, httpsPort, "/index.php", true)) {
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    http.addHeader("User-Agent", "ESP8266HTTPClient");
    http.addHeader("Host", String(configHost + ":" + httpsPort));
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
      status_timer = millis();
      statusPrint("Saved settings to webserver");
    } else {
      Serial.println("[HTTPS] failed, error: " + String(httpCode) + " = " +  http.errorToString(httpCode).c_str());
      status_timer = millis();
      statusPrint("ERROR saving to webserver");
    }
    http.end();
  } else {
    Serial.printf("[HTTPS] Unable to connect\n");
    status_timer = millis();
    statusPrint("ERROR saving to webserver");
  }
  Alarm.delay(10);
  client->flush();
  if (debug)
    debugVars();
}

void logToWebserver() {
  Serial.println("= logToWebserver");

  // configure path + query for sending to logserver
  if (emptyFile) {
    Serial.println(F("Empty settings file, maybe you cleared it? Not updating logserver"));
    return;
  }
  String pathQuery = "/logtemp.php?&status=" + relaisState + "&temperature=" + temp_c;
  pathQuery = pathQuery + "&hostname=" + hostname + "&temp_min=" + temp_min + "&temp_max=" + temp_max + "&temp_dev=" + temp_dev;
  pathQuery = pathQuery + "&interval=" + interval + "&heater=" + heater + "&manual=" + manual + "&debug=" + debug;
  pathQuery = pathQuery + "&tempP0=" + tempP0 + "&hourP0=" + hourP0 + "&minuteP0=" + minuteP0;
  pathQuery = pathQuery + "&tempP1=" + tempP1 + "&hourP1=" + hourP1 + "&minuteP1=" + minuteP1;
  pathQuery = pathQuery + "&tempP2=" + tempP2 + "&hourP2=" + hourP2 + "&minuteP2=" + minuteP2;

  if (debug) {
    Serial.print(F("Connecting to https://"));
    Serial.print(loghost);
    Serial.println(pathQuery);
  }

  WiFiClientSecure client;
  fingerprint2Hex();
  client.setFingerprint(sha1);
  HTTPClient https;
  if (https.begin(client, loghost, httpsPort, pathQuery)) {
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
      status_timer = millis();
      statusPrint("Logged data to webserver");
    } else {
      Serial.print(F("HTTP GET ERROR: failed logging to webserver! Error: "));
      Serial.println(https.errorToString(httpCode).c_str());
      status_timer = millis();
      statusPrint("ERROR logging to webserver");
    }
  } else {
    Serial.println(F("HTTP ERROR: Unable to connect"));
    status_timer = millis();
    statusPrint("ERROR logging to webserver");
  }
  Alarm.delay(10);
  https.end();
  client.flush();

  /*
  HTTPClient http;
  BearSSL::WiFiClientSecure *client = new BearSSL::WiFiClientSecure ;

  bool mfln = client->probeMaxFragmentLength(configHost, 443, 1024);
  Serial.printf("Maximum fragment Length negotiation supported: %s\n", mfln ? "yes" : "no");
  if (mfln) {
    client->setBufferSizes(1024, 1024);
  }

  fingerprint2Hex();
  client->setFingerprint(sha1);
  if (debug) {
    Serial.print(F("Posting data: "));
    Serial.println(pathQuery);
  }

  if (http.begin(*client, configHost, httpsPort, pathQuery, true)) {
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    http.addHeader("User-Agent", "ESP8266HTTPClient");
    http.addHeader("Connection", "keep-alive");
    http.addHeader("Host", String(configHost + ":" + httpsPort));
    http.addHeader("Content-Length", String(pathQuery.length()));

    int  httpCode = http.GET();
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
  } else {
    Serial.printf("[HTTPS] Unable to connect\n");
  }
  Alarm.delay(10);
  http.end();
  client->flush();
  */

}

////// Miscellaneous functions

//// print variables for debug
void debugVars() {
  Serial.println(F("# DEBUG:"));
  Serial.print(F("- hostname: "));
  Serial.println(hostname);
  Serial.print(F("- LAN IP: "));
  Serial.println(lanIP);
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
  printNextAlarmTime();
}

//// transform SHA1 to hex format needed for setFingerprint (from aa:ab:etc. to 0xaa, 0xab, etc.)
void fingerprint2Hex() {
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

// URL-encode JSON
String urlEncode(String str)
{
  String encodedString="";
  char c;
  char code0;
  char code1;
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
      encodedString+='%';
      encodedString+=code0;
      encodedString+=code1;
    }
    yield();
  }
  return encodedString;
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

//// WiFi config mode info
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println(F("Opening configuration portal"));
  tft.fillScreen(TFT_BLACK); // Black screen fill
  tft.setTextDatum(TL_DATUM); // Use top left corner as text coord datum
  tft.setTextColor(TFT_YELLOW);
  tft.setFreeFont(Bold_FONT);
  tft.setTextSize(1);
  tft.drawString("WiFi Configuration", 5, 30);
  tft.setTextColor(TFT_BLUE);
  tft.drawString("IP:", 5, 60);
  tft.setTextColor(TFT_GREEN);
  tft.drawString("10.0.1.1", 25, 90);
  tft.setTextColor(TFT_BLUE);
  tft.drawString("SSID:", 5, 120);
  tft.setTextColor(TFT_GREEN);
  tft.drawString(hostname, 25, 150);
  tft.setTextColor(TFT_BLUE);
  tft.drawString("Password: ", 5, 180);
  tft.setTextColor(TFT_GREEN);
  tft.drawString("pass4esp", 25, 210);
}

// print / clear display status message
void statusPrint(const char *msg) {
  // Print something in the mini status bar
  tft.setTextPadding(240);
  //tft.setCursor(STATUS_X, STATUS_Y);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextFont(0);
  tft.setTextDatum(TC_DATUM);
  tft.setTextSize(1);
  tft.drawString(msg, STATUS_X, STATUS_Y);
  statusCleared = false;
  tft.setTextDatum(TL_DATUM);
}

void statusClear() {
  if (status_timer < (millis() - 2000)) {
    statusPrint("");
    statusCleared = true;
  }
}

// update the 3 display screens
void updateDisplayN() {
  // Update the normal display fields
  tft.setTextDatum(TL_DATUM); // Use top left corner as text coord datum
  sprintf(temp_short, "%.1f", temp_c);
  setTemp = temp_min+(temp_max-temp_min)/2;

  // display temp_c
  tft.setFreeFont(&FreeSans24pt7b);
  if (temp_c <= temp_min) 
    tft.setTextColor(TFT_BLUE);
  if (temp_c >= temp_max) 
    tft.setTextColor(0xFB21);
  if (temp_c > temp_min && temp_c < temp_max) 
    tft.setTextColor(TFT_YELLOW);
  tft.fillRect(DISP1_N_X - 1, DISP1_N_Y + 7, 100, 42, TFT_BLACK);
  tft.drawString(String(temp_short), DISP1_N_X + 4, DISP1_N_Y + 9);

  // display setTemp
  tft.setFreeFont(&FreeSans18pt7b);
  tft.setTextColor(TFT_CYAN);
  tft.fillRect(DISP2_N_X - 1, DISP2_N_Y + 7, 48, 33, TFT_BLACK);
  tft.drawString(String(setTemp), DISP2_N_X + 4, DISP2_N_Y + 9);

  tft.setTextSize(1);
  tft.setFreeFont(Small_FONT);
  String state = manual ? "manual" : "timer";
  wRelais = tft.drawString("Heating: ", 5, textLineY);
  tft.setFreeFont(&FreeSans12pt7b);
  if (state == "timer") {
    tft.setTextColor(TFT_BLACK);
    tft.drawString("MANUAL", wRelais + 5, textLineY);
    tft.setTextColor(TFT_GREEN);
    wState = tft.drawString("TIMER", wRelais + 5, textLineY);
    tft.setTextColor(TFT_CYAN);
    wComma = tft.drawString(", ", wRelais + wState + 5, textLineY);
  } else {
    tft.setTextColor(TFT_BLACK);
    tft.drawString("TIMER", wRelais + 5, textLineY);
    tft.setTextColor(0xFB21);
    wState = tft.drawString("MANUAL", wRelais + 5, textLineY);
    tft.setTextColor(TFT_CYAN);
    wComma = tft.drawString(", ", wRelais + wState + 5, textLineY);
  }
  if (relaisState == "ON") {
    tft.setTextColor(TFT_BLACK);
    tft.drawString(String("OFF"), wRelais + wState + wComma + 5, textLineY);
    tft.setTextColor(TFT_GREEN);
    tft.drawString(String(relaisState), wRelais + wState + wComma + 5, textLineY);
  }
  if (relaisState == "OFF") {
    tft.setTextColor(TFT_BLACK);
    tft.drawString(String("ON"), wRelais + wState + wComma + 5, textLineY);
    tft.setTextColor(0xFB21);
    tft.drawString(String(relaisState), wRelais + wState + wComma + 5, textLineY);
  }

  tft.setTextSize(1);
  tft.setFreeFont(Small_FONT);
  tft.setTextColor(TFT_CYAN);
  tft.drawString("Room temp", 27, 10);
  tft.drawString("Set", 165, 10);
  tft.drawString("WiFi: " + WiFi.SSID(), 5, textLineY + 30);
  tft.drawString("IP: " + String(lanIP), 5, textLineY + 60);

  tft.drawString(String("") + printDigits(hourP0) + ":" + printDigits(minuteP0), textLineX, textLineY + 90);
  int wP1 = tft.drawString(String(tempP0), 195, textLineY + 90);
  tft.drawCircle(221, textLineY + 93, 2, TFT_CYAN);
  tft.drawString("C", 224, textLineY + 90);
  tft.drawString(String("") + printDigits(hourP1) + ":" + printDigits(minuteP1), textLineX, textLineY + 120);
  int wP2 = tft.drawString(String(tempP1), 195, textLineY + 120);
  tft.drawCircle(221, textLineY + 123, 2, TFT_CYAN);
  tft.drawString("C", 224, textLineY + 120);
  tft.drawString(String("") + printDigits(hourP2) + ":" + printDigits(minuteP2), textLineX, textLineY + 150);
  int wP3 = tft.drawString(String(tempP2), 195, textLineY + 150);
  tft.drawCircle(221, textLineY + 153, 2, TFT_CYAN);
  tft.drawString("C", 224, textLineY + 150);

  drawClockFace(50, 220, 35);
}

void updateDisplayS() {
  // Update the setup display fields
  tft.setTextDatum(TL_DATUM); // Use top left corner as text coord datum
  tft.fillRect(DISP1_S_X + 4, DISP1_S_Y + 1, DISP1_S_W - 5, DISP1_S_H - 2, BUTTON_LABEL);
  tft.fillRect(DISP2_S_X + 4, DISP2_S_Y + 1, DISP2_S_W - 5, DISP2_S_H - 2, BUTTON_LABEL);
  tft.fillRect(DISP3_S_X + 4, DISP3_S_Y + 1, DISP3_S_W - 5, DISP3_S_H - 2, BUTTON_LABEL);

  tft.setFreeFont(&FreeSans18pt7b);
  tft.setTextColor(TFT_CYAN);  // Set the font color
  int xwidth1 = tft.drawString(String(temp_min), DISP1_S_X + 4, DISP1_S_Y + 9);
  tft.fillRect(DISP1_S_X + 4 + xwidth1, DISP1_S_Y + 1, DISP1_S_W - xwidth1 - 5, DISP1_S_H - 2, BUTTON_LABEL);
  int xwidth2 = tft.drawString(String(temp_max), DISP2_S_X + 4, DISP2_S_Y + 9);
  tft.fillRect(DISP2_S_X + 4 + xwidth2, DISP2_S_Y + 1, DISP2_S_W - xwidth2 - 5, DISP2_S_H - 2, BUTTON_LABEL);
  int xwidth3 = tft.drawString(String(interval/60000), DISP3_S_X + 4, DISP3_S_Y + 9);
  tft.fillRect(DISP3_S_X + 4 + xwidth3, DISP3_S_Y + 1, DISP3_S_W - xwidth3 - 5, DISP3_S_H - 2, BUTTON_LABEL);
}

void updateDisplayP() {
  // Update the schedule display fields
  tft.setTextDatum(TL_DATUM); // Use top left corner as text coord datum

  tft.setFreeFont(Small_FONT);
  tft.setTextColor(TFT_CYAN);  // Set the font color
  tft.setTextDatum(MC_DATUM); // Use middle centre as text coord datum
  tft.drawString("Scheduled timers", 120, 15);
  tft.setTextDatum(TL_DATUM); // Use top left corner as text coord datum

  tft.drawString("Timer 1:", 5, DISP_P_Y);
  tft.drawString("Timer 2:", 5, DISP_P_Y + 70);
  tft.drawString("Timer 3:", 5, DISP_P_Y + 140);

  tft.fillRect(DISP_P_X - 2, DISP_P_Y + 1, DISP_P_W, DISP_P_H, TFT_BLACK);
  tft.drawString(String(printDigits(hourP0)), DISP_P_X, DISP_P_Y);
  tft.drawString(":", DISP_P_X + 22, DISP_P_Y);
  tft.drawString(String(printDigits(minuteP0)), DISP_P_X + 27, DISP_P_Y);
  tft.fillRect(184, DISP_P_Y + 1, DISP_P_W, DISP_P_H, TFT_BLACK);
  tft.drawString(String(tempP0), 185, DISP_P_Y);
  tft.drawCircle(211, DISP_P_Y + 3, 2, TFT_CYAN);
  tft.drawString("C", 214, DISP_P_Y);

  tft.fillRect(DISP_P_X - 2, DISP_P_Y + 71, DISP_P_W, DISP_P_H, TFT_BLACK);
  tft.drawString(String(printDigits(hourP1)), DISP_P_X, DISP_P_Y + 70);
  tft.drawString(":", DISP_P_X + 22, DISP_P_Y + 70);
  tft.drawString(String(printDigits(minuteP1)), DISP_P_X + 27, DISP_P_Y + 70);
  tft.fillRect(184, DISP_P_Y + 71, DISP_P_W, DISP_P_H, TFT_BLACK);
  tft.drawString(String(tempP1), 185, DISP_P_Y + 70);
  tft.drawCircle(211, DISP_P_Y + 73, 2, TFT_CYAN);
  tft.drawString("C", 214, DISP_P_Y + 70);

  tft.fillRect(DISP_P_X - 2, DISP_P_Y + 141, DISP_P_W, DISP_P_H, TFT_BLACK);
  tft.drawString(String(printDigits(hourP2)), DISP_P_X, DISP_P_Y + 140);
  tft.drawString(":", DISP_P_X + 22, DISP_P_Y + 140);
  tft.drawString(String(printDigits(minuteP2)), DISP_P_X + 27, DISP_P_Y + 140);
  tft.fillRect(184, DISP_P_Y + 141, DISP_P_W, DISP_P_H, TFT_BLACK);
  tft.drawString(String(tempP2), 185, DISP_P_Y + 140);
  tft.drawCircle(211, DISP_P_Y + 143, 2, TFT_CYAN);
  tft.drawString("C", 214, DISP_P_Y + 140);

  drawClockFace(50, 270, 35);
}

// get time from NTP server and change according to local timezone
void getTime() {
  Serial.print(F("= getTime: "));
  // update the NTP client and get the UNIX UTC timestamp 
  timeClient.update();
  unsigned long epochTime =  timeClient.getEpochTime();
  // convert received time stamp to time_t object
  utc = epochTime;

  // Then convert the UTC UNIX timestamp to local time, Central European Time (Berlin, Paris)
  TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};     // Central European Summer Time
  TimeChangeRule CET = {"CET ", Last, Sun, Oct, 3, 60};       // Central European Standard Time
  Timezone CE(CEST, CET);
  local = CE.toLocal(utc);
  minuteNow = minute(local);
  hourNow = hour(local);

  timeNow = "";
  // now format the Time variables into strings with proper names for month, day etc
  date = days[weekday(local)-1];
  date += ", ";
  date += months[month(local)-1];
  date += " ";
  date += day(local);
  date += ", ";
  date += year(local);
  if(hourNow < 10)  // add a zero if hour is under 10
    timeNow += "0";
  timeNow += hourNow;
  timeNow += ":";
  if(minuteNow < 10)  // add a zero if minute is under 10
    timeNow += "0";
  timeNow += minuteNow;
  Serial.println(epochTime + " => " + String(date) + " - " + String(timeNow));
}

// print next alarm time to serial
void printNextAlarmTime() {
  TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};     // Central European Summer Time
  TimeChangeRule CET = {"CET ", Last, Sun, Oct, 3, 60};       // Central European Standard Time
  Timezone CE(CEST, CET);
  time_t localAlarmTrigger = Alarm.getNextTrigger();
  minuteNextAlarm = minute(localAlarmTrigger);
  hourNextAlarm = hour(localAlarmTrigger);

  String timeNextAlarm = "";
  timeNextAlarm = days[weekday(localAlarmTrigger)-1];
  timeNextAlarm += ", ";
  timeNextAlarm += months[month(localAlarmTrigger)-1];
  timeNextAlarm += " ";
  timeNextAlarm += day(localAlarmTrigger);
  timeNextAlarm += ", ";
  timeNextAlarm += year(localAlarmTrigger);
  timeNextAlarm += ", ";
  if(hourNextAlarm < 10)  // add a zero if hour is under 10
    timeNextAlarm += "0";
  timeNextAlarm += hourNextAlarm;
  timeNextAlarm += ":";
  if(minuteNextAlarm < 10)  // add a zero if minute is under 10
    timeNextAlarm += "0";
  timeNextAlarm += minuteNextAlarm;
  Serial.print(F("= NextAlarm: "));
  Serial.println(timeNextAlarm);
}

// update / clear the timers
void changeTimers(bool resetTimers = false) {
  Alarm.free(0);
  Alarm.free(1);
  Alarm.free(2);
  if (! resetTimers && ! manual) {
    Alarm.alarmRepeat(hourP0, minuteP0, 0, triggerTimerP1);
    Alarm.alarmRepeat(hourP1, minuteP1, 0, triggerTimerP2);
    Alarm.alarmRepeat(hourP2, minuteP2, 0, triggerTimerP3);
  }
  if (! program_screen && ! setup_screen)
  if (debug) {
    Serial.print(F("= Timers changed, active alarms: "));
    Serial.println(Alarm.count());
    printNextAlarmTime();
  }
}

// add leading 0s to times
String printDigits(int digit) {
  String digitS;
  if (digit < 10) {
    digitS = String("0") + String(digit);
  } else {
    digitS = String(digit);
  }
  return digitS;
}

// draw clock face and hands
void drawClockFace(int clockX,int clockY,int clockRadius) {
  tft.drawCircle(clockX,clockY,clockRadius + 1,COLOR_CLOCK); // clock face
  tft.drawCircle(clockX,clockY,clockRadius + 2,COLOR_CLOCK); // clock face
  for( int z=0; z < 360;z= z + 30 ){ // hour ticks
    //Begin at 0° and stop at 360°
    float angle = z ;
    angle=(angle/57.29577951) ; //Convert degrees to radians
    x2=(clockX+(sin(angle)*clockRadius));
    y2=(clockY-(cos(angle)*clockRadius));
    x3=(clockX+(sin(angle)*(clockRadius-2)));
    y3=(clockY-(cos(angle)*(clockRadius-2)));
    x6=(clockX+(sin(angle)*(clockRadius-6)));
    y6=(clockY-(cos(angle)*(clockRadius-6)));
    if (z == 0 || z == 90 || z == 180 || z == 270) {
      //tft.fillCircle(x3,y3,2,COLOR_CLOCK); // draw bigger dots on 3/12/9/6
      tft.drawLine(x2,y2,x6,y6,COLOR_CLOCK); // draw longer lines on 3/6/9/12
    } else {
      //tft.fillCircle(x6,y6,1,COLOR_CLOCK); // draw small dots
      tft.drawLine(x2,y2,x3,y3,COLOR_CLOCK); // draw short lines
    }
  }
}

void drawClockTime(int clockX,int clockY,int clockRadius) {
  float angle = minuteNow * 6 ;
  angle = (angle/57.29577951) ; //Convert degrees to radians
  x4=(clockX+(sin(angle)*(clockRadius-10)));
  y4=(clockY-(cos(angle)*(clockRadius-10)));
  if (minuteOld != minuteNow)
    tft.drawLine(clockX,clockY,x4_old,y4_old,TFT_BLACK);
  tft.drawLine(clockX,clockY,x4,y4,COLOR_CLOCK);
  angle = hourNow * 30 + int((minuteNow / 12) * 6 )   ;
  angle=(angle/57.29577951) ; //Convert degrees to radians
  x5=(clockX+(sin(angle)*(clockRadius-17)));
  y5=(clockY-(cos(angle)*(clockRadius-17)));
  if (minuteOld != minuteNow)
    tft.drawLine(clockX,clockY,x5_old,y5_old,TFT_BLACK);
  tft.drawLine(clockX,clockY,x5,y5,COLOR_CLOCK);

  minuteOld = minuteNow;
  hourOld = hourNow;
  x4_old = x4;
  y4_old = y4;
  x5_old = x5;
  y5_old = y5;
}

// execute / trigger timers actions
void triggerTimerP1() {
  temp_min = tempP0 -1;
  temp_max = tempP0 +1;
  autoSwitchRelais();
  if (! setup_screen && ! program_screen)
    updateDisplayN();
  if (debug)
    Serial.print(F("Triggered AlarmID="));
    Serial.println(Alarm.getTriggeredAlarmId());
}

void triggerTimerP2() {
  temp_min = tempP1 -1;
  temp_max = tempP1 +1;
  autoSwitchRelais();
  if (! setup_screen && ! program_screen)
    updateDisplayN();
  if (debug)
    Serial.print(F("Triggered AlarmID="));
    Serial.println(Alarm.getTriggeredAlarmId());
}

void triggerTimerP3() {
  temp_min = tempP2 -1;
  temp_max = tempP2 +1;
  autoSwitchRelais();
  if (! setup_screen && ! program_screen)
    updateDisplayN();
  if (debug)
    Serial.print(F("Triggered AlarmID="));
    Serial.println(Alarm.getTriggeredAlarmId());
}

//// local webserver handlers / send data to logserver
void handleRoot() {
  server.send(200, "text/html", "<html><head></head><body><div align=\"center\"><h1>Nothing to see here! Move along...</h1></div></body></html>\n");
  Alarm.delay(10);
}

void handleNotFound(){
  server.send(404, "text/plain", "404: File not found!\n");
  Alarm.delay(10);
}

void updateSettings() {
  Serial.println("\n= updateSettings");

  if (server.args() < 1 || server.args() > 19 || !server.arg("SHA1") || !server.arg("loghost")) {
    server.send(400, "text/html", "400: Invalid Request\n");
    return;
  }
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
  tempP0 = server.arg("tempP0").toInt();
  hourP0 = server.arg("hourP0").toInt();
  minuteP0 = server.arg("minuteP0").toInt();
  tempP1 = server.arg("tempP1").toInt();
  hourP1 = server.arg("hourP1").toInt();
  minuteP1 = server.arg("minuteP1").toInt();
  tempP2 = server.arg("tempP2").toInt();
  hourP2 = server.arg("hourP2").toInt();
  minuteP2 = server.arg("minuteP2").toInt();
  writeSettingsFile();
  server.send(200, "text/html", webString);
  status_timer = millis();
  statusPrint("Settings updated");
  Alarm.delay(10);
}

//------------------------------------------------------------------------------------------

void setup() {
  // Use serial port
  Serial.begin(115200);

  // Initialise the TFT screen
  tft.init();
  tft.setRotation(0);

  // from thermostat.ino
  DS18B20.begin();
  yield();
  pinMode(RELAISPIN, OUTPUT);

  WiFiManager wifiManager;
  wifiManager.setTimeout(300);
  wifiManager.setAPStaticIPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  wifiManager.setDebugOutput(false);
  wifiManager.setAPCallback(configModeCallback);
  if(!wifiManager.autoConnect("Donbot","pass4esp")) {
    Alarm.delay(1000);
    Serial.println(F("Failed to connect and hit timeout, restarting..."));
    ESP.reset();
  }
  Serial.print(F("Seconds in void setup() WiFi connection: "));
  int connRes = WiFi.waitForConnectResult();
  Serial.println(connRes);
  if (!MDNS.begin("esp8266"))
    Serial.println(F("Error setting up mDNS responder"));
  IPAddress ip = WiFi.localIP();
  sprintf(lanIP, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);

  getTime();
  setTime(local);

  if (!SPIFFS.begin() || FORMAT_SPIFFS) { // initialize SPIFFS
    Serial.println(F("Formating file system"));
    SPIFFS.format();
    SPIFFS.begin();
  }
  Serial.println("");
  Serial.println(F("SPIFFS started. Contents:"));
  Dir dir = SPIFFS.openDir("/");
  while (dir.next()) { // List the file system contents
    String fileName = dir.fileName();
    size_t fileSize = dir.fileSize();
    Serial.printf("\tFS File: %s, size: %s\r\n", fileName.c_str(), formatBytes(fileSize).c_str());
  }
  Serial.println("");

  // Calibrate the touch screen and retrieve the scaling factors
  uint16_t calData[5];
  uint8_t calDataOK = 0;

  // check if calibration file exists and size is correct
  if (SPIFFS.exists(CALIBRATION_FILE)) {
    if (REPEAT_CAL)
    {
      // Delete if we want to re-calibrate
      SPIFFS.remove(CALIBRATION_FILE);
    } else {
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
  if (interval < 10000) // set a failsafe interval
    interval = 60000; // 20 secs
  getTemperature();
  changeTimers();

  // local webserver client handlers
  server.onNotFound(handleNotFound);
  server.on("/", handleRoot);
  server.on("/clear", clearSpiffs);
  server.on("/update", []() {
    updateSettings();
    getTemperature();
    changeTimers();
    autoSwitchRelais();
    if (! setup_screen && ! program_screen)
      updateDisplayN();
    if (debug)
    debugVars();
  });
  server.begin();
  MDNS.addService("http", "tcp", 80);
  Serial.println("HTTP server started");
  debugVars();
} // setup() END

//------------------------------------------------------------------------------------------

void loop(void) {
  server.handleClient();
  MDNS.update();
  Alarm.delay(10);
  int pressed = 0;
  unsigned long presTime = millis();
  unsigned long passed = presTime - prevTime;

  if (presTime - clockTime > 60000) {
    getTime();
    clockTime = millis();
  }

  if (passed > interval) {
    Serial.println(F("\nInterval passed"));
    getTemperature();
    changeTimers();
    autoSwitchRelais();
    logToWebserver();
    prevTime = presTime; // save the last time
    if (! setup_screen && ! program_screen)
      updateDisplayN();
    if (debug)
      debugVars();
  } else {
    printProgress(passed * 100 / interval);
  }

  // auto exit setup screen after 20s
  if ((millis() >= (setupStarted + 10000)) && setup_screen) {
    setup_screen = false;
    program_screen = false;
    display_changed = true;
    statusPrint("Auto exit Setup screen");
    Alarm.delay(10);
  }

  // auto exit schedule screen after 20s
  if ((millis() >= (scheduleStarted + 10000)) && program_screen) {
    statusPrint("Auto exit Schedule screen");
    setup_screen = false;
    program_screen = false;
    display_changed = true;
    Alarm.delay(10);
  }

  if (! statusCleared)
    statusClear();
  
  // ------------ DISPLAY ------------ //

  // ----- SCHEDULE DISPLAY INIT ----- //
  if (display_changed && program_screen) {
    tft.fillScreen(TFT_BLACK);

    // draw grid
    //for (int32_t x=20; x<240; x=x+20) tft.drawLine(x, 0, x, 320, TFT_NAVY);
    //for (int32_t y=20; y<320; y=y+20) tft.drawLine(0, y, 240, y, TFT_NAVY);

    updateDisplayP();

    // Draw schedule keypads
    for (uint8_t row = 0; row < 3; row++) {
      for (uint8_t col = 0; col < 2; col++) {
        uint8_t b1 = col + row * 2;

        tft.setFreeFont(Bold_FONT);
        key[b1].initButton(&tft, KEY1_P_X + col * (KEY1_P_W + KEY1_P_SPACING_X),
                          KEY1_P_Y + row * (KEY1_P_H + KEY1_P_SPACING_Y), // x, y, w, h, outline, fill, text
                          KEY1_P_W, KEY1_P_H, TFT_WHITE, keyColorSchedule[b1], TFT_WHITE,
                          keyLabelSchedule[b1], 1);
        key[b1].drawButton();
        uint8_t b2 = b1 + 6;
        key[b2].initButton(&tft, KEY2_P_X + col * (KEY2_P_W + KEY2_P_SPACING_X),
                          KEY2_P_Y + row * (KEY2_P_H + KEY2_P_SPACING_Y), // x, y, w, h, outline, fill, text
                          KEY2_P_W, KEY2_P_H, TFT_WHITE, keyColorSchedule[b1], TFT_WHITE,
                          keyLabelSchedule[b1], 1);
        key[b2].drawButton();
      }
    }
    tft.setFreeFont(Italic_FONT);
    key[12].initButton(&tft, 198, 285, 80, 30, TFT_WHITE, TFT_DARKGREEN, TFT_WHITE, exitButton, 1);
    key[12].drawButton();
    display_changed = false;
  } // ----- SCHEDULE DISPLAY INIT END ----- //

  if (display_changed && setup_screen) {
    // ----- SETUP DISPLAY INIT ----- //
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(TL_DATUM); // Use top left corner as text coord datum

    tft.setTextSize(1);
    tft.setFreeFont(Italic_FONT);
    tft.setTextColor(TFT_CYAN);
    tft.drawString("Temp min", 6, 20);
    tft.fillRect(DISP1_S_X, DISP1_S_Y, DISP1_S_W, DISP1_S_H, BUTTON_LABEL);
    tft.drawRect(DISP1_S_X, DISP1_S_Y, DISP1_S_W, DISP1_S_H, TFT_WHITE);
    tft.drawString("Temp max", 6, 70);
    tft.fillRect(DISP2_S_X, DISP2_S_Y, DISP2_S_W, DISP2_S_H, BUTTON_LABEL);
    tft.drawRect(DISP2_S_X, DISP2_S_Y, DISP2_S_W, DISP2_S_H, TFT_WHITE);
    tft.drawString("Interval", 6, 120);
    tft.fillRect(DISP3_S_X, DISP3_S_Y, DISP3_S_W, DISP3_S_H, BUTTON_LABEL);
    tft.drawRect(DISP3_S_X, DISP3_S_Y, DISP3_S_W, DISP3_S_H, TFT_WHITE);

    updateDisplayS();

    // Draw setup keypad
    for (uint8_t row = 0; row < 4; row++) {
      for (uint8_t col = 0; col < 4; col++) {
        uint8_t b = col + row * 4;

        if (b < 4 || b > 11) tft.setFreeFont(Italic_FONT);
        else tft.setFreeFont(Bold_FONT);

        key[b].initButton(&tft, KEY_S_X + col * (KEY_S_W + KEY_S_SPACING_X),
                          KEY_S_Y + row * (KEY_S_H + KEY_S_SPACING_Y), // x, y, w, h, outline, fill, text
                          KEY_S_W, KEY_S_H, TFT_WHITE, keyColorSetup[b], TFT_WHITE,
                          keyLabelSetup[b], 1);
        key[b].drawButton();
      }
    }
    display_changed = false;
  } // ----- SETUP DISPLAY INIT END ----- //

  // ----- NORMAL DISPLAY INIT ----- //
  if (display_changed && ! setup_screen && ! program_screen) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(TL_DATUM); // Use top left corner as text coord datum

    updateDisplayN();

    // Draw keypad
    for (uint8_t row = 0; row < 1; row++) {
      for (uint8_t col = 0; col < 3; col++) {
        uint8_t b = col + row * 3;

        if (b == 2) tft.setFreeFont(Italic_FONT);
        else tft.setFreeFont(Bold_FONT);

        key[b].initButton(&tft, KEY_N_X + col * (KEY_N_W + KEY_N_SPACING_X),
                          KEY_N_Y + row * (KEY_N_H + KEY_N_SPACING_Y), // x, y, w, h, outline, fill, text
                          KEY_N_W, KEY_N_H, TFT_WHITE, keyColorDisplay[b], TFT_WHITE,
                          keyLabelDisplay[b], 1);
        key[b].drawButton();
      }
    }
    display_changed = false;
  } // ----- NORMAL DISPLAY INIT END ----- //

  // --- key was pressed --- //

  // ----- SCHEDULE DISPLAY ----- //
  if (program_screen) {
    drawClockTime(50, 270, 35);

    uint16_t t_x = 0, t_y = 0; // To store the touch coordinates
    pressed = tft.getTouch(&t_x, &t_y);

    // Check if any key was pressed
    for (uint8_t b = 0; b < 13; b++) {
      if (key[b].contains(t_x, t_y)) {
        key[b].press(true);  // tell the button it is pressed
      } else {
        key[b].press(false);  // tell the button it is NOT pressed
      }
    }

    for (uint8_t b= 0; b < 13; b++) {
      if (b == 12) tft.setFreeFont(Italic_FONT);
      else tft.setFreeFont(Bold_FONT);

      if (key[b].justReleased())
        key[b].drawButton();     // draw normal
      if (key[b].justPressed()) {
        key[b].drawButton(true);  // draw invert

        // change program times and temps
        // P1 -
        if (b == 0) {
          if (minuteP0 == 0) {
            minuteP0 = 30;
            if (hourP0 == 0) hourP0  = 23;
            else hourP0 --;
          } else {
            minuteP0 = 0;
          }
          scheduleStarted = millis();
          Alarm.delay(200);
        }
        // P1 +
        if (b == 1) {
          if (minuteP0 == 0) {
            minuteP0 = 30;
          } else {
            minuteP0 = 0;
            if (hourP0 == 23) {
              hourP0 = 0;
            } else {
              hourP0 ++;
            }
          }
          scheduleStarted = millis();
          Alarm.delay(200);
        }
        // P2 -
        if (b == 2) {
          if (minuteP1 == 0) {
            minuteP1 = 30;
            if (hourP1 == 0) hourP1  = 23;
            else hourP1 --;
          } else {
            minuteP1 = 0;
          }
          scheduleStarted = millis();
          Alarm.delay(200);
        }
        // P2 +
        if (b == 3) {
          if (minuteP1 == 0) {
            minuteP1 = 30;
          } else {
            minuteP1 = 0;
            if (hourP1 == 23) {
              hourP1 = 0;
            } else {
              hourP1 ++;
            }
          }
          scheduleStarted = millis();
          Alarm.delay(200);
        }
        // P3 -
        if (b == 4) {
          if (minuteP2 == 0) {
            minuteP2 = 30;
            if (hourP2 == 0) hourP2  = 23;
            else hourP2 --;
          } else {
            minuteP2 = 0;
          }
          scheduleStarted = millis();
          Alarm.delay(200);
        }
        // P3 +
        if (b == 5) {
          if (minuteP2 == 0) {
            minuteP2 = 30;
          } else {
            minuteP2 = 0;
            if (hourP2 == 23) {
              hourP2 = 0;
            } else {
              hourP2 ++;
            }
          }
          scheduleStarted = millis();
          Alarm.delay(200);
        }

        // P1 temp
        if (b == 6 && tempP0 > 18) {
          tempP0--;
          scheduleStarted = millis();
          Alarm.delay(200);
        }
        if (b == 7 && tempP0 < 32) {
          tempP0++;
          scheduleStarted = millis();
          Alarm.delay(200);
        }
        // P2 temp
        if (b == 8 && tempP1 > 18) {
          tempP1--;
          scheduleStarted = millis();
          Alarm.delay(200);
        }
        if (b == 9 && tempP1 < 32) {
          tempP1++;
          scheduleStarted = millis();
          Alarm.delay(200);
        }
        // P3 temp
        if (b == 10 && tempP2 > 18) {
          tempP2--;
          scheduleStarted = millis();
          Alarm.delay(200);
        }
        if (b == 11 && tempP2 < 32) {
          tempP2++;
          scheduleStarted = millis();
          Alarm.delay(200);
        }

        updateDisplayP();

        // Exit Schedule
        if (b == 12) {
          Serial.println(F("Exit Schedule screen"));
          setup_screen = false;
          program_screen = false;
          display_changed = true;
          changeTimers();
          statusPrint("Exit Schedule screen");
          Alarm.delay(200);
        }

        Alarm.delay(10); // UI debouncing
      }
    }
    pressed = 0;
  } // ----- SCHEDULE DISPLAY END ----- //

  // ----- SETUP DISPLAY ----- //
  if (setup_screen) {
    uint16_t t_x = 0, t_y = 0; // To store the touch coordinates
    pressed = tft.getTouch(&t_x, &t_y);

    // Check if any key was pressed
    for (uint8_t b = 4; b < 16; b++) {
      if (key[b].contains(t_x, t_y)) {
        key[b].press(true);  // tell the button it is pressed
      } else {
        key[b].press(false);  // tell the button it is NOT pressed
      }
    }

    for (uint8_t b = 4; b < 16; b++) {
      if (b > 11) tft.setFreeFont(Italic_FONT);
      else tft.setFreeFont(Bold_FONT);

      if (key[b].justReleased())
        key[b].drawButton();     // draw normal
      if (key[b].justPressed()) {
        key[b].drawButton(true);  // draw invert

        // assign action
        // 4/8 - first col
        if (b == 4) {
          manual = false;
          changeTimers();
          status_timer = millis();
          setupStarted = millis();
          statusPrint("Mode: auto, timers created");
          Alarm.delay(200);
        }
        if (b == 8) {
          manual = true;
          changeTimers(true);
          status_timer = millis();
          setupStarted = millis();
          statusPrint("Mode: manual, timers deleted");
          Alarm.delay(200);
        }

        // 5/9 - second col
        if (b == 5 && temp_min < 31) {
          temp_min++;
          status_timer = millis();
          setupStarted = millis();
          statusPrint("Increased temp min");
          Alarm.delay(200);
        }
        if (b == 9 && temp_min > 17) {
          temp_min--;
          status_timer = millis();
          setupStarted = millis();
          statusPrint("Decreased temp min");
          Alarm.delay(200);
        }

        // 6/10 - third col
        if (b == 6 && temp_max < 33) {
          temp_max++;
          status_timer = millis();
          setupStarted = millis();
          statusPrint("Increased temp max");
          Alarm.delay(200);
        }
        if (b == 10 && temp_max > 19) {
          temp_max--;
          status_timer = millis();
          setupStarted = millis();
          statusPrint("Decreased temp max");
          Alarm.delay(200);
        }

        // 7/11 - fourth col
        if (b == 7 && interval <= 840000) {
          interval = interval + 60000;
          status_timer = millis();
          setupStarted = millis();
          statusPrint("Increased interval");
          Alarm.delay(200);
        }
        if (b == 11 && interval >= 120000) {
          interval = interval - 60000;
          status_timer = millis();
          setupStarted = millis();
          statusPrint("Decreased interval");
          Alarm.delay(200);
        }

        // last row
        if (b == 12) {
          // Auto
          Serial.println(F("Enter Schedule screen"));
          setup_screen = false;
          program_screen = true;
          display_changed = true;
          scheduleStarted = millis();
          status_timer = millis();
          statusPrint("Enter Schedule screen");
          Alarm.delay(200);
        }
        if (b == 13) {
          // Reset settings
          temp_min = 24;
          temp_max = 26;
          interval = 300000;
          changeTimers();
          status_timer = millis();
          setupStarted = millis();
          statusPrint("Values reset");
          Alarm.delay(200);
        }
        if (b == 14) {
          // Save settings
          writeSettingsFile();
          writeSettingsWeb();
          setupStarted = millis();
          Alarm.delay(200);
        }

        updateDisplayS();

        if (b == 15) {
          // Exit Setup Screen
          Serial.println(F("Exit Setup screen"));
          setup_screen = false;
          program_screen = false;
          display_changed = true;
          statusPrint("Exit Setup screen");
          Alarm.delay(200);
        }

        Alarm.delay(10); // UI debouncing
      }
    }
    pressed = 0;
  }  // ----- SETUP DISPLAY END ----- //

  // ----- NORMAL DISPLAY ----- //
  if (! setup_screen && ! program_screen) {
    drawClockTime(50, 220, 35);

    uint16_t t_x = 0, t_y = 0; // To store the touch coordinates
    pressed = tft.getTouch(&t_x, &t_y);

    // Check if clock was touched
    if ((t_x > 5) && (t_x < 75)) {
      if ((t_y > 185) && (t_y < 255)) {
        Serial.println(F("Enter Schedule screen"));
        setup_screen = false;
        program_screen = true;
        display_changed = true;
        scheduleStarted = millis();
        status_timer = millis();
        statusPrint("Enter Schedule screen");
      }
    }
    Alarm.delay(10); // UI debouncing

    // Check if any key was pressed
    for (uint8_t b = 0; b < 3; b++) {
      if (key[b].contains(t_x, t_y)) {
        key[b].press(true);  // tell the button it is pressed
      } else {
        key[b].press(false);  // tell the button it is NOT pressed
      }
    }

    for (uint8_t b = 0; b < 3; b++) {
      if (b == 2) tft.setFreeFont(Italic_FONT);
      else tft.setFreeFont(Bold_FONT);

      if (key[b].justReleased())
        key[b].drawButton();     // draw normal
      if (key[b].justPressed()) {
        key[b].drawButton(true);  // draw invert

        // assign action
        // 0/1
        if (b == 0 && temp_min < 31 && temp_max < 33) {
            temp_min++;
            temp_max++;
            autoSwitchRelais();
            status_timer = millis();
            statusPrint("Temperature raised");
            Alarm.delay(200);
        }
        if (b == 1 && temp_min > 17 && temp_max > 19) {
            temp_min--;
            temp_max--;
            autoSwitchRelais();
            status_timer = millis();
            statusPrint("Temperature lowered");
            Alarm.delay(200);
        }

        updateDisplayN();

        // Enter Setup
        if (b == 2) {
          Serial.println(F("Enter Setup screen"));
          setup_screen = true;
          display_changed = true;
          setupStarted = millis();
          statusPrint("Enter Setup screen");
          Alarm.delay(200);
        }

        Alarm.delay(10); // UI debouncing
      }
    }
    pressed = 0;
  }  // ----- NORMAL DISPLAY END ----- //
}
