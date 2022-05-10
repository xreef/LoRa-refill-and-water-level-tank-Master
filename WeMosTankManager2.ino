#include "Arduino.h"

#include <Wire.h>
#include "include/splash.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "PCF8574.h"
#include "LoRa_E32.h"
#include <ArduinoJson.h>

#include <Fonts/FreeMono12pt7b.h>

#include "include/states.h"
// #include "include/eng.h"
#include "include/ita.h"
#include "include/icons.h"

#include <stdlib.h>
#include "RTClib.h"
#include <EEPROM.h>

#define SERVER_ADDH 0x00
#define SERVER_ADDL 0x03
#define SERVER_CHANNEL 0x04

#define CLIENT_ADDH 0x00
#define CLIENT_ADDL 0x04
#define CLIENT_CHANNEL 0x04

void handleScreen(SCREEN screen);
SCREEN clickMenu(SCREEN screen, int state);
void startPump();
void stopPump();
//LoRa_E32 e32ttl(D3, D4, D5, D7, D6);
LoRa_E32 e32ttl(&Serial, D5, D0, D8);
//LoRa_E32 e32ttl(&Serial, D3, D0, D8);

#define ACTIVATE_OTA
#ifdef ACTIVATE_OTA
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

bool connected = false;
// #include <NTPClient.h>
// #include <WiFiUdp.h>

#endif

#define TANK_DEBUG
// Define where debug output will be printed.
#define DEBUG_PRINTER Serial1

// Setup debug printing macros.
#ifdef TANK_DEBUG
	#define DEBUG_PRINT(...) { DEBUG_PRINTER.print(__VA_ARGS__); }
	#define DEBUG_PRINTF(...) { DEBUG_PRINTER.printf(__VA_ARGS__); }
	#define DEBUG_PRINTLN(...) { DEBUG_PRINTER.println(__VA_ARGS__); }
#else
	#define DEBUG_PRINT(...) {}
	#define DEBUG_PRINTF(...) {}
	#define DEBUG_PRINTLN(...) {}
#endif

#define ENCODER_PIN_A P0
#define ENCODER_PIN_B P1
#define ENCODER_BUTTON P2
//#define ENCODER_PIN_A P0
//#define ENCODER_PIN_B P1
//#define ENCODER_BUTTON P2

#define INTERRUPTED_PIN D7
#define RELAY_PIN D6

#define ERROR_LED D3

//void ICACHE_RAM_ATTR updateEncoder();
void IRAM_ATTR updateEncoder();

RTC_DS1307 rtc;

// initialize library
PCF8574 pcf8574(0x38, INTERRUPTED_PIN, updateEncoder);

volatile long encoderValue = 0;
bool pompaAttiva = false;

uint8_t encoderButtonVal = LOW;
uint8_t encoderButtonValPrec = LOW;

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
//Adafruit_SSD1306 display(OLED_RESET);
//The setup function is called once at startup of the sketch
void printParameters(struct Configuration configuration);
ResponseStatus sendPumpMessageToClient(String action, bool wakeUp = false);

SCREEN currentScreen = SCREEN_MAIN;
int screenEncoderSizeMax = screenEncoderSize[currentScreen];

void renderScreen(SCREEN screen);

String errorMessage = "";

unsigned long pumpStartTime = 0;
unsigned long pumpStopTime = 0;

unsigned long lastPacketClient = 0;
unsigned long lastMillisMessageReceived = 0;
unsigned long pumpActiveTime = 0;

MAIN_CONTROL mainControl = CONTROL_NONE;
OPERATION_MODE operationalSelected = OPERATION_NORMAL;
ACION_MODE actionSelected = ACTION_AUTO;

bool changed = false;
bool changedButton = false;

void setError(const String errorMessagePar){
	display.clearDisplay();

	errorMessage = errorMessagePar;
	currentScreen = SCREEN_ERROR;
	changed = true;
}

#define EE_TIME_START 0
#define EE_TIME_END 5

void eeWriteInt(int pos, int val) {
    byte* p = (byte*) &val;
    EEPROM.write(pos, *p);
    EEPROM.write(pos + 1, *(p + 1));
    EEPROM.write(pos + 2, *(p + 2));
    EEPROM.write(pos + 3, *(p + 3));
    EEPROM.commit();
}

int eeGetInt(int pos) {
  int val;
  byte* p = (byte*) &val;
  *p        = EEPROM.read(pos);
  *(p + 1)  = EEPROM.read(pos + 1);
  *(p + 2)  = EEPROM.read(pos + 2);
  *(p + 3)  = EEPROM.read(pos + 3);
  return val;
}

int startTimeGlob = 11;
int endTimeGlob = 24;

