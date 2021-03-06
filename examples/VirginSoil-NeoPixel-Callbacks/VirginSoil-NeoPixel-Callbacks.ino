/*
  This is an initial sketch to be used as a "blueprint" to create apps which can be used with IOTappstory.com infrastructure
  Your code can be added wherever it is marked. 

  You will need a button & NeoPixel (ws2812 led)

  This sketch is based on:
  IotAppStory Callbacks   [Ildefonso Zanette]
  IotAppStory VirginSoil  [Andreas Spiess]
  Simple - Example        [Adafruit_NeoPixel_Library]

  Copyright (c) [2018] [Onno Dirkzwager]

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

*/

#define APPNAME "WS2812B-VirginSoil"
#define VERSION "V1.0.0"
#define COMPDATE __DATE__ __TIME__
#define MODEBUTTON 0                                      // Button pin on the esp8266 for selecting modes. D3 for the Wemos!

#define FEEDBACKLED 4                                     // Pin on the esp8266 connected to the NeoPixel you want to use for feedback. D2 for the Wemos!

#include <IOTAppStory.h>                                  // IotAppStory.com library
#include <ESP8266WiFi.h>                                  // esp core wifi library
#include <Adafruit_NeoPixel.h>                            // Adafruit_NeoPixel_Library (https://github.com/adafruit/Adafruit_NeoPixel)


IOTAppStory IAS(APPNAME, VERSION, COMPDATE, MODEBUTTON);  // Initialize IotAppStory
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(1, FEEDBACKLED, NEO_GRB + NEO_KHZ800);



// ================================================ VARS =================================================
unsigned long printEntry;

uint32_t none   = pixels.Color(0, 0, 0);

uint32_t cyan   = pixels.Color(0, 255, 255);              // wait / booting
uint32_t white  = pixels.Color(255, 255, 255);            // configuration mode

uint32_t blue   = pixels.Color(0, 0, 255);                // If mode button is released, I will enter in firmware update mode.
uint32_t purple = pixels.Color(128, 0, 128);              // If mode button is released, I will enter in configuration mode.

uint32_t yellow = pixels.Color(255, 255, 100);            // firmware update mode: calling home / checking for updates
uint32_t orange = pixels.Color(255, 140, 0);              // firmware update mode: downloading update
uint32_t red    = pixels.Color(255, 0, 0);                // firmware update mode: update error, check your logs

uint32_t green  = pixels.Color(0, 128, 0);                // loop / running



// ================================================ SETUP ================================================
void setup() {
  IAS.serialdebug(true);                                  // 1st parameter: true or false for serial debugging. Default: false
  //IAS.serialdebug(true,115200);                         // 1st parameter: true or false for serial debugging. Default: false | 2nd parameter: serial speed. Default: 115200

  
  pixels.begin();                                         // This initializes the NeoPixel library.
  pixels.setBrightness(1);                                // set pixel brightness (1-255)

  pixels.setPixelColor(0, cyan);                          // send cyan / wait to the neopixel
  pixels.show();                                          // tell the neopixel to show the color
  delay(100);
  

  String boardName = "ws2812b-vs-" + WiFi.macAddress().substring(9, 99);
  IAS.preSetBoardname(boardName);                         // preset Boardname this is also your MDNS responder: http://woled-vs.local


  IAS.setCallHome(true);                                  // Set to true to enable calling home frequently (disabled by default)
  IAS.setCallHomeInterval(60);                            // Call home interval in seconds, use 60s only for development. Please change it to at least 2 hours in production


  // You can configure callback functions that can give feedback to the app user about the current state of the application.
  // In this example we use serial print to demonstrate the call backs. But you could use leds etc.
  
  IAS.onModeButtonShortPress([]() {
    Serial.println(F(" If mode button is released, I will enter in firmware update mode."));
    Serial.println(F("*-------------------------------------------------------------------------*"));
    
    pixels.setPixelColor(0, blue);                        // send blue / release for updates to the neopixel
    pixels.show();
  });

  IAS.onModeButtonLongPress([]() {
    Serial.println(F(" If mode button is released, I will enter in configuration mode."));
    Serial.println(F("*-------------------------------------------------------------------------*"));
    
    pixels.setPixelColor(0, purple);                      // send purple / release for config to the neopixel
    pixels.show();
    delay(100);
  });

  IAS.onConfigMode([]() {
    pixels.setPixelColor(0, white);                       // send white /entered config mode to the neopixel
    pixels.show();
    delay(100);
  });

  IAS.onFirmwareUpdateCheck([]() {
    pixels.setPixelColor(0, yellow);                      // send yellow / update check to the neopixel
    pixels.show();
    delay(100);
  });

  IAS.onFirmwareUpdateDownload([]() {
    pixels.setPixelColor(0, orange);                      // send orange / update download to the neopixel
    pixels.show();
    delay(100);
  });

  IAS.onFirmwareUpdateError([]() {
    pixels.setPixelColor(0, red);                         // send red / update error to the neopixel
    pixels.show();
    delay(100);
  });
  

  IAS.begin(true,'P');                                    // 1st parameter: true or false to view BOOT STATISTICS
                                                          // 2nd parameter: Wat to do with EEPROM on First boot of the app? 'F' Fully erase | 'P' Partial erase(default) | 'L' Leave intact

  //-------- Your Setup starts from here ---------------


}



// ================================================ LOOP =================================================
void loop() {
  IAS.buttonLoop();                                        // this routine handles the calling home functionality and reaction of the MODEBUTTON pin. If short press (<4 sec): update of sketch, long press (>7 sec): Configuration


  //-------- Your Sketch starts from here ---------------

  
  if(digitalRead(MODEBUTTON) == HIGH) {                    // if the button is not being pressed set the neopixel to green and blink every 5 seconds
    if(millis() - printEntry > 5000) {
      printEntry = millis();
  
      // send black / off to the neopixel
      pixels.setPixelColor(0, none);
      pixels.show();
      delay(400);
    }else{
      // send green / running to the neopixel
      pixels.setPixelColor(0, green);
      pixels.show();
      delay(100);
    }
  }

}



// ================================================ Extra functions ======================================
