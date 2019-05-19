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

struct thermostatConfig {
  char SHA1[64];
  char loghost[32];
  long interval;
  bool heater;
  bool manual;
  bool debug;
  float temp_dev;
  int temp_min;
  int temp_max;
  int httpsPort;

  void load(JsonObjectConst);
  void save(JsonObject) const;
};

struct programsConfig {
  int hour;
  int minute;
  int temp;

  void load(JsonObjectConst);
  void save(JsonObject) const;
};

struct Config {
  thermostatConfig thermostat;

  static const int maxprograms = 3;
  programsConfig program[maxprograms];
  int programs = 0;

  void load(JsonObjectConst);
  void save(JsonObject) const;
};

bool serializeConfig(const Config &config, Print &dst);
bool deserializeConfig(String src, Config &config);

const size_t jsonCapacity = JSON_OBJECT_SIZE(19) + 300;
const char *sFile = "/settings.txt";  // SPIFFS file name must start with "/".
const static String configHost = "temperature.hugo.ro"; // chicken/egg situation, you have to get the initial config from somewhere
unsigned long blink;
unsigned long uptime = (millis() / 1000 );
unsigned long status_timer = millis();
unsigned long setupStarted = millis();
unsigned long scheduleStarted = millis();
unsigned long prevTime = 0;
unsigned long clockTime = 0;
time_t local, utc;
bool emptyFile = false;
bool success, loaded;
bool display_changed = true;
bool setup_screen = false;
bool program_screen = false;
bool statusCleared = true;
char buffer[3];
char lanIP[16];
char temp_short[8];
char exitButton [] = "Exit";
char relaisState[4];
String webString, epochTime;
String hostname = "Donbot";
uint8_t sha1[20];
float temp_c;
int wRelais, wState, wComma;
int setTemp; // config.thermostat.temp_min+(config.thermostat.temp_max-config.thermostat.temp_min)/2;
int textLineY = 92;
int textLineX = 135;
int pressed = 0;

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

Config config;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, NTP_OFFSET, NTP_INTERVAL);

// from thermostat.ino
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature DS18B20(&oneWire);
ESP8266WebServer server(80);
TFT_eSPI tft = TFT_eSPI(); // Invoke custom TFT library

//------------------------------------------------------------------------------------------

void thermostatConfig::save(JsonObject obj) const {
  obj["SHA1"] = SHA1;
  obj["loghost"] = loghost;
  obj["interval"] = interval;
  obj["heater"] = heater;
  obj["manual"] = manual;
  obj["debug"] = debug;
  obj["temp_dev"] = temp_dev;
  obj["temp_min"] = temp_min;
  obj["temp_max"] = temp_max;
  obj["httpsPort"] = httpsPort;
}

void programsConfig::save(JsonObject obj) const {
  obj["hour"] = hour;
  obj["minute"] = minute;
  obj["temp"] = temp;
}

void thermostatConfig::load(JsonObjectConst obj) {
  strlcpy(SHA1, obj["SHA1"], sizeof(SHA1));
  strlcpy(loghost, obj["loghost"], sizeof(loghost));
  interval = obj["interval"];
  heater = obj["heater"];
  manual = obj["manual"];
  debug = obj["debug"];
  temp_dev = obj["temp_dev"];
  temp_min = obj["temp_min"];
  temp_max = obj["temp_max"];
  httpsPort = obj["httpsPort"];
}

void programsConfig::load(JsonObjectConst obj) {
  hour = obj["hour"];
  minute = obj["minute"];
  temp = obj["temp"];
}

void Config::load(JsonObjectConst obj) {
  // Read "thermostat" object
  thermostat.load(obj["thermostat"]);

  // Get a reference to the programs array
  JsonArrayConst progs = obj["programs"];

  // Extract each program
  programs = 0;
  for (JsonObjectConst prog : progs) {
    // Load the program
    program[programs].load(prog);

    // Increment program count
    programs++;

    // Max reach?
    if (programs >= maxprograms)
      break;
  }
}

void Config::save(JsonObject obj) const {
  // Add "thermostat" object
  thermostat.save(obj.createNestedObject("thermostat"));

  // Add "programs" array
  JsonArray progs = obj.createNestedArray("programs");

  // Add each acces point in the array
  for (int i = 0; i < programs; i++)
    program[i].save(progs.createNestedObject());
}

bool serializeConfig(const Config &config, Print &dst) {
  Serial.print(F("= serializeConfig: "));
  DynamicJsonDocument doc(512);

  // Create an object at the root
  JsonObject root = doc.to<JsonObject>();

  // Fill the object
  config.save(root);

  if (config.thermostat.debug) {
    Serial.println("jsonPretty=");
    serializeJsonPretty(doc, Serial);
  }
  Serial.println("OK");
  // Serialize JSON to file
  return serializeJson(doc, dst) > 0;
}

