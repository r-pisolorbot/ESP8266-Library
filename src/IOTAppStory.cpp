#include <ESP8266WiFi.h>
#include "ESP8266httpUpdateIasMod.h"
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>
#include <pgmspace.h>
#include <ArduinoJson.h>
#include <FS.h>
#include "IOTAppStory.h"
#include "WiFiManagerMod.h"

#ifdef REMOTEDEBUGGING
	#include <WiFiUDP.h>
#endif

// used by the RTC memory read/write functions
extern "C" {
	#include "user_interface.h"
}

IOTAppStory::IOTAppStory(const char* appName, const char* appVersion, const char *compDate, const int modeButton)
: _modeButton(modeButton)
, _compDate(compDate)
, _firstBootCallback(NULL)
, _noPressCallback(NULL)
, _shortPressCallback(NULL)
, _longPressCallback(NULL)
, _veryLongPressCallback(NULL)
, _firmwareUpdateCheckCallback(NULL)
, _firmwareUpdateDownloadCallback(NULL)
, _firmwareUpdateErrorCallback(NULL)
, _configModeCallback(NULL)
{

	config.appName = &appName;
	config.appVersion = &appVersion;

	_firmware = appName;
	_firmware += " ";
	_firmware += appVersion;
	
}

void IOTAppStory::firstBoot(char ea){
	//DEBUG_PRINT(FPSTR(SER_DEV));
	//DEBUG_PRINTLN(_firmware.c_str());

	// THIS ONLY RUNS ON THE FIRST BOOT OF A JUST INSTALLED APP (OR AFTER RESET TO DEFAULT SETTINGS) <-----------------------------------------------------------------------------------------------------------------
	
	// get json config......
	
	// erase eeprom after config (delete extra field data etc.)
	if(ea == 'F'){
		#if DEBUG_LVL >= 1
			DEBUG_PRINTLN(F(" Full erase of EEPROM"));
		#endif
		WiFi.disconnect(true); 							// Wipe out WiFi credentials.
		delay(200);
		eraseFlash(0,EEPROM_SIZE);						// erase full eeprom
		
		
		String emty = F("000000");
		emty.toCharArray(config.actCode, 7);
		emty = "";
		emty.toCharArray(config.ssid, STRUCT_CHAR_ARRAY_SIZE);
		emty.toCharArray(config.password, STRUCT_CHAR_ARRAY_SIZE);
		config.automaticConfig = true;	// temp fix

	}else if(ea == 'P'){
		#if DEBUG_LVL >= 1
			DEBUG_PRINTLN(F(" Partial erase of EEPROM but leaving config settings"));
		#endif
		eraseFlash((sizeof(config)+2),EEPROM_SIZE);		// erase eeprom but leave the config settings
	}
	#if DEBUG_LVL >= 1
	else{
		DEBUG_PRINTLN(F(" Leave EEPROM intact"));
	#endif
	}
	
	// update first boot config flag (date)
	strcpy(config.compDate, _compDate);
	writeConfig();

	DEBUG_PRINTLN(FPSTR(SER_DEV));
	
	if (_firstBootCallback){
		#if DEBUG_LVL >= 3
			DEBUG_PRINTLN(F(" Run first boot callback"));
		#endif
		_firstBootCallback();
	}
}

void IOTAppStory::serialdebug(bool onoff,int speed){
	_serialDebug = onoff;
	if(_serialDebug == true){
		Serial.begin(speed);
		DEBUG_PRINT(F("\n\n\n\n\n"));
	}
}


void IOTAppStory::preSetBoardname(String boardName){
	if (!_configReaded) {
		readConfig();
	}
	_setBoardName = true;
	SetConfigValueCharArray(config.boardName, boardName, STRUCT_BNAME_SIZE, _setPreSet);
}

void IOTAppStory::preSetAutoUpdate(bool automaticUpdate){
	if (!_configReaded) {
		readConfig();
	}
	SetConfigValue(config.automaticUpdate, automaticUpdate, _setPreSet);
}

void IOTAppStory::preSetAutoConfig(bool automaticConfig){
	if (!_configReaded) {
		readConfig();
	}
	SetConfigValue(config.automaticConfig, automaticConfig, _setPreSet);
}

void IOTAppStory::preSetWifi(String ssid, String password){
	if (!_configReaded) {
		readConfig();
	}
	
	//ssid.toCharArray(config.ssid, STRUCT_CHAR_ARRAY_SIZE);
	//password.toCharArray(config.password, STRUCT_CHAR_ARRAY_SIZE);
	_setPreSet = true;
	SetConfigValueCharArray(config.ssid, ssid, STRUCT_CHAR_ARRAY_SIZE, _setPreSet);
	SetConfigValueCharArray(config.password, password, STRUCT_CHAR_ARRAY_SIZE, _setPreSet);
}

void IOTAppStory::preSetServer(String HOST1, String FILE1){
	if (!_configReaded) {
		readConfig();
	}
	SetConfigValueCharArray(config.HOST1, HOST1, STRUCT_CHAR_ARRAY_SIZE, _setPreSet);
	SetConfigValueCharArray(config.FILE1, FILE1, STRUCT_CHAR_ARRAY_SIZE, _setPreSet);
}


