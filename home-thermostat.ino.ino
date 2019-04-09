/*
  The TFT_eSPI library incorporates an Adafruit_GFX compatible
  button handling class, this sketch is based on the Arduin-o-phone
  example.

  This example diplays a keypad where numbers can be entered and
  send to the Serial Monitor window.

  The sketch has been tested on the ESP8266 (which supports SPIFFS)

  The minimum screen size is 320 x 240 as that is the keypad size.

  TOUCH_CS and SPI_TOUCH_FREQUENCY must be defined in the User_Setup.h file
  for the touch functions to do anything.
*/

// The SPIFFS (FLASH filing system) is used to hold touch screen
// calibration data

#include "FS.h"

#include <SPI.h>
#include <TFT_eSPI.h>      // Hardware-specific library

TFT_eSPI tft = TFT_eSPI(); // Invoke custom library

// This is the file name used to store the calibration data
// You can change this to create new calibration files.
// The SPIFFS file name must start with "/".
#define CALIBRATION_FILE "/TouchCalData2"

// Set REPEAT_CAL to true instead of false to run calibration
// again, otherwise it will only be done once.
// Repeat calibration if you change the screen rotation.
#define REPEAT_CAL false

// Using two fonts since numbers are nice when bold
#define LABEL1_FONT &FreeSansOblique12pt7b // Key label font 1
#define LABEL2_FONT &FreeSansBold12pt7b    // Key label font 2

// ----- NORMAL DISPLAY ----- //
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

// Keypad start position, key sizes and spacing
#define KEY_N_X 42 // Centre of key
#define KEY_N_Y 282 //302
#define KEY_N_W 67 // Width and height
#define KEY_N_H 30
#define KEY_N_SPACING_X 10 // X and Y gap
#define KEY_N_SPACING_Y 7
#define KEY_N_TEXTSIZE 1   // Font size multiplier

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

// Number length, buffer for storing it and character index
#define NUM_LEN 12
uint8_t numberIndex = 0;

// We have a status line for messages
#define STATUS_X 120 // Centred on this
#define STATUS_Y 305

// ----- SETUP DISPLAY END ----- //
int intv = 10;
int maxc = 25;
int hist = 3;
String numberBuffer1 = String(maxc);
String numberBuffer2 = String(intv);
String numberBuffer3 = String(hist);
bool display_changed = true;
bool setup_screen = false;
float temp_c = 25;
// Create 12 keys for the keypad
char keyLabel1[12][5] = {"temp", "intv", "hist", "+", "+", "+", "-", "-", "-", "Send", "RST", "Exit"};
uint16_t keyColor1[12] = {
                        TFT_BLACK, TFT_BLACK, TFT_BLACK,
                        TFT_RED, TFT_RED, TFT_RED,
                        TFT_BLUE, TFT_BLUE, TFT_BLUE,
                        TFT_DARKGREEN, TFT_DARKGREEN, TFT_DARKGREEN
                        };
// Create 3 keys for the keypad
char keyLabel2[3][6] = {"+", "-", "Setup"};
uint16_t keyColor2[12] = {
                        TFT_RED,
                        TFT_BLUE,
                        TFT_DARKGREEN
                        };

// Invoke the TFT_eSPI button class and create all the button objects
TFT_eSPI_Button key[12];


//------------------------------------------------------------------------------------------

void setup() {
  // Use serial port
  Serial.begin(115200);

  // Initialise the TFT screen
  tft.init();

  // Set the rotation before we calibrate
  tft.setRotation(0);

  // Calibrate the touch screen and retrieve the scaling factors
  touch_calibrate();

  // Draw keypad background
  tft.fillRect(0, 0, 240, 320, TFT_DARKGREY);

  delay(10); // UI debouncing
}

//------------------------------------------------------------------------------------------

