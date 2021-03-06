/*
  This is an initial sketch to be used as a "blueprint" to create apps which can be used with IOTappstory.com infrastructure
  Your code can be added wherever it is marked. 

  You will need the button & OLED shields!

  Copyright (c) [2016] [Andreas Spiess]

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

#define APPNAME "WemosLoader"
#define VERSION "V1.1.0"
#define COMPDATE __DATE__ __TIME__
#define MODEBUTTON D3


#include <ESP8266WiFi.h>                                  // esp core wifi library
#include <IOTAppStory.h>                                  // IotAppStory.com library
#include <SSD1306.h>                                      // OLED library by Daniel Eichhorn


IOTAppStory IAS(APPNAME, VERSION, COMPDATE, MODEBUTTON);  // Initialize IotAppStory
SSD1306  display(0x3c, D2, D1);                           // Initialize OLED


// ================================================ VARS =================================================
unsigned long printEntry;



// ================================================ SETUP ================================================
void setup() {
  IAS.serialdebug(true);                                  // 1st parameter: true or false for serial debugging. Default: false
  //IAS.serialdebug(true,115200);                         // 1st parameter: true or false for serial debugging. Default: false | 2nd parameter: serial speed. Default: 115200

  display.init();
  display.flipScreenVertically();
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(48, 35, F("Wait"));
  display.display();

  String boardName = APPNAME"_" + WiFi.macAddress();
  IAS.preSetBoardname(boardName);                         // preset Boardname
  IAS.preSetAutoUpdate(false);                            // automaticUpdate (true, false)


  IAS.setCallHome(true);                                  // Set to true to enable calling home frequently (disabled by default)
  IAS.setCallHomeInterval(60);                            // Call home interval in seconds, use 60s only for development. Please change it to at least 2 hours in production


  IAS.onModeButtonShortPress([]() {
    Serial.println(F(" If mode button is released, I will enter in firmware update mode."));
    Serial.println(F("*-------------------------------------------------------------------------*"));
    dispTemplate_threeLineV2(F("Release"), F("for"), F("Updates"));
  });

  IAS.onModeButtonLongPress([]() {
    Serial.println(F(" If mode button is released, I will enter in configuration mode."));
    Serial.println(F("*-------------------------------------------------------------------------*"));
    dispTemplate_threeLineV2(F("Release"), F("for"), F("Config"));
  });
  
  IAS.onFirstBoot([]() {
    Serial.println(F(" Hardware reset necessary after Serial upload. Reset to continu!"));
    Serial.println(F("*-------------------------------------------------------------------------*"));
    dispTemplate_threeLineV1(F("Press"), F("Reset"), F("Button"));
    ESP.reset();
  });

  IAS.onConfigMode([]() {
    dispTemplate_threeLineV2(F("Connect to"), F("Wi-Fi"), "x:x:" + WiFi.macAddress().substring(9, 99));
  });

  IAS.onFirmwareUpdateCheck([]() {
    dispTemplate_threeLineV2(F("Checking"), F("for"), F("Updates"));
  });

  IAS.onFirmwareUpdateDownload([]() {
    dispTemplate_threeLineV2(F("Download"), F("&"), F("Install App"));
  });

  IAS.onFirmwareUpdateError([]() {
    dispTemplate_threeLineV1(F("Update"), F("Error"), F("Check logs"));
  });


  IAS.begin(true, 'F');                                     // 1st parameter: true or false to view BOOT STATISTICS
                                                            // 2nd parameter: Wat to do with EEPROM on First boot of the app? 'F' Fully erase | 'P' Partial erase(default) | 'L' Leave intact
  
  //-------- Your Setup starts from here ---------------


  IAS.callHome(true);
  
}



// ================================================ LOOP =================================================
void loop() {
  IAS.buttonLoop();                                         // this routine handles the reaction of the MODEBUTTON pin. If short press (<4 sec): update of sketch, long press (>7 sec): Configuration


  if (millis() - printEntry > 5000 && digitalRead(D3) == HIGH) {
    // if the sketch reaches this point, you failed to activate your device at IotAppStory.com, did not create a project or did not add an app to your project
    printEntry = millis();
    dispTemplate_fourLineV1(F("Error"), F("Not registred"), F("No project"), F("No app"));
  }
}



// ================================================ Extra functions ======================================
void dispTemplate_threeLineV1(String str1, String str2, String str3) {
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(32, 15, str1);
  display.drawString(32, 30, str2);
  display.drawString(32, 45, str3);
  display.display();
}

void dispTemplate_threeLineV2(String str1, String str2, String str3) {
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(32, 13, str1);
  display.setFont(ArialMT_Plain_24);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 24, str2);
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.drawString(96, 51, str3);
  display.display();
}

void dispTemplate_fourLineV1(String str1, String str2, String str3, String str4) {
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(32, 15, str1);
  display.drawString(32, 27, str2);
  display.drawString(32, 39, str3);
  display.drawString(32, 51, str4);
  display.display();
}