// for backwards comp | depreciated
void IOTAppStory::preSetConfig(String boardName){
	preSetBoardname(boardName);
}
// for backwards comp | depreciated
void IOTAppStory::preSetConfig(String boardName, bool automaticUpdate){
	preSetBoardname(boardName);
}
// for backwards comp | depreciated
void IOTAppStory::preSetConfig(String ssid, String password){
	preSetWifi(ssid, password);
}
// for backwards comp | depreciated
void IOTAppStory::preSetConfig(String ssid, String password, String boardName){
	preSetWifi(ssid, password);
	preSetBoardname(boardName);
}


void IOTAppStory::begin(bool bootstats /*= true*/){
	begin(bootstats, 'P');
}

void IOTAppStory::begin(bool bootstats, bool ea){
	//#error "begin(bool bootstats, bool ea) is depreciated. Use: begin(bool bootstats, char ea) instead. See VirginSoil examples for more info."
	if(ea == true){
		begin(bootstats, 'F');
	}else{
		begin(bootstats, 'P');
	}
}

void IOTAppStory::begin(bool bootstats, char ea){
	// if boardName is not set, set it to the appName
	if(_setBoardName == false){
		preSetBoardname("yourESP");
	}
	
	DEBUG_PRINTLN(F("\n"));
	
	// read config if needed
	if (!_configReaded) {
		readConfig();
	}

	// write config if detected changes
	if(_setPreSet == true){
		writeConfig();
		#if DEBUG_LVL >= 1
			DEBUG_PRINTLN(F("Saving config presets...\n"));
		#endif
	}
	#if DEBUG_LVL >= 1
		DEBUG_PRINTLN(FPSTR(SER_DEV));
		DEBUG_PRINTF_P(PSTR(" Start %s\n"), _firmware.c_str());
	#endif
	#if DEBUG_LVL >= 2
		DEBUG_PRINTLN(FPSTR(SER_DEV));
		DEBUG_PRINTF_P(PSTR(" Mode select button: GPIO%d\n"), _modeButton);
		DEBUG_PRINTF_P(PSTR(" Boardname: %s\n"), config.boardName);
		DEBUG_PRINTF_P(PSTR(" Automatic update: %d\n"), config.automaticUpdate);
	#endif
	#if DEBUG_LVL >= 1
		DEBUG_PRINTLN(FPSTR(SER_DEV));
	#endif

	// ----------- PINS ----------------
	pinMode(_modeButton, INPUT_PULLUP);     		// MODEBUTTON as input for Config mode selection

	// Read the "bootTime" & "boardMode" flag RTC memory
	readRTCmem();
	
	// --------- BOOT STATISTICS ------------------------
	// read and increase boot statistics (optional)
	if(bootstats == true){
		rtcMem.bootTimes++;
		writeRTCmem();
		if(_serialDebug == true){
			printRTCmem();
		}
	}

	// on first boot of the app run the firstBoot() function
	if(strcmp(config.compDate,_compDate) != 0){
		firstBoot(ea);
	}

	// process added fields
	processField();
	
	
	//---------- SELECT BOARD MODE -----------------------------
	#if WIFIMAN == true
		if (rtcMem.boardMode == 'C') configESP();
	#endif
	
	// --------- READ FULL CONFIG --------------------------
	//readConfig();
	

	// --------- START WIFI --------------------------
	connectNetwork();


	// --------- if automaticUpdate Update --------------------------
	if(config.automaticUpdate == true){
		callHome();
	}
	
	_buttonEntry = millis() + MODE_BUTTON_VERY_LONG_PRESS;    // make sure the timedifference during startup is bigger than 10 sec. Otherwise it will go either in config mode or calls home
	_appState = AppStateNoPress;

	// ----------- END SPECIFIC SETUP CODE ----------------------------
	#if DEBUG_LVL >= 1
		DEBUG_PRINT(F("\n\n\n\n\n"));
	#endif
}


//---------- RTC MEMORY FUNCTIONS ----------
bool IOTAppStory::readRTCmem() {
	bool ret = true;
	system_rtc_mem_read(RTCMEMBEGIN, &rtcMem, sizeof(rtcMem));
	if (rtcMem.markerFlag != MAGICBYTE) {
		rtcMem.markerFlag = MAGICBYTE;
		rtcMem.bootTimes = 0;
		rtcMem.boardMode = 'N';
		system_rtc_mem_write(RTCMEMBEGIN, &rtcMem, sizeof(rtcMem));
		ret = false;
	}
	return ret;
}

void IOTAppStory::writeRTCmem() {
	rtcMem.markerFlag = MAGICBYTE;
	system_rtc_mem_write(RTCMEMBEGIN, &rtcMem, sizeof(rtcMem));
}

void IOTAppStory::printRTCmem() {
	DEBUG_PRINTF_P(PSTR(" rtcMem\n markerFlag: %c\n"), rtcMem.markerFlag);
	DEBUG_PRINTF_P(PSTR(" bootTimes since powerup: "));
	DEBUG_PRINT(rtcMem.bootTimes);
	DEBUG_PRINTF_P(PSTR("\n boardMode: %c\n"), rtcMem.boardMode);
	DEBUG_PRINTLN(FPSTR(SER_DEV));
}