bool deserializeConfig(String src, Config &config) {
  Serial.print(F("= deserializeConfig: "));
  DynamicJsonDocument doc(1024);

  // Parse the JSON object in the file
  DeserializationError err = deserializeJson(doc, src);
  if (err) {
    Serial.print(F("deserializeJson() failed with code "));
    Serial.println(err.c_str());
    return false;
  }

  config.load(doc.as<JsonObject>());
  Serial.println("OK");
  return true;
}

// Loads the configuration from a file on SPIFFS
bool loadFile() {
  Serial.print(F("= loadFile: "));
  File file = SPIFFS.open(sFile, "r");
  if (!file) {
    Serial.println(F("Failed to open config file"));
    return false;
  }

  String json="";
  while (file.available()) {
    json += (char)file.read();
  }

  // Parse the JSON object in the file
  success = deserializeConfig(json, config);
  if (!success) {
    Serial.println(F("Failed to deserialize configuration"));
    return false;
  }
  return true;
}

// Saves the configuration to a file on SPIFFS
void saveFile() {
  Serial.println(F("= saveFile"));
  File file = SPIFFS.open(sFile, "w");
  if (!file) {
    Serial.println(F("Failed to create config file"));
    return;
  }

  // Serialize JSON to file
  success = serializeConfig(config, file);
  if (!success) {
    Serial.println(F("Failed to serialize configuration"));
  }
  if (config.thermostat.debug)
    printFile();
}

// Prints the content of a file to the Serial
void printFile() {
  // Open file for reading
  File file = SPIFFS.open(sFile, "r");
  if (!file) {
    Serial.println(F("Failed to open config file"));
    return;
  }

  Serial.print("Settings JSON in SPIFFS file=");
  // Extract each by one by one
  while (file.available()) {
    Serial.print((char)file.read());
  }
  Serial.println();
}

//// read temperature from sensor / switch relay on or off
void getTemperature() {
  Serial.print(F("= getTemperature: "));
  float last_temp_c = temp_c;
  uptime = (millis() / 1000 ); // Refresh uptime
  DS18B20.requestTemperatures();  // initialize temperature sensor
  temp_c = float(DS18B20.getTempCByIndex(0)); // read sensor
  yield();
  if (temp_c < -120)
    temp_c = last_temp_c;
  temp_c = temp_c + config.thermostat.temp_dev; // calibrating sensor
  Serial.println(temp_c);
}

void switchRelais(const char* sw = "TOGGLE") { // if no parameter given, assume TOGGLE
  Serial.print(F("= switchRelais: "));
  if (sw == "TOGGLE") {
    if (digitalRead(RELAISPIN) == 1) {
      digitalWrite(RELAISPIN, 0);
      strcpy(relaisState, "OFF");
    } else {
      digitalWrite(RELAISPIN, 1);
      strcpy(relaisState, "ON");
    }
    Serial.println(relaisState);
    return;
  } else {
    if (sw == "ON") {
    digitalWrite(RELAISPIN, 0);
    strcpy(relaisState, "ON");
    } else if (sw == "OFF") {
      digitalWrite(RELAISPIN, 1);
      strcpy(relaisState, "OFF");
    }
    Serial.println(relaisState);
  }
}

void autoSwitchRelais() {
  Serial.print(F("= autoSwitchRelais: "));
  if (temp_c <= config.thermostat.temp_min) {
    switchRelais("ON");
  } else if (temp_c >= config.thermostat.temp_max) {
    switchRelais("OFF");
  }
  if (! setup_screen && ! program_screen)
    updateDisplayN();
}

//// SPIFFS settings read / write / clear
void clearSpiffs() {
  Serial.println(F("= clearSpiffs"));
  Serial.println(F("Please wait for SPIFFS to be formatted"));
  SPIFFS.format();
  yield();
  Serial.println(F("SPIFFS formatted"));
  emptyFile = true; // mark file as empty
  server.send(200, "text/plain", "200: OK, SPIFFS formatted, settings cleared\n");
}

bool readSettingsWeb() { // use plain http, as SHA1 fingerprint not known yet
  Serial.println(F("= readSettingsWeb"));
  Serial.print(F("Getting settings from http://"));
  Serial.println(configHost);
  WiFiClient client;
  HTTPClient http;
  if (http.begin(client, "http://" + configHost + "/settings-" + hostname + ".json")) {
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.GET();
    if (httpCode > 0) {
      Serial.printf("[HTTP] code: %d\n", httpCode);
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        Serial.print(F("Server response:"));
        Serial.println(http.getString());
        deserializeConfig(http.getString(), config);
      }
    } else {
      Serial.printf("[HTTP] GET failed, error: %d = %s\n", httpCode, http.errorToString(httpCode).c_str());
      return false;
    }
    http.end();
  } else {
    Serial.printf("[HTTPS] Unable to connect\n");
  }
  Alarm.delay(10);
  client.flush();
  yield();
  return true;
}