void setup()
{

#ifdef TANK_DEBUG
	DEBUG_PRINTER.begin(115200);

	//   while (!DEBUG_PRINTER) {
	//     ; // wait for serial port to connect. Needed for native USB
	//   }		// encoder pins
#endif

#ifdef ACTIVATE_OTA
	const char* ssid = "reef-casa-sopra";
	const char* password = "aabbccdd77";

	delay(100);

#ifdef TANK_DEBUG
	DEBUG_PRINTER.flush();
#endif

	DEBUG_PRINTLN("Booting");
	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, password);
	if (WiFi.waitForConnectResult() != WL_CONNECTED) {
		DEBUG_PRINTLN("Connection Failed! Rebooting...");
	  delay(5000);
	//   ESP.restart();
	} else {
	delay(1000);
	DEBUG_PRINTLN("Ready");
	DEBUG_PRINT("IP address: ");
	DEBUG_PRINTLN(WiFi.localIP());

	ArduinoOTA.onStart([]() {
		String type;
		if (ArduinoOTA.getCommand() == U_FLASH) {
		  type = "sketch";
		} else { // U_FS
		  type = "filesystem";
		}

		// NOTE: if updating FS this would be the place to unmount FS using FS.end()
		DEBUG_PRINTLN("Start updating " + type);
	  });
	  ArduinoOTA.onEnd([]() {
		DEBUG_PRINTLN("\nEnd");
	  });
	  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
		DEBUG_PRINTF("Progress: %u%%\r\n", (progress / (total / 100)));
	  });
	  ArduinoOTA.onError([](ota_error_t error) {
		DEBUG_PRINTF("Error[%u]: ", error);
		if (error == OTA_AUTH_ERROR) {
		  DEBUG_PRINTLN("Auth Failed");
		} else if (error == OTA_BEGIN_ERROR) {
		  DEBUG_PRINTLN("Begin Failed");
		} else if (error == OTA_CONNECT_ERROR) {
		  DEBUG_PRINTLN("Connect Failed");
		} else if (error == OTA_RECEIVE_ERROR) {
		  DEBUG_PRINTLN("Receive Failed");
		} else if (error == OTA_END_ERROR) {
		  DEBUG_PRINTLN("End Failed");
		}
	  });
	  ArduinoOTA.begin();
	
		connected = true;
	//   // Define NTP Client to get time
	// 	WiFiUDP ntpUDP;
	// 	NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600, 60000);

	// 	  timeClient.begin();
	// 	if (timeClient.forceUpdate()){
	// 			SERIAL_DEBUG.println("NTP updated!");
	// 	}
	}
#endif

	  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
	  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
//		  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
	    DEBUG_PRINTLN(F("SSD1306 allocation failed"));
	    for(;;); // Don't proceed, loop forever
	  }

	  // Show initial display buffer contents on the screen --
	  // the library initializes this with an Adafruit splash screen.
	  display.clearDisplay();
	  display.drawBitmap((display.width() - splash1_width) / 2, (display.height() - splash1_height) / 2, splash1_data, splash1_width, splash1_height, 1);
	  display.display();

	  DEBUG_PRINTLN();
	  DEBUG_PRINTLN("--------------------------------------------");

	  pinMode(RELAY_PIN, OUTPUT);
	  digitalWrite(RELAY_PIN, LOW);

	  pinMode(ERROR_LED, OUTPUT);
	  digitalWrite(ERROR_LED, HIGH);

		pcf8574.encoder(ENCODER_PIN_A, ENCODER_PIN_B);
		// encoder button
		pcf8574.pinMode(ENCODER_BUTTON, INPUT);

		// Start library
		pcf8574.begin();


		// Startup all pins and UART
		e32ttl.begin();

		ResponseStructContainer c;
		c = e32ttl.getConfiguration();
		Configuration configuration = *(Configuration*) c.data;
		configuration.ADDL = SERVER_ADDL;
		configuration.ADDH = SERVER_ADDH;
		configuration.CHAN = SERVER_CHANNEL;
		configuration.OPTION.fixedTransmission = FT_FIXED_TRANSMISSION;
		configuration.OPTION.wirelessWakeupTime = WAKE_UP_2000;

		configuration.OPTION.fec = FEC_1_ON;
		configuration.OPTION.ioDriveMode = IO_D_MODE_PUSH_PULLS_PULL_UPS;
		configuration.OPTION.transmissionPower = POWER_20;

		configuration.SPED.airDataRate = AIR_DATA_RATE_010_24;
		configuration.SPED.uartBaudRate = UART_BPS_9600;
		configuration.SPED.uartParity = MODE_00_8N1;

		ResponseStatus rs = e32ttl.setConfiguration(configuration, WRITE_CFG_PWR_DWN_SAVE);
		DEBUG_PRINTLN(rs.getResponseDescription());

		c = e32ttl.getConfiguration();
		// It's important get configuration pointer before all other operation
		Configuration configurationNew = *(Configuration*) c.data;
		DEBUG_PRINTLN(c.status.getResponseDescription());
		DEBUG_PRINTLN(c.status.code);

		printParameters(configurationNew);

		c.close();

		  delay(2000); // Pause for 2 seconds

		  // Clear the buffer
		  display.clearDisplay();

		  renderScreen(SCREEN_MAIN);

		  if (c.status.code!=SUCCESS) {
			  setError("LoRa device not respond!!!");
		  }

			if (! rtc.begin()) {
				DEBUG_PRINTLN("Couldn't find RTC");

				setError("Clock error!!!");
			}

			if (! rtc.isrunning()) {
				DEBUG_PRINTLN("RTC is NOT running, let's set the time!");
				// When time needs to be set on a new device, or after a power loss, the
				// following line sets the RTC to the date & time this sketch was compiled
				// rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
				// This line sets the RTC with an explicit date & time, for example to set
				// January 21, 2014 at 3am you would call:
				// rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
				setError("Change battery to internal Clock!!!");
				// #ifdef ARDUINO_OTA
				//   SERIAL_DEBUG.println("Update RTC with NTP!");
				//     rtc.adjust(DateTime(timeClient.getEpochTime()));
				// #endif
			} 
			//   else {
			// 	  DEBUG_PRINTLN("Update RTC with NTP!");
			//     rtc.adjust(DateTime(timeClient.getEpochTime()));
			//   }
	EEPROM.begin(24);
	startTimeGlob = eeGetInt(EE_TIME_START);
	endTimeGlob = eeGetInt(EE_TIME_END);
	if (startTimeGlob<0 || startTimeGlob>48) startTimeGlob = 0;
	if (endTimeGlob<0 || endTimeGlob>48) endTimeGlob = 0;
}