void IOTAppStory::configESP() {
	readConfig();
	//connectNetwork();
	
	DEBUG_PRINT(F("\n\n\n\nC O N F I G U R A T I O N   M O D E\n"));
	//DEBUG_PRINTLN(system_get_free_heap_size());

	initWiFiManager();

	//--------------- buttonbuttonLoop ----------------------------------
	while (1) {
		//if ((*buttonChanged) && (*buttonTime) > 4000) espRestart('N', "Back to normal mode");  // long button press > 4sec
		yield();
		loopWiFiManager();
	}
} 


void IOTAppStory::connectNetwork() {
	DEBUG_PRINTLN(F(" Connecting to WiFi AP"));

	WiFi.mode(WIFI_STA);
	if (!isNetworkConnected()) {
		#if DEBUG_LVL >= 1
			DEBUG_PRINTLN(F("\n No Connection. Try to connect with saved PW"));
		#endif
			WiFi.begin(config.ssid, config.password);  // if password forgotten by firmwware try again with stored PW
		#if WIFIMAN == true
			if (!isNetworkConnected()){  // still no success
				if(config.automaticConfig || (digitalRead(_modeButton) == LOW)) espRestart('C', " No Connection. Going into Configuration Mode");
			}
		#endif
	}
	#if DEBUG_LVL >= 1
		if (!isNetworkConnected()) {
			// this point is only reached if config.automaticConfig = false
			DEBUG_PRINT(F("\n WiFi NOT connected\n Continuing anyway"));
		}else{
			#if DEBUG_LVL >= 2
				DEBUG_PRINT(F("\n WiFi connected\n Device MAC: "));
				DEBUG_PRINTLN(WiFi.macAddress());
			#else
				DEBUG_PRINTLN("");
			#endif
			DEBUG_PRINT(F(" Device IP Address: "));
			DEBUG_PRINTLN(WiFi.localIP());
		}
	#endif

	#if USEMDNS == true
		// Register host name in WiFi and mDNS
		String hostNameWifi = config.boardName;   // boardName is device name
		hostNameWifi.concat(".local");
		wifi_station_set_hostname(config.boardName);
		//   WiFi.hostname(hostNameWifi);
		if (MDNS.begin(config.boardName)) {
		#if DEBUG_LVL >= 1
			DEBUG_PRINT(F(" MDNS responder started: http://"));
			DEBUG_PRINT(hostNameWifi);
		#endif
		#if DEBUG_LVL >= 2
			DEBUG_PRINTLN(F("\n\n To use MDNS Install host software:\n - For Linux, install Avahi (http://avahi.org/)\n - For Windows, install Bonjour (https://commaster.net/content/how-resolve-multicast-dns-windows)\n - For Mac OSX and iOS support is built in through Bonjour already"));
		#endif
		} else {
			espRestart('N', "MDNS not started");
		}
	#endif

	#if DEBUG_LVL >= 1
		DEBUG_PRINTLN(FPSTR(SER_DEV));
	#endif
}

// Wait until network is connected. Returns false if not connected after MAX_WIFI_RETRIES retries
bool IOTAppStory::isNetworkConnected() {
	int retries = MAX_WIFI_RETRIES;
	DEBUG_PRINT(" ");
	while (WiFi.status() != WL_CONNECTED && retries-- > 0 ) {
		delay(500);
		DEBUG_PRINT(".");
	}
	
	return (retries > 0);
}


//---------- IOTappStory FUNCTIONS ----------
bool IOTAppStory::callHome(bool spiffs /*= true*/) {
	// update from IOTappStory.com
	bool updateHappened = false;
	
	#if DEBUG_LVL >= 2
		DEBUG_PRINTF_P(PSTR(" Calling Home\n Current App: %s\n"), _firmware.c_str());
	#endif

	if (_firmwareUpdateCheckCallback){
		_firmwareUpdateCheckCallback();
	}
	
	// try to update from address 1
	updateHappened = iotUpdater(0,0);
	
	// if address 1 was unsuccesfull try address 2
	if (updateHappened == false) {
		updateHappened = iotUpdater(0,1);
	}
	


	if (spiffs) {
		
		// try to update spiffs from address 1
		updateHappened = iotUpdater(1,0);
		
		// if address 1 was unsuccesfull try address 2
		if (updateHappened == false) {
			updateHappened = iotUpdater(1,1);
		}
	}
	#if DEBUG_LVL >= 2
		DEBUG_PRINTLN(F("\n Returning from IOTAppStory.com"));
	#endif
	#if DEBUG_LVL >= 1
		DEBUG_PRINTLN(FPSTR(SER_DEV));
	#endif

	return updateHappened;
}