void writeSettingsWeb() {
  Serial.println(F("= writeSettingsWeb: "));

  HTTPClient https;
  BearSSL::WiFiClientSecure *client = new BearSSL::WiFiClientSecure ;
  bool mfln = client->probeMaxFragmentLength(configHost, 443, 1024);
  //Serial.printf("Maximum fragment Length negotiation supported: %s\n", mfln ? "yes" : "no");
  if (mfln)
    client->setBufferSizes(1024, 1024);
  fingerprint2Hex();
  client->setFingerprint(sha1);
  if (config.thermostat.debug) {
    Serial.print(F("POST data to https://"));
    Serial.print(configHost);
  }

  if (https.begin(*client, configHost, config.thermostat.httpsPort, "/index.php", true)) {
    https.addHeader("Content-Type", "application/x-www-form-urlencoded");
    https.addHeader("User-Agent", "ESP8266HTTPClient");
    //https.addHeader("Host", configHost + ":" + config.thermostat.httpsPort);
    https.addHeader("Connection", "close");
    String json = "";
    DynamicJsonDocument doc(512);
    JsonObject root = doc.to<JsonObject>();
    config.save(root);
    serializeJson(doc, json);
    int  httpCode = https.POST(String("") + "device=" + hostname + "&uploadJson=" + urlEncode(json));
    if (httpCode > 0) {
      Serial.printf("[HTTP] code: %d\n", httpCode);
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        if (config.thermostat.debug) {
          Serial.print(F("Server response:"));
          Serial.println(https.getString());
        }
      }
      status_timer = millis();
      statusCleared = false;
      statusPrint("Saved settings to webserver");
    } else {
      Serial.printf("[HTTP] POST failed, error: %d = %s\n", httpCode, https.errorToString(httpCode).c_str());
      status_timer = millis();
      statusPrint("ERROR saving to webserver");
    }
    https.end();
  } else {
    Serial.printf("[HTTPS] Unable to connect\n");
    status_timer = millis();
    statusPrint("ERROR saving to webserver");
  }
  Alarm.delay(10);
  client->flush();
  yield();
  if (config.thermostat.debug)
    debugVars();
}

void logToWebserver() {
  Serial.println(F("= logToWebserver"));

  // configure path + query for sending to logserver
  if (emptyFile) {
    Serial.println(F("Empty settings file, most likely this is the first run,"));
    Serial.println(F("or maybe you cleared it, NOT UPDATING logserver!"));
    return;
  }

  HTTPClient https;
  BearSSL::WiFiClientSecure *client = new BearSSL::WiFiClientSecure ;
  bool mfln = client->probeMaxFragmentLength(config.thermostat.loghost, 443, 1024);
  //Serial.printf("Maximum fragment Length negotiation supported: %s\n", mfln ? "yes" : "no");
  if (mfln)
    client->setBufferSizes(1024, 1024);
  fingerprint2Hex();
  client->setFingerprint(sha1);
  if (config.thermostat.debug) {
    Serial.print(F("GET data: https://"));
    Serial.print(config.thermostat.loghost);
  }

  if (https.begin(*client, config.thermostat.loghost, config.thermostat.httpsPort, String("") + "/logtemp.php?&status=" + relaisState + "&temperature=" + temp_c +
  "&hostname=" + hostname + "&temp_min=" + config.thermostat.temp_min + "&temp_max=" + config.thermostat.temp_max + "&temp_dev=" + config.thermostat.temp_dev +
  "&interval=" + config.thermostat.interval + "&heater=" + config.thermostat.heater + "&manual=" + config.thermostat.manual, true)) {
    https.addHeader("Content-Type", "application/x-www-form-urlencoded");
    https.addHeader("User-Agent", "ESP8266HTTPClient");
    //https.addHeader("Host", config.thermostat.loghost + ":" + config.thermostat.httpsPort);
    https.addHeader("Connection", "close");

    int  httpCode = https.GET();
    if (httpCode > 0) {
      Serial.printf("[HTTP] code: %d\n", httpCode);
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        if (config.thermostat.debug) {
          Serial.print(F("Server response:"));
          Serial.println(https.getString());
        }
      }
      status_timer = millis();
      statusCleared = false;
      statusDot(TFT_DARKGREEN);
    } else {
      Serial.printf("[HTTP] GET failed, error: %d = %s\n", httpCode, https.errorToString(httpCode).c_str());
      status_timer = millis();
      statusDot(TFT_RED);
    }
    https.end();
  } else {
    Serial.print(F("[HTTPS] Unable to connect\n"));
    status_timer = millis();
    statusDot(TFT_RED);
  }
  Alarm.delay(10);
  client->flush();
  yield();
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
  Serial.println(config.thermostat.heater);
  Serial.print(F("- manual: "));
  Serial.println(config.thermostat.manual);
  if (emptyFile) {
    Serial.print(F("- emptyFile: "));
    Serial.println(emptyFile);
    return;
  }
  Serial.print(F("- SHA1: "));
  Serial.println(config.thermostat.SHA1);
  Serial.print(F("- loghost: "));
  Serial.println(config.thermostat.loghost);
  Serial.print(F("- httpsPort: "));
  Serial.println(config.thermostat.httpsPort);
  Serial.print(F("- interval: "));
  Serial.println(config.thermostat.interval);
  Serial.print(F("- temp_min: "));
  Serial.println(config.thermostat.temp_min);
  Serial.print(F("- temp_max: "));
  Serial.println(config.thermostat.temp_max);
  Serial.print(F("- temp_dev: "));
  Serial.println(config.thermostat.temp_dev);
  Serial.print(F("- hourP0: "));
  Serial.println(config.program[0].hour);
  Serial.print(F("- minuteP0: "));
  Serial.println(config.program[0].minute);
  Serial.print(F("- tempP0: "));
  Serial.println(config.program[0].temp);
  Serial.print(F("- hourP1: "));
  Serial.println(config.program[1].hour);
  Serial.print(F("- minuteP1: "));
  Serial.println(config.program[1].minute);
  Serial.print(F("- tempP1: "));
  Serial.println(config.program[1].temp);
  Serial.print(F("- hourP2: "));
  Serial.println(config.program[2].hour);
  Serial.print(F("- minuteP2: "));
  Serial.println(config.program[2].minute);
  Serial.print(F("- tempP2: "));
  Serial.println(config.program[2].temp);
  Serial.print(F("- MEM free heap: \033[01;91m"));
  Serial.println(system_get_free_heap_size());
  Serial.print(F("\033[00m"));
  printNextAlarmTime();
}