unsigned long timePassed = millis();
unsigned long interval = 1000;
unsigned long maxTimeWithoutPingMessage = 60000;
unsigned long maxTimeWithoutMessage = 1000*60*60;

bool pumpIsActive = false;

bool maxLevel = false;
bool minLevel = false;

float batteryLevel = 0;

bool clientDisconnected = true;

void debugDate(DateTime dt) {
			DEBUG_PRINT(dt.year(), DEC);
		DEBUG_PRINT('/');
		DEBUG_PRINT(dt.month(), DEC);
		DEBUG_PRINT('/');
		DEBUG_PRINT(dt.day(), DEC);
		DEBUG_PRINT(" (");
		// DEBUG_PRINT(daysOfTheWeek[dt.dayOfTheWeek()]);
		DEBUG_PRINT(") ");
		DEBUG_PRINT(dt.hour(), DEC);
		DEBUG_PRINT(':');
		DEBUG_PRINT(dt.minute(), DEC);
		DEBUG_PRINT(':');
		DEBUG_PRINT(dt.second(), DEC);
		DEBUG_PRINTLN();
}

// The loop function is called in an endless loop
void loop()
{

	if (timePassed+interval<millis()){

	if (mainControl == CONTROL_TIMER) {
		DateTime now = rtc.now();
  // January 21, 2014 at 3am you would call:
//   rtc.adjust(DateTime(2022, 2, 25, 16, 4, 0));

		// DateTime intervalStart = DateTime(now.year(), now.month(), now.day(), startTimeGlob/2, startTimeGlob % 2 * 30, 0);
		// DateTime intervalEnd = DateTime(now.year(), now.month(), now.day(), endTimeGlob / 2, endTimeGlob % 2 * 30, 0);
		

		bool isInsideInterval =
			 ((startTimeGlob/2*60)+(startTimeGlob % 2 * 30))<=(now.hour()*60+now.minute())
			 &&
			 ((endTimeGlob/2*60)+(endTimeGlob % 2 * 30))>=(now.hour()*60+now.minute());

// 		if (actionSelected == ACTION_STOP) {
// 			DEBUG_PRINTLN(F("Check interval"));

// debugDate(now);
// debugDate(intervalStart);
// debugDate(intervalEnd);

// 			DEBUG_PRINTLN((intervalStart.hour()*60+intervalStart.minute())<(now.hour()*60+now.minute()));
// 			DEBUG_PRINTLN((intervalEnd.hour()*60+intervalEnd.minute())<(now.hour()*60+now.minute()));


// 			actionSelected = ACTION_AUTO;

// 		}

		if (actionSelected == ACTION_STOP && operationalSelected == OPERATION_DISABLED && isInsideInterval) {
			DEBUG_PRINTLN("PUMP_START_TIMER");
			stopPump();

			operationalSelected = OPERATION_NORMAL;
			DEBUG_PRINTLN("OPERATION_NORMAL");

			ResponseStatus rs = sendPumpMessageToClient("start", true);

			if (rs.code!=SUCCESS) {
				errorMessage = rs.getResponseDescription();
				currentScreen = SCREEN_ERROR;
			}

			actionSelected = ACTION_AUTO;
			// mainControl = CONTROL_MANUAL;
			currentScreen = SCREEN_STATUS_DEVICE;
		} else if (actionSelected == ACTION_AUTO && operationalSelected == OPERATION_NORMAL && !isInsideInterval){
			DEBUG_PRINTLN("PUMP_STOP_MENU");

			stopPump();
			actionSelected = ACTION_STOP;
			// mainControl = CONTROL_MANUAL;
			operationalSelected = OPERATION_DISABLED;

			ResponseStatus rs = sendPumpMessageToClient("stopp", true);
			if (rs.code!=SUCCESS) {
				errorMessage = rs.getResponseDescription();
				currentScreen = SCREEN_ERROR;
			}else{
				currentScreen = SCREEN_STATUS_DEVICE;
			}
		}
	}


		if (currentScreen==SCREEN_STATUS_DEVICE || currentScreen==SCREEN_STATUS_DEVICE_DETAILED){
			changed = true;
		}

		if (operationalSelected==OPERATION_NORMAL && pumpIsActive && millis()-lastMillisMessageReceived>maxTimeWithoutMessage) {
			DEBUG_PRINTLN("NO MESSAGE FROM CLIENT!");
			DEBUG_PRINTLN(millis());
			DEBUG_PRINTLN(lastMillisMessageReceived);
			DEBUG_PRINTLN(maxTimeWithoutMessage);


			stopPump();
			setError("No message from client!!!");
			clientDisconnected = true;
		}
		if (operationalSelected==OPERATION_PING && pumpIsActive && millis()-lastMillisMessageReceived>maxTimeWithoutPingMessage) {
			DEBUG_PRINTLN("NO MESSAGE FROM CLIENT!");
			stopPump();
			setError("No message from client!!!");
			clientDisconnected = true;
		}

		timePassed = millis();
	}
	handleScreen(currentScreen);

	if (changed){

		pcf8574.detachInterrupt();
		if (changedButton) {
			currentScreen = clickMenu(currentScreen, encoderValue);
			changedButton = false;
		}

		renderScreen(currentScreen);

		display.clearDisplay();

		pcf8574.attachInterrupt();
		changed = false;
		pcf8574.readEncoderValue(ENCODER_PIN_A, ENCODER_PIN_B);
		pcf8574.digitalRead(ENCODER_BUTTON);
//		updateEncoder();
	}
#ifdef ACTIVATE_OTA
	  if (connected) ArduinoOTA.handle();
#endif
}