bool IOTAppStory::iotUpdater(bool spiffs, bool loc) {
	String url = "";
	bool httpSwitch = false;
	
	if(HTTPS == true && system_get_free_heap_size() > HEAPFORHTTPS){
		httpSwitch = true;
	}
	
	#if DEBUG_LVL == 1
		DEBUG_PRINT(F(" Checking for "));
	#endif
	#if DEBUG_LVL >= 2
		DEBUG_PRINT(F("\n Checking for "));
	#endif
	#if DEBUG_LVL >= 1
		if(spiffs == false){
			// type = sketch
			DEBUG_PRINT(F("App(Sketch)"));
		}
		if(spiffs == true){
			// type = spiffs
			DEBUG_PRINT(F("SPIFFS"));
		}
	#endif

	#if DEBUG_LVL >= 2
		DEBUG_PRINT(F(" updates from: "));
	#else
		DEBUG_PRINT(F(" updates"));
	#endif

	if(httpSwitch == true){
		url = F("https://");
	}else{
		url = F("http://");
	}

	if(loc == 0){
		// location 1
		url += config.HOST1;
		url += config.FILE1;
	}else{
		// location 2
		url += FPSTR(HOST2);
		url += FPSTR(FILE2);
	}
	
	#if DEBUG_LVL >= 2
		DEBUG_PRINTLN(url);
	#else
		DEBUG_PRINTLN("");		
	#endif
	
    HTTPClient http;
	if(httpSwitch == true){
		http.begin(url, config.sha1); // 								<<--  https We need to free up RAM first!
	}else{
		http.begin(url);
	}
	
	
    // use HTTP/1.0 the update handler does not support transfer encoding
    http.useHTTP10(true);
    http.setTimeout(8000);
    http.setUserAgent(F("ESP8266-http-Update"));


    http.addHeader(F("x-ESP8266-STA-MAC"), WiFi.macAddress());
    http.addHeader(F("x-ESP8266-act-id"), String(config.actCode));

	
    http.addHeader(F("x-ESP8266-free-space"), String(ESP.getFreeSketchSpace()));
    http.addHeader(F("x-ESP8266-sketch-size"), String(ESP.getSketchSize()));
    http.addHeader(F("x-ESP8266-sketch-md5"), String(ESP.getSketchMD5()));

	
	http.addHeader(F("x-ESP8266-flashchip-size"), String(ESP.getFlashChipRealSize()));
    http.addHeader(F("x-ESP8266-flashchip-id"), String(ESP.getFlashChipId()));
	http.addHeader(F("x-ESP8266-chip-id"), String(ESP.getChipId()));
	
    http.addHeader(F("x-ESP8266-core-version"), ESP.getCoreVersion());
	http.addHeader(F("x-ESP8266-version"), _firmware);
    if(spiffs) {
        http.addHeader(F("x-ESP8266-mode"), F("spiffs"));
    } else {
        http.addHeader(F("x-ESP8266-mode"), F("sketch"));
    }

    


    // track these headers for later use
    const char * headerkeys[] = { "x-MD5", "x-name" };
    size_t headerkeyssize = sizeof(headerkeys) / sizeof(char*);
    http.collectHeaders(headerkeys, headerkeyssize);


    int code = http.GET();
    int len = http.getSize();

    if(code == HTTP_CODE_OK){
		
		ESP8266HTTPUpdate ESPhttpUpdate;
		ESPhttpUpdate.rebootOnUpdate(true);
		
		#if DEBUG_LVL >= 1
			DEBUG_PRINT(F(" Downloading update..."));
		#endif
		
		if (_firmwareUpdateDownloadCallback){
			_firmwareUpdateDownloadCallback();
		}

		#if DEBUG_LVL >= 3
			DEBUG_PRINTLN("[httpUpdate] Header read fin.\n");
			DEBUG_PRINTLN("[httpUpdate] Server header:\n");
			DEBUG_PRINTF_P("[httpUpdate]  - code: %d\n", code);
			DEBUG_PRINTF_P("[httpUpdate]  - len: %d\n", len);

			if(http.hasHeader("x-MD5")) {
				DEBUG_PRINTF_P("[httpUpdate]  - MD5: %s\n", http.header("x-MD5").c_str());
			}

			DEBUG_PRINTLN("[httpUpdate] ESP8266 info:\n");
			DEBUG_PRINTF_P("[httpUpdate]  - free Space: %d\n", ESP.getFreeSketchSpace());
			DEBUG_PRINTF_P("[httpUpdate]  - current Sketch Size: %d\n", ESP.getSketchSize());
			DEBUG_PRINTF_P("[httpUpdate]  - current version: %s\n", _firmware.c_str() );
		#endif

		ESPhttpUpdate.handleUpdate(http, len, spiffs);
		return true;

	}else{
		#if DEBUG_LVL >= 1
			DEBUG_PRINTLN(http.getString());
		#endif

		if(code == HTTP_CODE_NOT_MODIFIED){
			return true;
		}else{
			if (_firmwareUpdateErrorCallback){
				_firmwareUpdateErrorCallback();
			}
			return false;
		}

	}

	http.end();
}


//---------- WIFIMANAGER COMMON FUNCTIONS
void IOTAppStory::initWiFiManager() {
	//if(_serialDebug == true){WiFi.printDiag(Serial);} //Remove this line if you do not want to see WiFi password printed

	if (WiFi.SSID() == "") {
		DEBUG_PRINTLN(F("We haven't got any access point credentials, so get them now"));
	}else{
		WiFi.mode(WIFI_STA); // Force to station mode because if device was switched off while in access point mode it will start up next time in access point mode.
		unsigned long startedAt = millis();
		int connRes = WiFi.waitForConnectResult();
		float waited = (millis() - startedAt);

		DEBUG_PRINTF_P(PSTR("After waiting %d secs in setup(), the connection result is %d\n"), waited / 1000, connRes);
	}

	if (WiFi.status() != WL_CONNECTED) {
		DEBUG_PRINTLN(F("Failed to connect"));
	}else{
		DEBUG_PRINT(F("Local ip: "));
		DEBUG_PRINTLN(WiFi.localIP());
	}
	
}