void loop(void) {
  int i_am_here = 0;
  int pressed = 0;
  if (display_changed && setup_screen) {
    // ----- SETUP DISPLAY INIT ----- //

    // Clear the screen
    tft.fillScreen(TFT_DARKGREY);

    // Draw number display area and frame
    tft.setTextDatum(TL_DATUM);    // Use top left corner as text coord datum
    tft.setFreeFont(LABEL1_FONT);  // Choose a nice font that fits box
    tft.setTextColor(TFT_BLACK);

    tft.drawString("Temperature", 6, 20);
    tft.fillRect(DISP1_S_X, DISP1_S_Y, DISP1_S_W, DISP1_S_H, TFT_BLACK);
    tft.drawRect(DISP1_S_X, DISP1_S_Y, DISP1_S_W, DISP1_S_H, TFT_WHITE);
    tft.drawString("Interval", 6, 70);
    tft.fillRect(DISP2_S_X, DISP2_S_Y, DISP2_S_W, DISP2_S_H, TFT_BLACK);
    tft.drawRect(DISP2_S_X, DISP2_S_Y, DISP2_S_W, DISP2_S_H, TFT_WHITE);
    tft.drawString("Histeresis", 6, 120);
    tft.fillRect(DISP3_S_X, DISP3_S_Y, DISP3_S_W, DISP3_S_H, TFT_BLACK);
    tft.drawRect(DISP3_S_X, DISP3_S_Y, DISP3_S_W, DISP3_S_H, TFT_WHITE);

    tft.fillRect(DISP1_S_X + 4, DISP1_S_Y + 1, DISP1_S_W - 5, DISP1_S_H - 2, TFT_BLACK);
    String numberBuffer1= String(maxc);
    tft.fillRect(DISP2_S_X + 4, DISP2_S_Y + 1, DISP2_S_W - 5, DISP2_S_H - 2, TFT_BLACK);
    String numberBuffer2= String(intv);
    tft.fillRect(DISP3_S_X + 4, DISP3_S_Y + 1, DISP3_S_W - 5, DISP3_S_H - 2, TFT_BLACK);
    String numberBuffer3= String(hist);

    // Update the number display field
    tft.setTextDatum(TL_DATUM);        // Use top left corner as text coord datum
    tft.setFreeFont(&FreeSans18pt7b);  // Choose a nice font that fits box
    tft.setTextColor(DISP1_S_TCOLOR);  // Set the font color

    // Draw the string, the value returned is the width in pixels
    int xwidth1 = tft.drawString(numberBuffer1, DISP1_S_X + 4, DISP1_S_Y + 9);

    // Now cover up the rest of the line up by drawing a black rectangle.  No flicker this way
    // but it will not work with italic or oblique fonts due to character overlap.
    tft.fillRect(DISP1_S_X + 4 + xwidth1, DISP1_S_Y + 1, DISP1_S_W - xwidth1 - 5, DISP1_S_H - 2, TFT_BLACK);

    // Draw the string, the value returned is the width in pixels
    int xwidth2 = tft.drawString(numberBuffer2, DISP2_S_X + 4, DISP2_S_Y + 9);

    // Now cover up the rest of the line up by drawing a black rectangle.  No flicker this way
    // but it will not work with italic or oblique fonts due to character overlap.
    tft.fillRect(DISP2_S_X + 4 + xwidth2, DISP2_S_Y + 1, DISP2_S_W - xwidth2 - 5, DISP2_S_H - 2, TFT_BLACK);

    // Draw the string, the value returned is the width in pixels
    int xwidth3 = tft.drawString(numberBuffer3, DISP3_S_X + 4, DISP3_S_Y + 9);

    // Now cover up the rest of the line up by drawing a black rectangle.  No flicker this way
    // but it will not work with italic or oblique fonts due to character overlap.
    tft.fillRect(DISP3_S_X + 4 + xwidth3, DISP3_S_Y + 1, DISP3_S_W - xwidth3 - 5, DISP3_S_H - 2, TFT_BLACK);

    // Draw keypad Setup
    //drawKeypad1(4, 3, keyLabel1, keyColor1);
    // Draw the keys
    delay(10);
    for (uint8_t row = 0; row < 4; row++) {
      for (uint8_t col = 0; col < 3; col++) {
        uint8_t b = col + row * 3;

        if (b > 8) tft.setFreeFont(LABEL1_FONT);
        else tft.setFreeFont(LABEL2_FONT);

        key[b].initButton(&tft, KEY_S_X + col * (KEY_S_W + KEY_S_SPACING_X),
                          KEY_S_Y + row * (KEY_S_H + KEY_S_SPACING_Y), // x, y, w, h, outline, fill, text
                          KEY_S_W, KEY_S_H, TFT_WHITE, keyColor1[b], TFT_WHITE,
                          keyLabel1[b], KEY_S_TEXTSIZE);
        key[b].drawButton();
      }
    // ----- SETUP DISPLAY INIT END ----- //
    }
    display_changed = false;
    i_am_here = 1;
  }

  if (display_changed && ! setup_screen) {
    // ----- DISPLAY ROUTINE INIT ----- //

    // Clear the screen
    tft.fillScreen(TFT_DARKGREY);

    // Draw number display area and frame
    tft.setTextDatum(TL_DATUM);    // Use top left corner as text coord datum
    tft.setFreeFont(LABEL1_FONT);  // Choose a nice font that fits box
    tft.setTextColor(TFT_BLACK);

    tft.drawString("Current Temperature", 6, 20);
    tft.fillRect(DISP1_N_X, DISP1_N_Y, DISP1_N_W, DISP1_N_H, TFT_BLACK);
    tft.drawRect(DISP1_N_X, DISP1_N_Y, DISP1_N_W, DISP1_N_H, TFT_WHITE);
    tft.drawString("Target Temperature", 6, 120);
    tft.fillRect(DISP2_N_X, DISP2_N_Y, DISP2_N_W, DISP2_N_H, TFT_BLACK);
    tft.drawRect(DISP2_N_X, DISP2_N_Y, DISP2_N_W, DISP2_N_H, TFT_WHITE);

    tft.fillRect(DISP1_N_X + 4, DISP1_N_Y + 1, DISP1_N_W - 5, DISP1_N_H - 2, TFT_BLACK);
    String numberBuffer_a= String(temp_c);
    tft.fillRect(DISP2_N_X + 4, DISP2_N_Y + 1, DISP2_N_W - 5, DISP2_N_H - 2, TFT_BLACK);
    String numberBuffer_b= String(maxc+hist);

    // Update the number display field
    tft.setTextDatum(TL_DATUM);        // Use top left corner as text coord datum
    tft.setFreeFont(&FreeSans18pt7b);  // Choose a nice font that fits box
    tft.setTextColor(DISP1_N_TCOLOR);  // Set the font color

    // Draw the string, the value returned is the width in pixels
    int xwidth_a = tft.drawString(numberBuffer_a, DISP1_N_X + 4, DISP1_N_Y + 9);

    // Now cover up the rest of the line up by drawing a black rectangle.  No flicker this way
    // but it will not work with italic or oblique fonts due to character overlap.
    tft.fillRect(DISP1_N_X + 4 + xwidth_a, DISP1_N_Y + 1, DISP1_N_W - xwidth_a - 5, DISP1_N_H - 2, TFT_BLACK);

    // Draw the string, the value returned is the width in pixels
    int xwidth_b = tft.drawString(numberBuffer_b, DISP2_N_X + 4, DISP2_N_Y + 9);

    // Now cover up the rest of the line up by drawing a black rectangle.  No flicker this way
    // but it will not work with italic or oblique fonts due to character overlap.
    tft.fillRect(DISP2_N_X + 4 + xwidth_b, DISP2_N_Y + 1, DISP2_N_W - xwidth_b - 5, DISP2_N_H - 2, TFT_BLACK);

    // Draw keypad Display
    //drawKeypad2(1, 3, keyLabel2, keyColor2);
    // Draw the keys
    delay(10);
    for (uint8_t row = 0; row < 1; row++) {
      for (uint8_t col = 0; col < 3; col++) {
        uint8_t b = col + row * 3;

        if (b > 1) tft.setFreeFont(LABEL1_FONT);
        else tft.setFreeFont(LABEL2_FONT);

        key[b].initButton(&tft, KEY_N_X + col * (KEY_N_W + KEY_N_SPACING_X),
                          KEY_N_Y + row * (KEY_N_H + KEY_N_SPACING_Y), // x, y, w, h, outline, fill, text
                          KEY_N_W, KEY_N_H, TFT_WHITE, keyColor2[b], TFT_WHITE,
                          keyLabel2[b], KEY_N_TEXTSIZE);
        key[b].drawButton();
      }
    }
    // ----- DISPLAY ROUTINE INIT END ----- //
    display_changed = false;
    i_am_here = 2;
  }

  uint16_t t_x = 0, t_y = 0; // To store the touch coordinates
  // Pressed will be set true is there is a valid touch on the screen
  pressed = tft.getTouch(&t_x, &t_y);

  if (setup_screen && pressed > 0) {
    // ----- SETUP ROUTINE ----- //
    // Check if any key coordinate boxes contain the touch coordinates
    for (uint8_t b = 3; b < 12; b++) {
      if (pressed && key[b].contains(t_x, t_y)) {
        key[b].press(true);  // tell the button it is pressed
      } else {
        key[b].press(false);  // tell the button it is NOT pressed
      }
    }

    // Check if any key has changed state
    for (uint8_t b = 0; b < 12; b++) {

      if (b > 5) tft.setFreeFont(LABEL1_FONT);
      else tft.setFreeFont(LABEL2_FONT);

      if (key[b].justReleased()) key[b].drawButton();     // draw normal
      if (key[b].justPressed()) {
        key[b].drawButton(true);  // draw invert

        // if a numberpad button, append + to the numberBuffer
        // 3/6
        if (b == 3 && maxc <= 29) {
            maxc++;
            status(""); // Clear the old status
        }
        if (b == 6 && maxc >= 19) {
            maxc--;
            status(""); // Clear the old status
        }

        // 4/7
        if (b == 4 && intv <= 25) {
            intv = intv + 5;
            status(""); // Clear the old status
        }
        if (b == 7 && intv >= 15) {
            intv = intv - 5;
            status(""); // Clear the old status
        }

        //5/8
        if (b == 5 && hist <= 4) {
            hist++;
            status(""); // Clear the old status
        }
        if (b == 8 && hist >= 3) {
            hist--;
            status(""); // Clear the old status
        }

        // New
        if (b == 10) {
          intv = 10;
          maxc = 25;
          hist = 3;
          status("Values cleared");
        }

        tft.fillRect(DISP1_S_X + 4, DISP1_S_Y + 1, DISP1_S_W - 5, DISP1_S_H - 2, TFT_BLACK);
        String numberBuffer1= String(maxc);
        tft.fillRect(DISP2_S_X + 4, DISP2_S_Y + 1, DISP2_S_W - 5, DISP2_S_H - 2, TFT_BLACK);
        String numberBuffer2= String(intv);
        tft.fillRect(DISP3_S_X + 4, DISP3_S_Y + 1, DISP3_S_W - 5, DISP3_S_H - 2, TFT_BLACK);
        String numberBuffer3= String(hist);

        // Update the number display field
        tft.setTextDatum(TL_DATUM);        // Use top left corner as text coord datum
        tft.setFreeFont(&FreeSans18pt7b);  // Choose a nice font that fits box
        tft.setTextColor(DISP1_S_TCOLOR);  // Set the font color

        // Draw the string, the value returned is the width in pixels
        int xwidth1 = tft.drawString(numberBuffer1, DISP1_S_X + 4, DISP1_S_Y + 9);

        // Now cover up the rest of the line up by drawing a black rectangle.  No flicker this way
        // but it will not work with italic or oblique fonts due to character overlap.
        tft.fillRect(DISP1_S_X + 4 + xwidth1, DISP1_S_Y + 1, DISP1_S_W - xwidth1 - 5, DISP1_S_H - 2, TFT_BLACK);

        // Draw the string, the value returned is the width in pixels
        int xwidth2 = tft.drawString(numberBuffer2, DISP2_S_X + 4, DISP2_S_Y + 9);

        // Now cover up the rest of the line up by drawing a black rectangle.  No flicker this way
        // but it will not work with italic or oblique fonts due to character overlap.
        tft.fillRect(DISP2_S_X + 4 + xwidth2, DISP2_S_Y + 1, DISP2_S_W - xwidth2 - 5, DISP2_S_H - 2, TFT_BLACK);

        // Draw the string, the value returned is the width in pixels
        int xwidth3 = tft.drawString(numberBuffer3, DISP3_S_X + 4, DISP3_S_Y + 9);

        // Now cover up the rest of the line up by drawing a black rectangle.  No flicker this way
        // but it will not work with italic or oblique fonts due to character overlap.
        tft.fillRect(DISP3_S_X + 4 + xwidth3, DISP3_S_Y + 1, DISP3_S_W - xwidth3 - 5, DISP3_S_H - 2, TFT_BLACK);

        if (b == 9) {
          status("Values sent to serial port");
          Serial.println("maxc: " + numberBuffer1);
          Serial.println("intv: " + numberBuffer2);
          Serial.println("hist: " + numberBuffer3);
        }

        // Exit Setup
        if (b == 11) {
          Serial.println("Exit Setup screen");
          setup_screen = false;
          display_changed = true;
          delay(300);
        }

        delay(10); // UI debouncing
      }
    i_am_here = 3;
    }
    pressed = 0;
    // ----- SETUP ROUTINE END ----- //
  }

  if (! setup_screen && pressed > 0) {
    // ----- DISPLAY ROUTINE ----- //
    // Check if any key coordinate boxes contain the touch coordinates
    for (uint8_t b = 0; b < 3; b++) {
      if (pressed && key[b].contains(t_x, t_y)) {
        key[b].press(true);  // tell the button it is pressed
      } else {
        key[b].press(false);  // tell the button it is NOT pressed
      }
    }

    // Check if any key has changed state
    for (uint8_t b = 0; b < 3; b++) {

      if (b > 0) tft.setFreeFont(LABEL1_FONT);
      else tft.setFreeFont(LABEL2_FONT);

      if (key[b].justReleased()) key[b].drawButton();     // draw normal
      if (key[b].justPressed()) {
        key[b].drawButton(true);  // draw invert

        // if a numberpad button, append + to the numberBuffer
        // 0/1
        if (b == 0 && maxc <= 29) {
            maxc++;
            status("Temperature raised");
        }
        if (b == 1 && maxc >= 19) {
            maxc--;
            status("Temperature lowered");
        }
        // Enter Setup
        if (b == 2) {
          Serial.println("Enter Setup screen");
          setup_screen = true;
          display_changed = true;
          delay(300);
        }

        tft.fillRect(DISP1_N_X + 4, DISP1_N_Y + 1, DISP1_N_W - 5, DISP1_N_H - 2, TFT_BLACK);
        String numberBuffer_a= String(temp_c);
        tft.fillRect(DISP2_N_X + 4, DISP2_N_Y + 1, DISP2_N_W - 5, DISP2_N_H - 2, TFT_BLACK);
        String numberBuffer_b= String(maxc+hist);

        // Update the number display field
        tft.setTextDatum(TL_DATUM);        // Use top left corner as text coord datum
        tft.setFreeFont(&FreeSans18pt7b);  // Choose a nice font that fits box
        tft.setTextColor(DISP1_N_TCOLOR);  // Set the font color

        // Draw the string, the value returned is the width in pixels
        int xwidth_a = tft.drawString(numberBuffer_a, DISP1_N_X + 4, DISP1_N_Y + 9);

        // Now cover up the rest of the line up by drawing a black rectangle.  No flicker this way
        // but it will not work with italic or oblique fonts due to character overlap.
        tft.fillRect(DISP1_N_X + 4 + xwidth_a, DISP1_N_Y + 1, DISP1_N_W - xwidth_a - 5, DISP1_N_H - 2, TFT_BLACK);

        // Draw the string, the value returned is the width in pixels
        int xwidth_b = tft.drawString(numberBuffer_b, DISP2_N_X + 4, DISP2_N_Y + 9);

        // Now cover up the rest of the line up by drawing a black rectangle.  No flicker this way
        // but it will not work with italic or oblique fonts due to character overlap.
        tft.fillRect(DISP2_N_X + 4 + xwidth_b, DISP2_N_Y + 1, DISP2_N_W - xwidth_b - 5, DISP2_N_H - 2, TFT_BLACK);

        delay(10); // UI debouncing
      }
    i_am_here = 4;
    }
    // ----- DISPLAY ROUTINE END ----- //
    pressed = 0;
  }
  Serial.print("i_am_here = ");
  Serial.println(i_am_here);
  //Serial.print("setup_screen = ");
  //Serial.println(setup_screen);
  //Serial.print("display_changed = ");
  //Serial.println(display_changed);
  //Serial.println("");
  //delay(200);
}