bool fistTimeRead = true;
void updateEncoder(){
	long encoderValuePrec = encoderValue;

	changed = pcf8574.readEncoderValue(ENCODER_PIN_A, ENCODER_PIN_B, &encoderValue, false);
	encoderButtonVal = pcf8574.digitalRead(ENCODER_BUTTON);

	if (fistTimeRead){
		encoderValue = encoderValuePrec;
		fistTimeRead = false;
	}else{
		if (encoderValue > screenEncoderSizeMax-1) encoderValue = 0;
		if (encoderValue < 0) encoderValue = screenEncoderSizeMax-1;
	}

	if (encoderButtonVal!=encoderButtonValPrec){
  	    changed = true;
		if (encoderButtonVal==LOW && encoderButtonValPrec == HIGH){
		  changedButton = true;
		  encoderValue = encoderValuePrec;
	  }
	  encoderButtonValPrec = encoderButtonVal;
	}
}

SCREEN mainMenuState(MENU_MAIN_STATE state){
	DEBUG_PRINT(F("mainMenuState --> "));
	DEBUG_PRINTLN(state);
	ResponseStatus rs;
	switch (state) {
		case PUMP_START_MENU:
			DEBUG_PRINTLN("PUMP_START_MENU");
			stopPump();

			operationalSelected = OPERATION_NORMAL;
			DEBUG_PRINTLN("OPERATION_NORMAL");

			rs = sendPumpMessageToClient("start", true);

			if (rs.code!=SUCCESS) {
				errorMessage = rs.getResponseDescription();
				return SCREEN_ERROR;
			}

			actionSelected = ACTION_AUTO;
			mainControl = CONTROL_MANUAL;
			return SCREEN_STATUS_DEVICE;
			break;
		case PUMP_TIMER_MENU:
			DEBUG_PRINTLN("PUMP_TIMER_MENU");
			stopPump();

			operationalSelected = OPERATION_DISABLED;
			DEBUG_PRINTLN("OPERATION_DISABLED");

			actionSelected = ACTION_STOP;
			mainControl = CONTROL_TIMER;
			return SCREEN_STATUS_DEVICE;
			break;
		case PUMP_START_PING_MENU:
			DEBUG_PRINTLN("PUMP_START_PING_MENU");

			stopPump();

			operationalSelected = OPERATION_PING;
			DEBUG_PRINTLN("OPERATION_PING");

			rs = sendPumpMessageToClient("start", true);

			if (rs.code!=SUCCESS) {
				errorMessage = rs.getResponseDescription();
				return SCREEN_ERROR;
			}
			actionSelected = ACTION_AUTO;
			mainControl = CONTROL_MANUAL;
			return SCREEN_STATUS_DEVICE;
			break;
		case PUMP_STOP_MENU:
			DEBUG_PRINTLN("PUMP_STOP_MENU");

			stopPump();
			actionSelected = ACTION_STOP;
			mainControl = CONTROL_NONE;
			operationalSelected = OPERATION_DISABLED;

			rs = sendPumpMessageToClient("stopp", true);
			if (rs.code!=SUCCESS) {
				errorMessage = rs.getResponseDescription();
				return SCREEN_ERROR;
			}else{
				return SCREEN_STATUS_DEVICE;
			}

			break;
		case STATUS_MENU:
			return SCREEN_STATUS_DEVICE;
			break;
		case CONFIG_TIMER_MENU:
			return SCREEN_CONFIG_TIMER;
			break;
		case STATUS_MENU_DETAILED:
			return SCREEN_STATUS_DEVICE_DETAILED;
			break;
		default:
			break;
	}
	return SCREEN_ERROR;
};

ResponseStatus setWakeUpMode(){
	ResponseStatus rs;
	rs.code =	e32ttl.setMode(MODE_1_WAKE_UP);
	return rs;
}

ResponseStatus setNormalMode(){
	ResponseStatus rs;
	rs.code =	e32ttl.setMode(MODE_0_NORMAL);
	return rs;
}

DynamicJsonDocument doc(512);

ResponseStatus setReceiveMode(){
	ResponseStatus rs;
	rs.code =	e32ttl.setMode(MODE_0_NORMAL);
	return rs;
}

ResponseStatus sendPumpMessageToClient(String action, bool wakeUp){
	ResponseStatus rsW;
	if (wakeUp){
		rsW = setWakeUpMode();
	}else{
		rsW = setNormalMode();
	}
	DEBUG_PRINTLN(rsW.getResponseDescription());
	if (rsW.code != SUCCESS) return rsW;

	JsonObject root = doc.to<JsonObject>();

	root["type"] = action;
	root["mode"] = (int)operationalSelected;

	int size = measureJson(doc)+1;

	char buf[size];
	serializeJson(doc, buf, size);
	DEBUG_PRINTLN(buf);
	DEBUG_PRINTLN(measureJson(doc));

	DEBUG_PRINT("Send message to client ");
	DEBUG_PRINT(CLIENT_ADDH, DEC);
	DEBUG_PRINT(" ");
	DEBUG_PRINT(CLIENT_ADDL, DEC);
	DEBUG_PRINT(" ");
	DEBUG_PRINT(CLIENT_CHANNEL, HEX);
	DEBUG_PRINTLN(" ");

	DEBUG_PRINTLN("Mode --> ");
	DEBUG_PRINTLN(e32ttl.getMode());

	rsW = e32ttl.sendFixedMessage(CLIENT_ADDH, CLIENT_ADDL, CLIENT_CHANNEL, buf, size);
	DEBUG_PRINTLN(rsW.getResponseDescription());
	if (rsW.code != SUCCESS) return rsW;

	rsW = setReceiveMode();

	return rsW;
}