// add char array field to the wifi configuration page and add value to eeprom
void IOTAppStory::addField(char* &defaultVal,const char *fieldIdName,const char *fieldLabel, int length){
	_nrXF++;
	
	// add values to the fieldstruct
	fieldStruct[_nrXF-1].fieldIdName = fieldIdName;
	fieldStruct[_nrXF-1].fieldLabel = fieldLabel;
	fieldStruct[_nrXF-1].varPointer = &defaultVal;
	fieldStruct[_nrXF-1].length = length+1;
}

// add char array field to the wifi configuration page and add value to eeprom
void IOTAppStory::processField(){

	// to prevent longer then default values overwriting each other
	// temp save value, overwrite variable with longest value posible
	// and then resave the temp value to the original variable
	if(_nrXF > 0){
		char* tempValue[_nrXF];
		
		for(unsigned int nr = 0; nr < _nrXF; nr++){
			tempValue[nr] = (*fieldStruct[nr].varPointer);
			
			char* tmpVal = new char[fieldStruct[nr].length];
			for (int i = 0; i < fieldStruct[nr].length-1; i++) {
				tmpVal[i] = 't';
			}
			
			(*fieldStruct[nr].varPointer) = tmpVal;
		}
		
		for(unsigned int nr = 0; nr < _nrXF; nr++){
			//String().toCharArray(, fieldStruct[nr].length);
			
			strcpy((*fieldStruct[nr].varPointer), tempValue[nr]);
		}
	}
		
	if(_nrXF > 0){
		DEBUG_PRINTLN(F(" Processing added fields\n ID | LABEL                          | LEN |  EEPROM LOC  | DEFAULT VALUE                  | CURRENT VALUE                  | STATUS\n"));
		EEPROM.begin(EEPROM_SIZE);
		
		for (unsigned int nr = 1; nr <= _nrXF; nr++){
			delay(100);
			// loop through the fields struct array
			
			int prevTotLength = 0;
			for(unsigned int i = 0; i < (nr-1); i++){
				prevTotLength += fieldStruct[i].length;
			}
			const int sizeOfVal = fieldStruct[nr-1].length;
			const int sizeOfConfig = sizeof(config)+2;
			const int eeBeg = sizeOfConfig+prevTotLength+nr+((nr-1)*2);
			const int eeEnd = sizeOfConfig+(prevTotLength+sizeOfVal)+nr+1+((nr-1)*2);

			DEBUG_PRINTF_P(PSTR(" %02d | %-30s | %03d | %04d to %04d | %-30s | "), nr, fieldStruct[nr-1].fieldLabel, fieldStruct[nr-1].length-1, eeBeg, eeEnd, (*fieldStruct[nr-1].varPointer));

			char* eepVal = new char[fieldStruct[nr-1].length + 1];
			char* tmpVal = new char[fieldStruct[nr-1].length + 1];
			for (int i = 0; i < fieldStruct[nr-1].length; i++) {
				eepVal[i] = 0;
				tmpVal[i] = 0;
			}
			if ((*fieldStruct[nr-1].varPointer) != NULL) {
				strncpy(tmpVal, (*fieldStruct[nr-1].varPointer), fieldStruct[nr-1].length);
			}

			// read eeprom, check for MAGICEEP and get the updated value if present
			if(EEPROM.read(eeBeg) == MAGICEEP[0] && EEPROM.read(eeEnd) == '^'){

				for (unsigned int t = eeBeg; t <= eeEnd; t++){
					// start after MAGICEEP
					if(t != eeBeg && t != eeEnd && EEPROM.read(t) != 0){
						*((char*)eepVal + (t-eeBeg)-1) = EEPROM.read(t);
					}
				}
				
				// if eeprom value is different update the ret value
				if(strcmp(eepVal, (*fieldStruct[nr-1].varPointer)) != 0){
				//if(String(eepVal) != String((*fieldStruct[nr-1].varPointer))){
					DEBUG_PRINTF_P(PSTR("%-30s | OVERWRITTEN"), eepVal);

					(*fieldStruct[nr-1].varPointer) = eepVal;
				}else{
					DEBUG_PRINTF_P(PSTR("%-30s | DEFAULT"), (*fieldStruct[nr-1].varPointer));
				}

			}else{
				DEBUG_PRINTF_P(PSTR("%-30s | WRITTING TO EEPROM"), (*fieldStruct[nr-1].varPointer));

				// add MAGICEEP to value and write to eeprom
				for (unsigned int t = eeBeg; t <= eeEnd; t++){
					if(t == eeBeg){
						EEPROM.put(t, MAGICEEP[0]);						// magic begin marker
					}else  if(t == eeEnd){
						EEPROM.put(t, '^');								// magic end marker
					}else{
						EEPROM.put(t, *((char*)tmpVal + (t-eeBeg)-1));	// byte of value`
					}
				}
			}
		
			// add values to the fieldstruct
			//fieldStruct[_nrXF-1].varPointer = (*fieldStruct[nr-1].varPointer);
			DEBUG_PRINTLN("");
		}
		EEPROM.end();
		DEBUG_PRINTLN(FPSTR(SER_DEV));
	}
}
int IOTAppStory::dPinConv(String orgVal){
	#if defined ESP8266_OAK

		// DEBUG_PRINTLN("- Digistump OAK -");
		if      (orgVal == "P1"  || orgVal == "5")    return P1;
		else if (orgVal == "P2"  || orgVal == "0")    return P2;
		else if (orgVal == "P3"  || orgVal == "3")    return P3;
		else if (orgVal == "P4"  || orgVal == "1")    return P4;
		else if (orgVal == "P5"  || orgVal == "4")    return P5;
		else if (orgVal == "P6"  || orgVal == "15")   return P6;
		else if (orgVal == "P7"  || orgVal == "13")   return P7;
		else if (orgVal == "P8"  || orgVal == "12")   return P8;
		else if (orgVal == "P9"  || orgVal == "14")   return P9;
		else if (orgVal == "P10" || orgVal == "16")   return P10;
		else if (orgVal == "P11" || orgVal == "17")   return P11;
		else                                          return P1;

	#elif defined ESP8266_WEMOS_D1MINI || defined ESP8266_WEMOS_D1MINILITE || defined ESP8266_WEMOS_D1MINIPRO || defined ESP8266_NODEMCU || defined WIFINFO

		// DEBUG_PRINTLN("- Special ESP's -");
		if      (orgVal == "D0"  || orgVal == "16")   return D0;
		else if (orgVal == "D1"  || orgVal == "5")    return D1;
		else if (orgVal == "D2"  || orgVal == "4")    return D2;
		else if (orgVal == "D3"  || orgVal == "0")    return D3;
		else if (orgVal == "D4"  || orgVal == "2")    return D4;
		else if (orgVal == "D5"  || orgVal == "14")   return D5;
		else if (orgVal == "D6"  || orgVal == "12")   return D6;
		else if (orgVal == "D7"  || orgVal == "13")   return D7;
		else if (orgVal == "D8"  || orgVal == "15")   return D8;
		else if (orgVal == "D9"  || orgVal == "3")    return D9;
		else if (orgVal == "D10" || orgVal == "1")    return D10;
		else                                          return D0;
		
	#else

		// DEBUG_PRINTLN("- Generic ESP's -");
		
		// There are NO constants for the generic eps's!
		// But people makes mistakes when entering pin nr's in config
		// And if you originally developed your code for "Special ESP's"
		// this part makes makes it compatible when compiling for "Generic ESP's"
		
		if      (orgVal == "D0"  || orgVal == "16")   return 16;
		else if (orgVal == "D1"  || orgVal == "5")    return 5;
		else if (orgVal == "D2"  || orgVal == "4")    return 4;
		else if (orgVal == "D3"  || orgVal == "0")    return 0;
		else if (orgVal == "D4"  || orgVal == "2")    return 2;
		else if (orgVal == "D5"  || orgVal == "14")   return 14;
		else if (orgVal == "D6"  || orgVal == "12")   return 12;
		else if (orgVal == "D7"  || orgVal == "13")   return 13;
		else if (orgVal == "D8"  || orgVal == "15")   return 15;
		else if (orgVal == "D9"  || orgVal == "3")    return 3;
		else if (orgVal == "D10" || orgVal == "1")    return 1;
		else                                          return 16;

	#endif
}