//------------------------------------------------------------------------------------------
/*
void drawKeypad1(uint8_t key_row, uint8_t key_col, char keyLabel1, uint16_t keyColor1)
{
  // Draw the keys
  for (uint8_t row = 0; row < key_row; row++) {
    for (uint8_t col = 0; col < key_col; col++) {
      uint8_t b = col + row * 3;

      //if (b > 8) tft.setFreeFont(LABEL1_FONT);
      //else tft.setFreeFont(LABEL2_FONT);

      key[b].initButton(&tft, KEY_S_X + col * (KEY_S_W + KEY_S_SPACING_X),
                        KEY_S_Y + row * (KEY_S_H + KEY_S_SPACING_Y), // x, y, w, h, outline, fill, text
                        KEY_S_W, KEY_S_H, TFT_WHITE, keyColor1[b], TFT_WHITE,
                        keyLabel1[b], KEY_S_TEXTSIZE);
      key[b].drawButton();
    }
  }
}

void drawKeypad2(uint8_t key_row, uint8_t key_col, char keyLabel2, uint16_t keyColor2)
{
  // Draw the keys
  for (uint8_t row = 0; row < key_row; row++) {
    for (uint8_t col = 0; col < key_col; col++) {
      uint8_t b = col + row * 3;

      //if (b > 8) tft.setFreeFont(LABEL1_FONT);
      //else tft.setFreeFont(LABEL2_FONT);

      key[b].initButton(&tft, KEY_S_X + col * (KEY_S_W + KEY_S_SPACING_X),
                        KEY_S_Y + row * (KEY_S_H + KEY_S_SPACING_Y), // x, y, w, h, outline, fill, text
                        KEY_S_W, KEY_S_H, TFT_WHITE, keyColor2[b], TFT_WHITE,
                        keyLabel2[b], KEY_S_TEXTSIZE);
      key[b].drawButton();
    }
  }
}
*/
//------------------------------------------------------------------------------------------

void touch_calibrate()
{
  uint16_t calData[5];
  uint8_t calDataOK = 0;

  // check file system exists
  if (!SPIFFS.begin()) {
    Serial.println("Formating file system");
    SPIFFS.format();
    SPIFFS.begin();
  }

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
        if (f.readBytes((char *)calData, 8) == 8)
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

    tft.calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 9);

    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.println("Calibration complete!");

    // store data
    File f = SPIFFS.open(CALIBRATION_FILE, "w");
    if (f) {
      f.write((const unsigned char *)calData, 8);
      f.close();
    }
  }
}

//------------------------------------------------------------------------------------------

// Print something in the mini status bar
void status(const char *msg) {
  tft.setTextPadding(240);
  //tft.setCursor(STATUS_X, STATUS_Y);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.setTextFont(0);
  tft.setTextDatum(TC_DATUM);
  tft.setTextSize(1);
  tft.drawString(msg, STATUS_X, STATUS_Y);
}

//------------------------------------------------------------------------------------------