SCREEN checkMenuState(){
	return SCREEN_MAIN;
};
SCREEN checkConfigTimerState(int state) {
	if (state == 0) {
		encoderValue = startTimeGlob;
		return SCREEN_CONFIG_TIMER_DATE_START;
	} else if (state == 1) {
		encoderValue = endTimeGlob;
		return SCREEN_CONFIG_TIMER_DATE_END;
	}
	encoderValue = 0;
	return SCREEN_MAIN;
}

SCREEN clickMenu(SCREEN screen, int state){
	SCREEN returningScreen = SCREEN_ERROR;
	switch (screen) {
		case SCREEN_MAIN:
			returningScreen = mainMenuState((MENU_MAIN_STATE)state);
			encoderValue = 0;
			break;
		case SCREEN_STATUS_DEVICE:
		case SCREEN_STATUS_DEVICE_DETAILED:
		case SCREEN_ERROR:
			returningScreen = checkMenuState();
			encoderValue = 0;
			break;
		case SCREEN_CONFIG_TIMER:
			encoderValue = 0;
			returningScreen = checkConfigTimerState(state);
			break;
		case SCREEN_CONFIG_TIMER_DATE_START:
			startTimeGlob = encoderValue;
			eeWriteInt(EE_TIME_START, startTimeGlob);

			encoderValue = 0;

			returningScreen = SCREEN_CONFIG_TIMER;
			break;
		case SCREEN_CONFIG_TIMER_DATE_END:
			endTimeGlob = encoderValue;
			eeWriteInt(EE_TIME_END, endTimeGlob);

			encoderValue = 0;

			returningScreen = SCREEN_CONFIG_TIMER;
			break;
		default:
			encoderValue = 0;
			break;
	}

	screenEncoderSizeMax = screenEncoderSize[returningScreen];

	DEBUG_PRINT(F("CLICK --> "));

	DEBUG_PRINT( F("FROM SCREEN --> ") );

	DEBUG_PRINT( screen );
	DEBUG_PRINT(F(" - TO SCREEN --> "));
	DEBUG_PRINT( returningScreen );
	DEBUG_PRINT(F(" - MENU STATE --> "));
	DEBUG_PRINTLN( state );

	return returningScreen;
}


void displayField(String label, bool selected, int top, int left = 2){
	  // display.setTextSize(1);
	  display.setCursor(left, top);
	  if (selected){
		  display.setTextColor(BLACK, WHITE);
	  }else{
		  display.setTextColor(WHITE, BLACK);
	  }
	  display.println(label);
}

void displayTitle(String label,  int top = 0, int left = 2){
	  display.setTextSize(2);
	  display.setTextColor(WHITE);

	  display.setCursor(left, top);
	  display.println(label);
}

void displayIconLabel(String label, int rows){
	  display.setTextSize(2);

	  int position = 15;
	  if (rows==1) position = 25;
	  if (rows==2) position = 15;
	  if (rows==3) position = 5;

	  display.setCursor(0, position);

	  display.setTextColor(WHITE, BLACK);

	  display.println(label);

}

void mainMenu(String title = SCREEN_MAIN_TITLE) {
	  display.clearDisplay(); //for Clearing the display
	  switch (encoderValue) {
		case PUMP_START_MENU:
			displayIconLabel(START_PUMP_LABEL, 2);
			display.drawBitmap(70, 5, tapDrop, 54, 54, WHITE); // display.drawBitmap(x position, y position, bitmap data, bitmap width, bitmap height, color)
			break;
		case PUMP_TIMER_MENU:
			displayIconLabel(PUMP_TIMER_MENU_LABEL, 2);
			display.drawBitmap(70, 5, timerIcon, 54, 54, WHITE); // display.drawBitmap(x position, y position, bitmap data, bitmap width, bitmap height, color)
			break;
		case PUMP_STOP_MENU:
			displayIconLabel(STOP_PUMP_MENU_LABEL, 2);
			display.drawBitmap(70, 5, dropSlash, 54, 54, WHITE); // display.drawBitmap(x position, y position, bitmap data, bitmap width, bitmap height, color)
			break;
		case PUMP_START_PING_MENU:
			displayIconLabel(START_PUMP_PING_LABEL, 3);
			display.drawBitmap(70, 5, containerDrop, 54, 54, WHITE); // display.drawBitmap(x position, y position, bitmap data, bitmap width, bitmap height, color)
			break;
		case STATUS_MENU:
			displayIconLabel(STATUS_DEVICE_MENU_LABEL, 1);
			display.drawBitmap(70, 5, info, 54, 54, WHITE); // display.drawBitmap(x position, y position, bitmap data, bitmap width, bitmap height, color)

			break;
		case CONFIG_TIMER_MENU:
			displayIconLabel(CONFIG_TIMER_MENU_LABEL, 2);
			display.drawBitmap(70, 5, configTimerIcon, 54, 54, WHITE); // display.drawBitmap(x position, y position, bitmap data, bitmap width, bitmap height, color)
			break;

		case STATUS_MENU_DETAILED:
			displayIconLabel(STATUS_DETT_MENU_LABEL, 1);
			display.drawBitmap(70, 5, info, 54, 54, WHITE); // display.drawBitmap(x position, y position, bitmap data, bitmap width, bitmap height, color)

			break;
		default:
			break;
	}

	  display.display();
}