void IOTAppStory::loopWiFiManager() {
	if (_configModeCallback){
		_configModeCallback();
	}
	
	WiFiManagerParameter parArray[MAXNUMEXTRAFIELDS];
	
	for(unsigned int i = 0; i < _nrXF; i++){
		// add the WiFiManagerParameter to parArray so it can be referenced to later
		parArray[i] = WiFiManagerParameter(fieldStruct[i].fieldIdName, fieldStruct[i].fieldLabel, (*fieldStruct[i].varPointer), fieldStruct[i].length);
	}

	// Initialize WiFIManager
	WiFiManager wifiManager;
	wifiManager.config = &config;
	//wifiManager.addParameter(&p_hint);

	//add all parameters here
	for(unsigned int i = 0; i < _nrXF; i++){
		wifiManager.addParameter(&parArray[i]);
	}

	// Sets timeout in seconds until configuration portal gets turned off.
	// If not specified device will remain in configuration mode until
	// switched off via webserver or device is restarted.
	wifiManager.setConfigPortalTimeout(420);

	// It starts an access point
	// and goes into a blocking loop awaiting configuration.
	// Once the user leaves the portal with the exit button
	// processing will continue
	if (!wifiManager.startConfigPortal()) {
		DEBUG_PRINTLN(F(" Not connected to WiFi but continuing anyway."));
	}else{
		// If you get here you have connected to the WiFi
		DEBUG_PRINTLN(F(" Connected... :-)"));
	}
	// Getting posted form values and overriding local variables parameters
	// Config file is written

	//DEBUG_PRINTLN(" ---------------------------");

	//add all parameters here
	for(unsigned int i = 0; i < _nrXF; i++){
		strcpy((*fieldStruct[i].varPointer), parArray[i].getValue());
	}
	//wifiManager.actCode.toCharArray(config.actCode,7);
	//DEBUG_PRINTLN(wifiManager.actCode);
	
	writeConfig(true);
	readConfig();  // read back to fill all variables
	espRestart('N', "Configuration finished"); //Normal Operation
}