//// transform SHA1 to hex format needed for setFingerprint (from aa:ab:etc. to 0xaa, 0xab, etc.)
void fingerprint2Hex() {
  int j = 0;
  for (int i = 0; i < 60; i = i + 3) {
    String x = ("0x" + String(config.thermostat.SHA1).substring(i, i+2));
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
  tft.setTextPadding(210);
  //tft.setCursor(STATUS_X, STATUS_Y);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextFont(0);
  tft.setTextDatum(TC_DATUM);
  tft.setTextSize(1);
  tft.drawString(msg, STATUS_X, STATUS_Y);
  statusCleared = false;
  tft.setTextDatum(TL_DATUM);
}

void statusDot(uint16_t color) {
  tft.fillCircle(10, 310, 5, color);
}

void statusClear() {
  if (status_timer < (millis() - 3000)) {
    statusPrint("");
    statusCleared = true;
    tft.fillCircle(10, 310, 6, TFT_BLACK);
  }
}

// update the 3 display screens
void updateDisplayN() {
  Serial.print(F("- MEM free heap: "));
  tft.setTextDatum(TL_DATUM); // Use top left corner as text coord datum
  sprintf(temp_short, "%.1f", temp_c);
  setTemp = config.thermostat.temp_min+(config.thermostat.temp_max-config.thermostat.temp_min)/2;

  // display temp_c
  tft.setFreeFont(&FreeSans24pt7b);
  if (temp_c <= config.thermostat.temp_min) 
    tft.setTextColor(TFT_BLUE);
  if (temp_c >= config.thermostat.temp_max) 
    tft.setTextColor(0xFB21);
  if (temp_c > config.thermostat.temp_min && temp_c < config.thermostat.temp_max) 
    tft.setTextColor(TFT_YELLOW);
  tft.fillRect(DISP1_N_X - 1, DISP1_N_Y + 7, 100, 42, TFT_BLACK);
  tft.drawString(temp_short, DISP1_N_X + 4, DISP1_N_Y + 9);

  // display setTemp
  tft.setFreeFont(&FreeSans18pt7b);
  tft.setTextColor(TFT_CYAN);
  tft.fillRect(DISP2_N_X - 1, DISP2_N_Y + 7, 48, 33, TFT_BLACK);
  tft.drawString(String(setTemp), DISP2_N_X + 4, DISP2_N_Y + 9);

  tft.setTextSize(1);
  tft.setFreeFont(Small_FONT);
  wRelais = tft.drawString("Heating: ", 5, textLineY);
  tft.fillRect(wRelais + 4, textLineY - 1, 162, 27, TFT_BLACK);
  tft.setFreeFont(&FreeSans12pt7b);
  if (!config.thermostat.manual) {
    tft.setTextColor(TFT_GREEN);
    wState = tft.drawString("TIMER", wRelais + 5, textLineY);
    tft.setTextColor(TFT_CYAN);
    wComma = tft.drawString(", ", wRelais + wState + 5, textLineY);
  } else {
    tft.setTextColor(0xFB21);
    wState = tft.drawString("MANUAL", wRelais + 5, textLineY);
    tft.setTextColor(TFT_CYAN);
    wComma = tft.drawString(", ", wRelais + wState + 5, textLineY);
  }
  if (strcmp(relaisState, "ON")) {
    tft.setTextColor(TFT_GREEN);
    tft.drawString(relaisState, wRelais + wState + wComma + 5, textLineY);
  }
  if (strcmp(relaisState, "OFF")) {
    tft.setTextColor(0xFB21);
    tft.drawString(relaisState, wRelais + wState + wComma + 5, textLineY);
  }

  tft.setTextSize(1);
  tft.setFreeFont(Small_FONT);
  tft.setTextColor(TFT_CYAN);
  tft.drawString("Room temp", 27, 10);
  tft.drawString("Set", 165, 10);
  tft.drawString("WiFi: " + WiFi.SSID(), 5, textLineY + 30);
  tft.drawString("IP: " + String(lanIP), 5, textLineY + 60);

  tft.fillRect(textLineX, textLineY + 89, 100 , 83, TFT_BLACK);
  tft.drawString(String("") + printDigits(config.program[0].hour) + ":" + printDigits(config.program[0].minute), textLineX, textLineY + 90);
  int wP1 = tft.drawString(String(config.program[0].temp), 195, textLineY + 90);
  tft.drawCircle(221, textLineY + 93, 2, TFT_CYAN);
  tft.drawString("C", 224, textLineY + 90);
  tft.drawString(String("") + printDigits(config.program[1].hour) + ":" + printDigits(config.program[1].minute), textLineX, textLineY + 120);
  int wP2 = tft.drawString(String(config.program[1].temp), 195, textLineY + 120);
  tft.drawCircle(221, textLineY + 123, 2, TFT_CYAN);
  tft.drawString("C", 224, textLineY + 120);
  tft.drawString(String("") + printDigits(config.program[2].hour) + ":" + printDigits(config.program[2].minute), textLineX, textLineY + 150);
  int wP3 = tft.drawString(String(config.program[2].temp), 195, textLineY + 150);
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
  int xwidth1 = tft.drawString(String(config.thermostat.temp_min), DISP1_S_X + 4, DISP1_S_Y + 9);
  tft.fillRect(DISP1_S_X + 4 + xwidth1, DISP1_S_Y + 1, DISP1_S_W - xwidth1 - 5, DISP1_S_H - 2, BUTTON_LABEL);
  int xwidth2 = tft.drawString(String(config.thermostat.temp_max), DISP2_S_X + 4, DISP2_S_Y + 9);
  tft.fillRect(DISP2_S_X + 4 + xwidth2, DISP2_S_Y + 1, DISP2_S_W - xwidth2 - 5, DISP2_S_H - 2, BUTTON_LABEL);
  int xwidth3 = tft.drawString(String(config.thermostat.interval/60000), DISP3_S_X + 4, DISP3_S_Y + 9);
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
  tft.drawString(String(printDigits(config.program[0].hour)), DISP_P_X, DISP_P_Y);
  tft.drawString(":", DISP_P_X + 22, DISP_P_Y);
  tft.drawString(String(printDigits(config.program[0].minute)), DISP_P_X + 27, DISP_P_Y);
  tft.fillRect(184, DISP_P_Y + 1, DISP_P_W, DISP_P_H, TFT_BLACK);
  tft.drawString(String(config.program[0].temp), 185, DISP_P_Y);
  tft.drawCircle(211, DISP_P_Y + 3, 2, TFT_CYAN);
  tft.drawString("C", 214, DISP_P_Y);

  tft.fillRect(DISP_P_X - 2, DISP_P_Y + 71, DISP_P_W, DISP_P_H, TFT_BLACK);
  tft.drawString(String(printDigits(config.program[1].hour)), DISP_P_X, DISP_P_Y + 70);
  tft.drawString(":", DISP_P_X + 22, DISP_P_Y + 70);
  tft.drawString(String(printDigits(config.program[1].minute)), DISP_P_X + 27, DISP_P_Y + 70);
  tft.fillRect(184, DISP_P_Y + 71, DISP_P_W, DISP_P_H, TFT_BLACK);
  tft.drawString(String(config.program[1].temp), 185, DISP_P_Y + 70);
  tft.drawCircle(211, DISP_P_Y + 73, 2, TFT_CYAN);
  tft.drawString("C", 214, DISP_P_Y + 70);

  tft.fillRect(DISP_P_X - 2, DISP_P_Y + 141, DISP_P_W, DISP_P_H, TFT_BLACK);
  tft.drawString(String(printDigits(config.program[2].hour)), DISP_P_X, DISP_P_Y + 140);
  tft.drawString(":", DISP_P_X + 22, DISP_P_Y + 140);
  tft.drawString(String(printDigits(config.program[2].minute)), DISP_P_X + 27, DISP_P_Y + 140);
  tft.fillRect(184, DISP_P_Y + 141, DISP_P_W, DISP_P_H, TFT_BLACK);
  tft.drawString(String(config.program[2].temp), 185, DISP_P_Y + 140);
  tft.drawCircle(211, DISP_P_Y + 143, 2, TFT_CYAN);
  tft.drawString("C", 214, DISP_P_Y + 140);

  drawClockFace(50, 260, 35);
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
  setTime(local);

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
  Serial.print(epochTime);
  Serial.print(F(" => "));
  Serial.print(date);
  Serial.print(F(" - "));
  Serial.println(timeNow);
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
  if (! resetTimers && ! config.thermostat.manual) {
    Alarm.alarmRepeat(config.program[0].hour, config.program[0].minute, 0, triggerTimerP1);
    Alarm.alarmRepeat(config.program[1].hour, config.program[1].minute, 0, triggerTimerP2);
    Alarm.alarmRepeat(config.program[2].hour, config.program[2].minute, 0, triggerTimerP3);
  }
  if (! program_screen && ! setup_screen)
  if (config.thermostat.debug) {
    Serial.print(F("= Timers changed, active alarms: "));
    Serial.println(Alarm.count());
    printNextAlarmTime();
  }
}

// add leading 0s to times
char* printDigits(int digit) {
  if (digit < 10) {
    sprintf(buffer, "0%d", digit);
  } else {
    sprintf(buffer, "%d", digit);
  }
  return buffer;
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
  config.thermostat.temp_min = config.program[0].temp - 1;
  config.thermostat.temp_max = config.program[0].temp + 1;
  autoSwitchRelais();
  if (! setup_screen && ! program_screen)
    updateDisplayN();
  if (config.thermostat.debug)
    Serial.print(F("Triggered AlarmID="));
    Serial.println(Alarm.getTriggeredAlarmId());
}

void triggerTimerP2() {
  config.thermostat.temp_min = config.program[1].temp - 1;
  config.thermostat.temp_max = config.program[1].temp + 1;
  autoSwitchRelais();
  if (! setup_screen && ! program_screen)
    updateDisplayN();
  if (config.thermostat.debug)
    Serial.print(F("Triggered AlarmID="));
    Serial.println(Alarm.getTriggeredAlarmId());
}

void triggerTimerP3() {
  config.thermostat.temp_min = config.program[2].temp - 1;
  config.thermostat.temp_max = config.program[2].temp + 1;
  autoSwitchRelais();
  if (! setup_screen && ! program_screen)
    updateDisplayN();
  if (config.thermostat.debug)
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

void updateSettingsWebform() {
  Serial.println(F("\n= updateSettingsWebform"));

  if (server.args() < 1 || server.args() > 19 || !server.arg("SHA1") || !server.arg("loghost")) {
    server.send(400, "text/html", "400: Invalid Request\n");
    return;
  }
  strcpy(config.thermostat.SHA1, server.arg("SHA1").c_str());
  strcpy(config.thermostat.loghost, server.arg("loghost").c_str());
  config.thermostat.httpsPort = server.arg("httpsPort").toInt();
  config.thermostat.interval = server.arg("interval").toInt();
  config.thermostat.temp_min = server.arg("temp_min").toInt();
  config.thermostat.temp_max = server.arg("temp_max").toInt();
  config.thermostat.temp_dev = server.arg("temp_dev").toFloat();
  config.thermostat.heater = true;
  if (server.arg("manual") == "true" || server.arg("manual") == "1") {
    config.thermostat.manual = true;
  } else {
    config.thermostat.manual = false;
  }
  if (server.arg("debug") == "true" || server.arg("debug") == "1") {
    config.thermostat.debug = true;
  } else {
    config.thermostat.debug = false;
  }
  config.program[0].temp = server.arg("tempP0").toInt();
  config.program[0].hour = server.arg("hourP0").toInt();
  config.program[0].minute = server.arg("minuteP0").toInt();
  config.program[1].temp = server.arg("tempP1").toInt();
  config.program[1].hour = server.arg("hourP1").toInt();
  config.program[1].minute = server.arg("minuteP1").toInt();
  config.program[2].temp = server.arg("tempP2").toInt();
  config.program[2].hour = server.arg("hourP2").toInt();
  config.program[2].minute = server.arg("minuteP2").toInt();
  saveFile();
  String json = "";
  DynamicJsonDocument doc(512);
  JsonObject root = doc.to<JsonObject>();
  config.save(root);
  serializeJson(doc, json);
  server.send(200, "text/html", "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\">\n<head>\n<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\" />\n<style>\n\tbody { \n\t\tpadding: 3rem; \n\t\tfont-size: 16px;\n\t}\n\tform { \n\t\tdisplay: inline; \n\t}\n</style>\n</head>\n<body>\nArduino response: Code 200, OK.\n<br>Back to \n<form method='POST' action='https://temperature.hugo.ro'>\n\t<button name='device' value='" + String(hostname) + "'>Graph</button></form>\n<br>\nJSON root: \n<br>\n<div id='debug'></div>\n<script src='https://temperature.hugo.ro/prettyprint.js'></script>\n<script>\n\tvar root = " + json + ";\n\tvar tbl = prettyPrint(root);\n\tdocument.getElementById('debug').appendChild(tbl);\n</script>\n</body>\n");
  status_timer = millis();
  statusPrint("Settings updated from webform");
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

  if (!SPIFFS.begin() || FORMAT_SPIFFS) { // initialize SPIFFS
    Serial.println(F("Formating file system"));
    SPIFFS.format();
    SPIFFS.begin();
  }
  Serial.println(F("\nSPIFFS started. Contents:"));
  Dir dir = SPIFFS.openDir("/");
  while (dir.next()) { // List the file system contents
    Serial.printf("\tFS File: %s, size: %d\r\n", dir.fileName().c_str(), dir.fileSize());
  }

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

  if (!readSettingsWeb()) { // first, try reading settings from webserver
    if (!loadFile()) { // if failed, read settings from SPIFFS
      Serial.println(F("Using default config"));
      strcpy(config.thermostat.SHA1, "B0:4E:57:48:5A:97:45:BB:E2:EC:48:32:8B:ED:11:43:9F:BD:1A:F3");
      strcpy(config.thermostat.loghost, "192.168.178.25");
      config.thermostat.interval = 600000;
      config.thermostat.heater = true;
      config.thermostat.manual = false;
      config.thermostat.debug = true;
      config.thermostat.temp_dev = -0.5;
      config.thermostat.temp_min = 24;
      config.thermostat.temp_max = 26;
      config.thermostat.httpsPort = 443;
      config.program[0].hour = 6;
      config.program[0].minute = 30;
      config.program[0].temp = 23;
      config.program[1].hour = 9;
      config.program[1].minute = 30;
      config.program[1].temp = 25;
      config.program[2].hour = 21;
      config.program[2].minute = 30;
      config.program[2].temp = 22;
      config.programs = 3;
    }
  }
  getTemperature();
  getTime();
  setTime(local);
  changeTimers();

  // local webserver client handlers
  server.onNotFound(handleNotFound);
  server.on("/", handleRoot);
  server.on("/clear", clearSpiffs);
  server.on("/update", []() {
    updateSettingsWebform();
    getTemperature();
    changeTimers();
    autoSwitchRelais();
    if (! setup_screen && ! program_screen)
      updateDisplayN();
    if (config.thermostat.debug)
    debugVars();
  });
  server.begin();
  MDNS.addService("http", "tcp", 80);
  Serial.println("HTTP server started");
  debugVars();
} // setup() END

//------------------------------------------------------------------------------------------

void loop(void) {
  if (millis() - blink >= 700)
    if (system_get_free_heap_size() <= 11000) {
      tft.fillCircle(230, 310, 2, TFT_RED);
    } else {
      tft.fillCircle(230, 310, 2, TFT_BLUE);
    }
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

  if (passed > config.thermostat.interval) {
    if (system_get_free_heap_size() < 10000) {
      tft.fillScreen(TFT_RED);
      Alarm.delay(5000);
      ESP.reset();
    }
    Serial.println(F("\nInterval passed"));
    getTemperature();
    autoSwitchRelais();
    logToWebserver();
    prevTime = presTime; // save the last time
    if (! setup_screen && ! program_screen)
      updateDisplayN();
    if (config.thermostat.debug)
      debugVars();
  } else {
    printProgress(passed * 100 / config.thermostat.interval);
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
  if (millis() - blink >= 1000) {
    tft.fillCircle(230, 310, 3, TFT_BLACK);
    blink = millis();
  }
  
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
    drawClockTime(50, 260, 35);

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
          if (config.program[0].minute == 0) {
            config.program[0].minute = 30;
            if (config.program[0].hour == 0) config.program[0].hour  = 23;
            else config.program[0].hour--;
          } else {
            config.program[0].minute = 0;
          }
          scheduleStarted = millis();
          Alarm.delay(200);
        }
        // P1 +
        if (b == 1) {
          if (config.program[0].minute == 0) {
            config.program[0].minute = 30;
          } else {
            config.program[0].minute = 0;
            if (config.program[0].hour == 23) {
              config.program[0].hour = 0;
            } else {
              config.program[0].hour++;
            }
          }
          scheduleStarted = millis();
          Alarm.delay(200);
        }
        // P2 -
        if (b == 2) {
          if (config.program[1].minute == 0) {
            config.program[1].minute = 30;
            if (config.program[1].hour == 0) config.program[1].hour  = 23;
            else config.program[1].hour--;
          } else {
            config.program[1].minute = 0;
          }
          scheduleStarted = millis();
          Alarm.delay(200);
        }
        // P2 +
        if (b == 3) {
          if (config.program[1].minute == 0) {
            config.program[1].minute = 30;
          } else {
            config.program[1].minute = 0;
            if (config.program[1].hour == 23) {
              config.program[1].hour = 0;
            } else {
              config.program[1].hour++;
            }
          }
          scheduleStarted = millis();
          Alarm.delay(200);
        }
        // P3 -
        if (b == 4) {
          if (config.program[2].minute == 0) {
            config.program[2].minute = 30;
            if (config.program[2].hour == 0) config.program[2].hour  = 23;
            else config.program[2].hour --;
          } else {
            config.program[2].minute = 0;
          }
          scheduleStarted = millis();
          Alarm.delay(200);
        }
        // P3 +
        if (b == 5) {
          if (config.program[2].minute == 0) {
            config.program[2].minute = 30;
          } else {
            config.program[2].minute = 0;
            if (config.program[2].hour == 23) {
              config.program[2].hour = 0;
            } else {
              config.program[2].hour ++;
            }
          }
          scheduleStarted = millis();
          Alarm.delay(200);
        }

        // P1 temp
        if (b == 6 && config.program[0].temp > 18) {
          config.program[0].temp--;
          scheduleStarted = millis();
          Alarm.delay(200);
        }
        if (b == 7 && config.program[0].temp < 32) {
          config.program[0].temp++;
          scheduleStarted = millis();
          Alarm.delay(200);
        }
        // P2 temp
        if (b == 8 && config.program[1].temp > 18) {
          config.program[1].temp--;
          scheduleStarted = millis();
          Alarm.delay(200);
        }
        if (b == 9 && config.program[1].temp < 32) {
          config.program[1].temp++;
          scheduleStarted = millis();
          Alarm.delay(200);
        }
        // P3 temp
        if (b == 10 && config.program[2].temp > 18) {
          config.program[2].temp--;
          scheduleStarted = millis();
          Alarm.delay(200);
        }
        if (b == 11 && config.program[2].temp < 32) {
          config.program[2].temp++;
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
          config.thermostat.manual = false;
          changeTimers();
          status_timer = millis();
          setupStarted = millis();
          statusPrint("Mode: auto, timers created");
          Alarm.delay(200);
        }
        if (b == 8) {
          config.thermostat.manual = true;
          changeTimers(true);
          status_timer = millis();
          setupStarted = millis();
          statusPrint("Mode: manual, timers deleted");
          Alarm.delay(200);
        }

        // 5/9 - second col
        if (b == 5 && config.thermostat.temp_min < 31) {
          config.thermostat.temp_min++;
          status_timer = millis();
          setupStarted = millis();
          statusPrint("Increased temp min");
          Alarm.delay(200);
        }
        if (b == 9 && config.thermostat.temp_min > 17) {
          config.thermostat.temp_min--;
          status_timer = millis();
          setupStarted = millis();
          statusPrint("Decreased temp min");
          Alarm.delay(200);
        }

        // 6/10 - third col
        if (b == 6 && config.thermostat.temp_max < 33) {
          config.thermostat.temp_max++;
          status_timer = millis();
          setupStarted = millis();
          statusPrint("Increased temp max");
          Alarm.delay(200);
        }
        if (b == 10 && config.thermostat.temp_max > 19) {
          config.thermostat.temp_max--;
          status_timer = millis();
          setupStarted = millis();
          statusPrint("Decreased temp max");
          Alarm.delay(200);
        }

        // 7/11 - fourth col
        if (b == 7 && config.thermostat.interval <= 840000) {
          config.thermostat.interval = config.thermostat.interval + 60000;
          status_timer = millis();
          setupStarted = millis();
          statusPrint("Increased interval");
          Alarm.delay(200);
        }
        if (b == 11 && config.thermostat.interval >= 120000) {
          config.thermostat.interval = config.thermostat.interval - 60000;
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
          config.thermostat.temp_min = 24;
          config.thermostat.temp_max = 26;
          config.thermostat.interval = 600000;
          changeTimers();
          status_timer = millis();
          setupStarted = millis();
          statusPrint("Values reset");
          Alarm.delay(200);
        }
        if (b == 14) {
          // Save settings
          saveFile();
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
        if (b == 0 && config.thermostat.temp_min < 31 && config.thermostat.temp_max < 33) {
            config.thermostat.temp_min++;
            config.thermostat.temp_max++;
            autoSwitchRelais();
            status_timer = millis();
            statusPrint("Temperature raised");
            Alarm.delay(200);
        }
        if (b == 1 && config.thermostat.temp_min > 17 && config.thermostat.temp_max > 19) {
            config.thermostat.temp_min--;
            config.thermostat.temp_max--;
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