uint8_t iconsNum = 0;
unsigned long startTime = millis();

void checkPageDetailed(String title = SCREEN_STATUS_DEVICE_TITLE) {
	  displayTitle(title);

	  display.setTextSize(1);
	  uint8_t top = 2;
	  display.setCursor(2, 10*top++ -3);

	  display.setTextColor(WHITE, BLACK);

	  if (lastMillisMessageReceived==0){
		  display.println("Await update message");
	  }else{
		  display.print("LM: ");
		  display.print((unsigned long)((millis()-lastMillisMessageReceived)/(1000)));
		  display.print("sec");
		  if (batteryLevel>1){
			  display.print(" Bat: ");
			  display.print(batteryLevel);
		  }
		  display.println();

		  display.setCursor(2, 10*top++ -3);
		  display.print("Pump: ");
		  display.println((pumpIsActive?"active":"stopped"));

		  display.setCursor(2, 10*top++ -3);
		  display.print("Max: ");
		  display.println((maxLevel?"raised":"not raised"));

		  display.setCursor(2, 10*top++ -3);
		  display.print("Min: ");
		  display.println((minLevel?"raised":"not raised"));
	  }
      display.setTextSize(1);

	  displayField(BACK_LABEL, true, 10*top++ -3);
      display.display();
}

void configTimerPage(String title = SCREEN_CONFIG_TIMER_TITLE, int encoderValueInt = 0, int startTime = 11, int endTime = 24) {
  displayTitle(title);
  
  display.setTextSize(1);

  int top = 3;

	int startHours = startTime / 2;
	int startMinutes = (startTime % 2) * 30;

	int endHours = endTime / 2;
	int endMinutes = (endTime % 2) * 30;
	
	String startTimeStr =  ((startHours<10)?"0"+String(startHours):String(startHours)) + ":" + ((startMinutes<10)?"0"+String(startMinutes):String(startMinutes));
	String endTimeStr =  ((endHours<10)?"0"+String(endHours):String(endHours)) + ":" + ((endMinutes<10)?"0"+String(endMinutes):String(endMinutes));
	
	DateTime now = rtc.now();

	displayField(getFormattedDateTime(now.year(), now.month(), now.day(), now.dayOfTheWeek(), now.hour(), now.minute(), now.second()), false, 17);

  displayField(String(TIMER_START_LABEL)+" "+startTimeStr, encoderValueInt==0, 10*top++ );
  displayField(String(TIMER_STOP_LABEL)+" "+endTimeStr, encoderValueInt==1, 10*top++ );
  display.println();
  displayField(BACK_LABEL, encoderValueInt==2, 10*top++ +3);

  display.display();

}

