/*
  This is an initial sketch to be used as a "blueprint" to create apps which can be used with IOTappstory.com infrastructure
  Your code can be added wherever it is marked. 

  You will need the button & OLED shields!

  Copyright (c) [2018] [Andreas Spiess]

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

#define APPNAME "TempSensor"
#define VERSION "V1.3.0"
#define COMPDATE __DATE__ __TIME__
#define MODEBUTTON D3


#include <SSD1306.h>              // OLED library by Daniel Eichhorn
#include <IOTAppStory.h>          // IotAppStory.com library
#include <DHT.h>                  // Adafruit DHT Arduino library
#include <ESP8266WiFi.h>          // esp core wifi library
#include <PubSubClient.h>         // Arduino Client for MQTT by knolleary/pubsubclient


#define PIN_RESET 255             //
#define DC_JUMPER 0               // I2C Addres: 0 - 0x3C, 1 - 0x3D
#define DHTPIN D4                 // what pin we're connected to


// Uncomment whatever type you're using!
#define DHTTYPE DHT11                                     // DHT 11
//#define DHTTYPE DHT21                                   // DHT 21 (AM2301)
//#define DHTTYPE DHT22                                   // DHT 22  (AM2302)


WiFiClient espClient;
PubSubClient thingspeak(espClient);
DHT dht(DHTPIN, DHTTYPE);                                 // Initialize DHT sensor.
SSD1306  display(0x3c, D2, D1);                           // Initialize OLED

IOTAppStory IAS(APPNAME, VERSION, COMPDATE, MODEBUTTON);  // Initialize IotAppStory



// ================================================ VARS =================================================
int tempEntry;
float h, t, f, hic, hif;
char* outTopic          = "channels/<channelID/publish/";
const char* mqtt_server = "mqtt.thingspeak.com";
char msg[50]            = "field1=22.5&field2=65.7&status=MQTTPUBLISH";

// We want to be able to edit these example variables below from the wifi config manager
// Currently only char arrays are supported.
// Use functions like atoi() and atof() to transform the char array to integers or floats
// Use IAS.dPinConv() to convert Dpin numbers to integers (D6 > 14)

char* scale = "0";                                        // 0 = Celsius, 1 = Fahrenheit
char* apikey = "";                                        // ThingSpeak Write API Key



// ================================================ SETUP ================================================
void setup() {
  IAS.serialdebug(true);                                  // 1st parameter: true or false for serial debugging. Default: false
  //IAS.serialdebug(true,115200);                         // 1st parameter: true or false for serial debugging. Default: false | 2nd parameter: serial speed. Default: 115200
  
  display.init();                                         // setup OLED and show "Wait"
  display.flipScreenVertically();
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(48, 35, F("Wait"));
  display.display();


  String boardName = APPNAME"_" + WiFi.macAddress();
  IAS.preSetBoardname(boardName);                         // preset Boardname this is also your MDNS responder: http://TempSensor_xx:xx:xx.local


  IAS.setCallHome(true);                                  // Set to true to enable calling home frequently (disabled by default)
  IAS.setCallHomeInterval(60);                            // Call home interval in seconds, use 60s only for development. Please change it to at least 2 hours in production


  IAS.addField(scale, "scale", "Celc Fahr (0,1)", 1);     // These fields are added to the config wifimanager and saved to eeprom. Updated values are returned to the original variable.
  IAS.addField(apikey, "key", "TS Write API Key", 16);    // reference to org variable | field name | field label value | max char return


  // You can configure callback functions that can give feedback to the app user about the current state of the application.
  // In this example we use serial print to demonstrate the call backs. But you could use leds etc.
  
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
  

  IAS.begin(true,'P');                                    // 1st parameter: true or false to view BOOT STATISTICS
                                                          // 2nd parameter: Wat to do with EEPROM on First boot of the app? 'F' Fully erase | 'P' Partial erase(default) | 'L' Leave intact

  //-------- Your Setup starts from here ---------------


  // start the temp sensor
  dht.begin();

  // If the Write API Key is set start the mqtt_server
  if((apikey != NULL) && (apikey[0] != '\0')){
    thingspeak.setServer(mqtt_server, 1883);
    thingspeak.setCallback(callback);
    strcat(outTopic, apikey);

    Serial.println(F(" Connected to ThingsSpeak."));
    Serial.println(F("*-------------------------------------------------------------------------*"));
  }else{
    Serial.println(F(" No API Key found!"));
    Serial.println(F(" Set your \"Write API Key\" in config to connect and write to ThingsSpeak."));
    Serial.println(F("*-------------------------------------------------------------------------*"));
  }

  // Celc or Fahr?
  if(atoi(scale) == 0){
    Serial.println(F(" OLED display set to \"Celsius\", you can change this in config."));
    Serial.println(F("*-------------------------------------------------------------------------*\n\n"));
  }else{
    Serial.println(F(" OLED display set to \"Fahrenh\", you can change this in config."));
    Serial.println(F("*-------------------------------------------------------------------------*\n\n"));
  }
}



// ================================================ LOOP =================================================
void loop() {
  IAS.buttonLoop();   // this routine handles the calling home functionality and reaction of the MODEBUTTON pin. If short press (<4 sec): update of sketch, long press (>7 sec): Configuration


  //-------- Your Sketch starts from here ---------------
  
  if (millis() > tempEntry + 5000 && digitalRead(D3) == HIGH) { // Wait a few seconds between measurements.
    tempEntry = millis();
    // Reading temperature or humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)

    // Read temperature as Celsius (the default)
    unsigned long measurementEntry = millis();
    do {
      unsigned long readEntry = millis();

      // Reading temperature or humidity takes about 250 milliseconds!
      // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
      do {
        h = dht.readHumidity();
        yield();
      } while (isnan(h) && millis() - readEntry < 300);

      // Read temperature as Celsius (the default)
      readEntry = millis();
      do {
        t = dht.readTemperature();
        yield();
      } while (isnan(t) && millis() - readEntry < 300);

      // Read temperature as Fahrenheit (isFahrenheit = true)
      readEntry = millis();
      do {
        f = dht.readTemperature(true);
        yield();
      } while (isnan(f) && millis() - readEntry < 300);

    } while ((isnan(h) || isnan(t) || isnan(f)) && millis() - measurementEntry < 2000);

    // Check if any reads failed and exit early (to try again).
    if (isnan(h) || isnan(t) || isnan(f)) {
      Serial.println(F("Failed to read from DHT sensor!"));
      dispTemplate_threeLineV1(F("Error"), F("Reading"), F("Sensor"));
    }else{

      // Compute heat index in Fahrenheit (the default)
      hif = dht.computeHeatIndex(f, h);
      // Compute heat index in Celsius (isFahreheit = false)
      hic = dht.computeHeatIndex(t, h, false);

      // Celc or Fahr?
      if(atoi(scale) == 0){
        displayTemp(t, F("Celsius"));
      }else{
        displayTemp(f, F("Fahrenh"));
      }
      
      // Publish to Thingspeak
      if((apikey != NULL) && (apikey[0] != '\0')){
        publishToThingspeak();
      }
      
      // Print to serial
      printLog();

      if((apikey != NULL) && (apikey[0] != '\0')){
        Serial.println();
      }
    }
  }
  
}



// ================================================ Extra functions ======================================
void displayTemp(float temp, String scale) {
  display.clear();
  display.setFont(ArialMT_Plain_24);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(32, 15, String(temp));
  display.setFont(ArialMT_Plain_16);
  display.drawString(32, 40, scale);
  display.display();
}

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

void printLog() {
  Serial.print(F(" Humidity: "));
  Serial.print(h);
  Serial.print(" %\t");
  Serial.print(F("Temperature: "));
  Serial.print(t);
  Serial.print(F(" *C "));
  Serial.print(f);
  Serial.print(F(" *F\t"));
  Serial.print(F("Heat index: "));
  Serial.print(hic);
  Serial.print(F(" *C "));
  Serial.print(hif);
  Serial.println(F(" *F"));
}


void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print(F("Message arrived ["));
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because
    // it is acive low on the ESP-01)
  } else {
    digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!thingspeak.connected()) {
    Serial.print(F(" Attempting MQTT connection..."));
    // Attempt to connect
    if (thingspeak.connect("ESP8266Client")) {
      Serial.println(F("connected"));
      // Once connected, publish an announcement...
      thingspeak.publish(outTopic, "hello world");
      // ... and resubscribe
      thingspeak.subscribe("inTopic");
    } else {
      Serial.print(F("failed, rc="));
      Serial.print(thingspeak.state());
      Serial.println(F(" try again in 5 seconds"));
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void publishToThingspeak() {
  if (!thingspeak.connected()) {
    reconnect();
  }
  thingspeak.loop();
  snprintf (msg, 75, "field1=%f&field2=%f&status=MQTTPUBLISH", t, h);
  Serial.print(F(" Publish message: "));
  Serial.println(msg);
  delay(1);
  thingspeak.publish(outTopic, msg);
}