//---------- MISC FUNCTIONS ----------
void IOTAppStory::espRestart(char mmode, const char* message) {
	while (isModeButtonPressed()) yield();    // wait till GPIOo released
	delay(500);
	
	rtcMem.boardMode = mmode;
	writeRTCmem();
	//system_rtc_mem_write(RTCMEMBEGIN + 100, &mmode, 1);
	//system_rtc_mem_read(RTCMEMBEGIN + 100, &boardMode, 1);

	DEBUG_PRINTLN("");
	DEBUG_PRINTLN(message);

	ESP.restart();
}

void IOTAppStory::eraseFlash(unsigned int eepFrom, unsigned int eepTo) {
	DEBUG_PRINTF_P(PSTR(" Erasing Flash...\n From %4d to %4d\n"), eepFrom, eepTo);

	EEPROM.begin(EEPROM_SIZE);
	for (unsigned int t = eepFrom; t < eepTo; t++) EEPROM.write(t, 0);
	EEPROM.end();
}

//---------- CONFIGURATION PARAMETERS ----------
void IOTAppStory::writeConfig(bool wifiSave) {
	//DEBUG_PRINTLN(" ------------------ Writing Config --------------------------------");
	if (WiFi.psk() != "") {
		WiFi.SSID().toCharArray(config.ssid, STRUCT_CHAR_ARRAY_SIZE);
		WiFi.psk().toCharArray(config.password, STRUCT_CHAR_ARRAY_SIZE);
		#if DEBUG_EEPROM_CONFIG
			DEBUG_PRINT_P(PSTR("Stored %s\n %s\n"), config.ssid, config.password);   // actCode
		#endif		
	}
	
	EEPROM.begin(EEPROM_SIZE);
	/*
	config.magicBytes[0] = MAGICBYTES[0];
	config.magicBytes[1] = MAGICBYTES[1];
	config.magicBytes[2] = MAGICBYTES[2];
	*/
	
	// WRITE CONFIG TO EEPROM
	for (unsigned int t = 0; t < sizeof(config); t++) {
		EEPROM.write(t, *((char*)&config + t));
		
		#if DEBUG_EEPROM_CONFIG
			// DEBUG (show all config EEPROM slots in one line)
			DEBUG_PRINT(GetCharToDisplayInDebug(*((char*)&config + t)));
		#endif
		
	}
	EEPROM.commit();
	#if DEBUG_EEPROM_CONFIG
		DEBUG_PRINTLN();
	#endif
	
	if(wifiSave == true && _nrXF > 0){
		// LOOP THROUGH ALL THE ADDED FIELDS, CHECK VALUES AND IF NECESSARY WRITE TO EEPROM
		for (unsigned int nr = 1; nr <= _nrXF; nr++){
			
			int prevTotLength = 0;
			for(unsigned int i = 0; i < (nr-1); i++){
				prevTotLength += fieldStruct[i].length;
			}
			const int sizeOfVal = fieldStruct[nr-1].length;
			const int sizeOfConfig = sizeof(config)+2;
			const int eeBeg = sizeOfConfig+prevTotLength+nr+((nr-1)*2);
			const int eeEnd = sizeOfConfig+(prevTotLength+sizeOfVal)+nr+1+((nr-1)*2);
			
			/*
			DEBUG_PRINT(" EEPROM space: ");
			DEBUG_PRINT(eeBeg);
			DEBUG_PRINT("  to ");
			DEBUG_PRINTLN(eeEnd);
			DEBUG_PRINTLN(" ");
			DEBUG_PRINT(" Size: ");
			DEBUG_PRINTLN(sizeOfVal);
			DEBUG_PRINTLN((*fieldStruct[nr-1].varPointer));
			*/
			
			char* tmpVal = new char[sizeOfVal + 1];
			for (int i = 0; i <= sizeOfVal; i++) {
				tmpVal[i] = 0;
			}
			if ((*fieldStruct[nr-1].varPointer) != NULL) {
				strncpy(tmpVal, (*fieldStruct[nr-1].varPointer), fieldStruct[nr-1].length);
			}

			//DEBUG_PRINTLN("^^");
			//DEBUG_PRINTLN(tmpVal);
			//DEBUG_PRINTLN(" ");
			
			// check for MAGICEEP
			if(EEPROM.read(eeBeg) == MAGICEEP[0] && EEPROM.read(eeEnd) == '^'){
				// add MAGICEEP to value and write to eeprom
				for (unsigned int t = eeBeg; t <= eeEnd; t++){
					char valueTowrite;
					
					if(t == eeBeg){
						valueTowrite = MAGICEEP[0];
					}else if(t == eeEnd){
						valueTowrite = '^';
					}else{
						valueTowrite = *((char*)tmpVal + (t-eeBeg)-1);
					}
					EEPROM.put(t, valueTowrite);
					
					#if DEBUG_EEPROM_CONFIG
						// DEBUG (show all wifiSave EEPROM slots in one line)
						DEBUG_PRINT(GetCharToDisplayInDebug(valueTowrite));
					#endif
		
				}
			}
			EEPROM.commit();
		}
	}
	
	EEPROM.end();
}