void checkPage(String title = SCREEN_STATUS_DEVICE_TITLE) {
	display.setTextSize(1);
	display.setTextColor(WHITE, BLACK);
	display.setCursor(2, 2);

	if (pumpIsActive){
		display.print(STATUS_REFILL);
	}else if(lastMillisMessageReceived==0){
		display.print(STATUS_WAIT_STATUS);
	}else if(maxLevel){
		display.print(STATUS_FULL);
	}else if(minLevel){
		display.print(STATUS_EMPTY);
	}

	uint8_t battery_position = 100;

	unsigned long lastTimeMessage = ((millis()-lastMillisMessageReceived)/(1000));
	if (lastMillisMessageReceived!=0 && lastTimeMessage>0 && lastTimeMessage<15) {
		display.drawBitmap(battery_position-22, 1, message, 17, 12, WHITE); // display.drawBitmap(x position, y position, bitmap data, bitmap width, bitmap height, color)
	}

	if (batteryLevel>1){
		display.drawBitmap(battery_position, 1, battery, 20, 12, WHITE); // display.drawBitmap(x position, y position, bitmap data, bitmap width, bitmap height, color)

//		19,10
//		118

		float maxInt = 4.3-2.8;
		float currInt = batteryLevel-2.8;
		if (currInt<0) currInt = 2;

		short int levVal = currInt*11/maxInt;

		display.writeFillRect(battery_position+4, 4, levVal, 6, WHITE);
	  }else{
		display.drawBitmap(battery_position, 1, batteryNo, 20, 12, WHITE); // display.drawBitmap(x position, y position, bitmap data, bitmap width, bitmap height, color)
	  }

	if (startTime+500<millis()) {
		if (iconsNum==0) {
			iconsNum = 1;
		}else{
			iconsNum = 0;
		}
		startTime = millis();
	}

	int pumpIconY = 22;
	if (mainControl == CONTROL_TIMER) {
		pumpIconY = 14;
	}

	if (pumpIsActive){
		display.drawBitmap(12, pumpIconY, pumpOpen[iconsNum], 33, 42, WHITE); // display.drawBitmap(x position, y position, bitmap data, bitmap width, bitmap height, color)
	}else{
		display.drawBitmap(12, pumpIconY, pumpClosed[iconsNum], 33, 42, WHITE); // display.drawBitmap(x position, y position, bitmap data, bitmap width, bitmap height, color)
	}

	if (minLevel){
		display.drawBitmap(72, 22, waterLevelEmpty[iconsNum], 42, 42, WHITE); // display.drawBitmap(x position, y position, bitmap data, bitmap width, bitmap height, color)
	}else if (!maxLevel) {
		display.drawBitmap(72, 22, waterLevelMiddle[iconsNum], 42, 42, WHITE); // display.drawBitmap(x position, y position, bitmap data, bitmap width, bitmap height, color)
	}else{
		display.drawBitmap(72, 22, waterLevelFull[iconsNum], 42, 42, WHITE); // display.drawBitmap(x position, y position, bitmap data, bitmap width, bitmap height, color)
	}

	DateTime now = rtc.now();

	if (mainControl == CONTROL_TIMER) {
		display.setCursor(0, 48);
	} else {
		display.setCursor(0, SCREEN_HEIGHT - 7 );
	}
	// display.println(getFormattedDate(now.year(), now.month(), now.day(), now.dayOfTheWeek()));
	// display.println(getFormattedTime(now.hour(), now.minute(), now.second()));
	if (now.hour()<10) {
		display.print("0");
	}
	display.print(now.hour());
	display.print(":");
	if (now.minute()<10) {
		display.print("0");
	}
	display.print(now.minute());
	// display.println(now.second());

	if (mainControl == CONTROL_TIMER) {
		int marginLeft = (64-48)/2;
		display.fillRect(marginLeft, SCREEN_HEIGHT-4, 48, 4, WHITE);

		display.drawLine(marginLeft+startTimeGlob, SCREEN_HEIGHT-3, (64-48)/2+endTimeGlob, SCREEN_HEIGHT-3, BLACK);
		display.drawLine(marginLeft+startTimeGlob, SCREEN_HEIGHT-2, (64-48)/2+endTimeGlob, SCREEN_HEIGHT-2, BLACK);

		int positionNow = marginLeft + (now.hour()*2)+(now.minute()>30?1:0);

		display.fillTriangle( 
			positionNow, 	SCREEN_HEIGHT-4, 
			positionNow-1, 	SCREEN_HEIGHT-7, 
			positionNow+1, 	SCREEN_HEIGHT-7, 
			(iconsNum==0)?BLACK:WHITE);
		display.drawLine(	positionNow, SCREEN_HEIGHT-5, positionNow, SCREEN_HEIGHT, (iconsNum==0)?BLACK:WHITE);

		// DEBUG_PRINTLN(((64-48)/2) + ((now.hour()*2)+(now.minute()%30)));
		// DEBUG_PRINTLN((now.hour()*2)+(now.minute()>30?1:0));
	}


	display.drawBitmap(SCREEN_WIDTH-7, SCREEN_HEIGHT-7, controlIcons[mainControl], 7, 7, WHITE);

	display.display();
}
void errorPage(String title = SCREEN_ERROR_TITLE) {
  displayTitle(title);

  display.setTextSize(1);
  display.setCursor(2, 20);

  display.setTextColor(WHITE, BLACK);

  display.println(errorMessage);

  displayField(BACK_LABEL, true, 10*4);

  display.display();
}

void renderScreen(SCREEN screen){
	switch (screen) {
		case SCREEN_MAIN:
			mainMenu();
			break;
		case SCREEN_STATUS_DEVICE:
			checkPage();
			break;
		case SCREEN_STATUS_DEVICE_DETAILED:
			checkPageDetailed();
			break;
		case SCREEN_CONFIG_TIMER:
			configTimerPage(SCREEN_CONFIG_TIMER_TITLE, encoderValue, startTimeGlob, endTimeGlob);
			break;
		case SCREEN_CONFIG_TIMER_DATE_START:
			configTimerPage(SCREEN_CONFIG_TIMER_TITLE, 0, encoderValue, endTimeGlob);
			break;
		case SCREEN_CONFIG_TIMER_DATE_END:
			configTimerPage(SCREEN_CONFIG_TIMER_TITLE, 1, startTimeGlob, encoderValue);
			break;

		case SCREEN_ERROR:
			errorPage();
			break;

		default:
			break;
	}
}

void managePump(){
	if (lastPacketClient>0){
		switch (operationalSelected) {
			case OPERATION_DISABLED:
				if (pumpIsActive) {
					stopPump();
					DEBUG_PRINTLN("OP DIS MAX LEVEL RAISED --> STOP PUMP!");

					maxLevel = false;
					minLevel = false;
				}
				break;
			case OPERATION_NORMAL:
			case OPERATION_PING:
				if (pumpIsActive && maxLevel) {
					DEBUG_PRINTLN("MAX LEVEL RAISED --> STOP PUMP!");
					DEBUG_PRINT("PUMP ACTIVE --> ");
					DEBUG_PRINTLN(pumpIsActive);
					DEBUG_PRINT("MAX LEVEL --> ");
					DEBUG_PRINTLN(maxLevel);

					stopPump();

					ResponseStatus rs;
					rs = sendPumpMessageToClient("stopp", true);
				}else if (!pumpIsActive && !clientDisconnected && !maxLevel) {
					DEBUG_PRINTLN("MAX LEVEL NOT RAISED --> START PUMP!");

					DEBUG_PRINT("PUMP ACTIVE --> ");
					DEBUG_PRINTLN(pumpIsActive);
					DEBUG_PRINT("CLIENT DISCONNECTED --> ");
					DEBUG_PRINTLN(clientDisconnected);
					DEBUG_PRINT("MAX LEVEL --> ");
					DEBUG_PRINTLN(maxLevel);

					startPump();
				}
				break;
			default:
				break;
		}
	}
}

void waitMessageUpdateFromClient(){
	if (e32ttl.available()>0){
		DEBUG_PRINTLN("Message status received");

		ResponseContainer rs = e32ttl.receiveMessageUntil('\0');
		String message = rs.data;

		DEBUG_PRINTLN(rs.status.getResponseDescription());

		DEBUG_PRINTLN(message);
		deserializeJson(doc, message);

		lastMillisMessageReceived = millis();

		String type = doc["ty"];
		DEBUG_PRINT("type --> ");
		DEBUG_PRINTLN(type);

		if (type=="bl"){
			lastPacketClient = doc["pn"];

			batteryLevel = doc["battLev"];

			DEBUG_PRINT("BATTERY LEVEL ");
			DEBUG_PRINTLN(batteryLevel);

		}else  if (type=="ppl"){
			lastPacketClient = doc["pn"];
			maxLevel = doc["maxL"];
			minLevel = doc["minL"];

			DEBUG_PRINT("MAX MIN ");
			DEBUG_PRINT(maxLevel);
			DEBUG_PRINTLN(minLevel);

			clientDisconnected = false;

			bool needAck = ((uint8_t)doc["ack"])==1;
			DEBUG_PRINT("Need ack ");
			DEBUG_PRINTLN(needAck);
			if (needAck){
				sendPumpMessageToClient("ackpa");
			}
		}
	}

}

bool errorSet = false;

void handleScreen(SCREEN screen){
	switch (screen) {
		case SCREEN_MAIN:
		case SCREEN_STATUS_DEVICE:
			if (errorSet) digitalWrite(ERROR_LED, HIGH);
			errorSet = false;
			break;
		case SCREEN_STATUS_DEVICE_DETAILED:
			if (errorSet) digitalWrite(ERROR_LED, HIGH);
			errorSet = false;
			break;
		case SCREEN_ERROR:
			if (!errorSet) digitalWrite(ERROR_LED, LOW);
			errorSet = true;
			break;
		default:
			break;
	}
	waitMessageUpdateFromClient();
	managePump();
}


void printParameters(struct Configuration configuration) {
	DEBUG_PRINTLN("----------------------------------------");

	DEBUG_PRINT(F("HEAD BIN: "));  DEBUG_PRINT(configuration.HEAD, BIN);DEBUG_PRINT(" ");DEBUG_PRINT(configuration.HEAD, DEC);DEBUG_PRINT(" ");DEBUG_PRINTLN(configuration.HEAD, HEX);
	DEBUG_PRINTLN(F(" "));
	DEBUG_PRINT(F("AddH BIN: "));  DEBUG_PRINTLN(configuration.ADDH, DEC);
	DEBUG_PRINT(F("AddL BIN: "));  DEBUG_PRINTLN(configuration.ADDL, DEC);
	DEBUG_PRINT(F("Chan BIN: "));  DEBUG_PRINT(configuration.CHAN, DEC); DEBUG_PRINT(" -> "); DEBUG_PRINTLN(configuration.getChannelDescription());
	DEBUG_PRINTLN(F(" "));
	DEBUG_PRINT(F("SpeedParityBit BIN    : "));  DEBUG_PRINT(configuration.SPED.uartParity, BIN);DEBUG_PRINT(" -> "); DEBUG_PRINTLN(configuration.SPED.getUARTParityDescription());
	DEBUG_PRINT(F("SpeedUARTDataRate BIN : "));  DEBUG_PRINT(configuration.SPED.uartBaudRate, BIN);DEBUG_PRINT(" -> "); DEBUG_PRINTLN(configuration.SPED.getUARTBaudRate());
	DEBUG_PRINT(F("SpeedAirDataRate BIN  : "));  DEBUG_PRINT(configuration.SPED.airDataRate, BIN);DEBUG_PRINT(" -> "); DEBUG_PRINTLN(configuration.SPED.getAirDataRate());

	DEBUG_PRINT(F("OptionTrans BIN       : "));  DEBUG_PRINT(configuration.OPTION.fixedTransmission, BIN);DEBUG_PRINT(" -> "); DEBUG_PRINTLN(configuration.OPTION.getFixedTransmissionDescription());
	DEBUG_PRINT(F("OptionPullup BIN      : "));  DEBUG_PRINT(configuration.OPTION.ioDriveMode, BIN);DEBUG_PRINT(" -> "); DEBUG_PRINTLN(configuration.OPTION.getIODroveModeDescription());
	DEBUG_PRINT(F("OptionWakeup BIN      : "));  DEBUG_PRINT(configuration.OPTION.wirelessWakeupTime, BIN);DEBUG_PRINT(" -> "); DEBUG_PRINTLN(configuration.OPTION.getWirelessWakeUPTimeDescription());
	DEBUG_PRINT(F("OptionFEC BIN         : "));  DEBUG_PRINT(configuration.OPTION.fec, BIN);DEBUG_PRINT(" -> "); DEBUG_PRINTLN(configuration.OPTION.getFECDescription());
	DEBUG_PRINT(F("OptionPower BIN       : "));  DEBUG_PRINT(configuration.OPTION.transmissionPower, BIN);DEBUG_PRINT(" -> "); DEBUG_PRINTLN(configuration.OPTION.getTransmissionPowerDescription());

	DEBUG_PRINTLN("----------------------------------------");

}

void startPump(){
	DEBUG_PRINTER.println("START PUMP");
	pumpIsActive = true;
	digitalWrite(RELAY_PIN, HIGH);

	pumpStartTime = millis();
	lastMillisMessageReceived = millis();
}

void stopPump(){
	DEBUG_PRINTER.println("STOP PUMP");
	pumpIsActive = false;
	digitalWrite(RELAY_PIN, LOW);

	pumpStopTime = millis();
}