bool IOTAppStory::readConfig() {
	//DEBUG_PRINTLN(" ------------------ Reading Config --------------------------------");

	boolean ret = false;
	EEPROM.begin(EEPROM_SIZE);
	long magicBytesBegin = sizeof(config) - 4; 								// Magic bytes at the end of the structure

	if (EEPROM.read(magicBytesBegin) == MAGICBYTES[0] && EEPROM.read(magicBytesBegin + 1) == MAGICBYTES[1] && EEPROM.read(magicBytesBegin + 2) == MAGICBYTES[2]) {
		DEBUG_PRINTLN(F(" EEPROM Configuration found"));
		for (unsigned int t = 0; t < sizeof(config); t++) {
			char valueReaded = EEPROM.read(t);
			*((char*)&config + t) = valueReaded;
			
			#if DEBUG_EEPROM_CONFIG
				// DEBUG (show all config EEPROM slots in one line)
				DEBUG_PRINT(GetCharToDisplayInDebug(valueReaded));
			#endif
		}
		EEPROM.end();
		#if DEBUG_EEPROM_CONFIG
			DEBUG_PRINTLN();
		#endif
		ret = true;

	} else {
		DEBUG_PRINTLN(F(" EEPROM Configuration NOT FOUND!!!!"));
		writeConfig();
		ret = false;
	}
	
	_configReaded = true;
	
	return ret;
}

void IOTAppStory::updateLoop() {
   if (_callHome && millis() - _lastCallHomeTime > _callHomeInterval) {
      this->callHome();
      _lastCallHomeTime = millis();
   }
}

ModeButtonState IOTAppStory::buttonLoop() {
   this->updateLoop();
   return getModeButtonState();
}

void IOTAppStory::saveConfigCallback () {
	writeConfig();
}

bool IOTAppStory::isModeButtonPressed() {
	return digitalRead(_modeButton) == LOW; // LOW means flash button IS pressed
}

ModeButtonState IOTAppStory::getModeButtonState() {
	
	while(true)
	{
		unsigned long buttonTime = millis() - _buttonEntry;
	
		switch(_appState) {
		case AppStateNoPress:
			if (isModeButtonPressed()) {
				_buttonEntry = millis();
				_appState = AppStateWaitPress;
				continue;
			}
			return ModeButtonNoPress;
		
		case AppStateWaitPress:
			if (buttonTime > MODE_BUTTON_SHORT_PRESS) {
				_appState = AppStateShortPress;
				if (_shortPressCallback)
					_shortPressCallback();
				continue;
			}
			if (!isModeButtonPressed()) {
				_appState = AppStateNoPress;
			}
			return ModeButtonNoPress;
		
		case AppStateShortPress:
			if (buttonTime > MODE_BUTTON_LONG_PRESS) {
				_appState = AppStateLongPress;
				if (_longPressCallback)
					_longPressCallback();
				continue;
			}
			if (!isModeButtonPressed()) {
				_appState = AppStateFirmwareUpdate;
				continue;
			}
			return ModeButtonShortPress;

		case AppStateLongPress:
			if (buttonTime > MODE_BUTTON_VERY_LONG_PRESS) {
				_appState = AppStateVeryLongPress;
				if (_veryLongPressCallback)
					_veryLongPressCallback();
				continue;
			}
#if WIFIMAN == true
			if (!isModeButtonPressed()) {
				_appState = AppStateConfigMode;
				continue;
			}
#endif
			return ModeButtonLongPress;
	
		case AppStateVeryLongPress:
			if (!isModeButtonPressed()) {
				_appState = AppStateNoPress;
				if (_noPressCallback)
					_noPressCallback();
				continue;
			}
			return ModeButtonVeryLongPress;
		
		case AppStateFirmwareUpdate:
			_appState = AppStateNoPress;
			//DEBUG_PRINTLN("Calling Home");
			callHome();
			continue;
#if WIFIMAN == true	
		case AppStateConfigMode:
			_appState = AppStateNoPress;
			DEBUG_PRINTLN(F(" Entering in Configuration Mode"));
			espRestart('C', "Going into Configuration Mode");
			continue;
#endif
		}
	}
	return ModeButtonNoPress; // will never reach here (used just to avoid compiler warnings)
}


void IOTAppStory::setCallHome(bool callHome) {
   _callHome = callHome;
}

void IOTAppStory::setCallHomeInterval(unsigned long interval) {
   _callHomeInterval = interval * 1000; //Convert to millis so users can pass seconds to this function
}


void IOTAppStory::onFirstBoot(THandlerFunction value) {
	_firstBootCallback = value;
}

void IOTAppStory::onModeButtonNoPress(THandlerFunction value) {
	_noPressCallback = value;
}

void IOTAppStory::onModeButtonShortPress(THandlerFunction value) {
	_shortPressCallback = value;
}

void IOTAppStory::onModeButtonLongPress(THandlerFunction value) {
	_longPressCallback = value;
}

void IOTAppStory::onModeButtonVeryLongPress(THandlerFunction value) {
	_veryLongPressCallback = value;
}

void IOTAppStory::onFirmwareUpdateCheck(THandlerFunction value) {
	_firmwareUpdateCheckCallback = value;
}
void IOTAppStory::onFirmwareUpdateDownload(THandlerFunction value) {
	_firmwareUpdateDownloadCallback = value;
}
void IOTAppStory::onFirmwareUpdateError(THandlerFunction value) {
	_firmwareUpdateErrorCallback = value;
}

void IOTAppStory::onConfigMode(THandlerFunction value) {
	_configModeCallback = value;
}

