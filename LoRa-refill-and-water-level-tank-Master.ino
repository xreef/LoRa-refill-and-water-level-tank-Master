#include "Arduino.h"

#include <Wire.h>
#include "include/splash.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "PCF8574.h"
#include "LoRa_E32.h"
#include <ArduinoJson.h>

#include "include/states.h"
#include "include/eng.h"
//#include "include/ita.h"
#include "include/icons.h"

#include <stdlib.h>

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
#endif

#define SERIAL_DEBUG Serial1

#define START_ON_BOOT

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

OPERATION_MODE operationalSelected = OPERATION_NORMAL;
ACION_MODE actionSelected = ACTION_AUTO;

bool changed = false;
bool changedButton = false;

void setup()
{
delay(4000);
	SERIAL_DEBUG.begin(115200);

	  while (!SERIAL_DEBUG) {
	    ; // wait for serial port to connect. Needed for native USB
	  }		// encoder pins

#ifdef ACTIVATE_OTA
	const char* ssid = "YOUR-SSID";
	const char* password = "YOUR-PASSWD";

	delay(100);
	SERIAL_DEBUG.flush();

	SERIAL_DEBUG.println("Booting");
	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, password);
//	while (WiFi.waitForConnectResult(20000) != WL_CONNECTED) {
//		SERIAL_DEBUG.println("Connection Failed!");
////	  delay(5000);
////	  ESP.restart();
//	}

	delay(4000);

	if (WiFi.status() == WL_CONNECTED) {
//	delay(1000);
	SERIAL_DEBUG.println("Ready");
	SERIAL_DEBUG.print("IP address: ");
	SERIAL_DEBUG.println(WiFi.localIP());

	ArduinoOTA.onStart([]() {
		String type;
		if (ArduinoOTA.getCommand() == U_FLASH) {
		  type = "sketch";
		} else { // U_FS
		  type = "filesystem";
		}

		// NOTE: if updating FS this would be the place to unmount FS using FS.end()
		SERIAL_DEBUG.println("Start updating " + type);
	  });
	  ArduinoOTA.onEnd([]() {
		SERIAL_DEBUG.println("\nEnd");
	  });
	  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
		SERIAL_DEBUG.printf("Progress: %u%%\r\n", (progress / (total / 100)));
	  });
	  ArduinoOTA.onError([](ota_error_t error) {
		SERIAL_DEBUG.printf("Error[%u]: ", error);
		if (error == OTA_AUTH_ERROR) {
		  SERIAL_DEBUG.println("Auth Failed");
		} else if (error == OTA_BEGIN_ERROR) {
		  SERIAL_DEBUG.println("Begin Failed");
		} else if (error == OTA_CONNECT_ERROR) {
		  SERIAL_DEBUG.println("Connect Failed");
		} else if (error == OTA_RECEIVE_ERROR) {
		  SERIAL_DEBUG.println("Receive Failed");
		} else if (error == OTA_END_ERROR) {
		  SERIAL_DEBUG.println("End Failed");
		}
	  });
	  ArduinoOTA.begin();
	}
#endif

	  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
	  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
//		  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
	    SERIAL_DEBUG.println(F("SSD1306 allocation failed"));
	    for(;;); // Don't proceed, loop forever
	  }

	  // Show initial display buffer contents on the screen --
	  // the library initializes this with an Adafruit splash screen.
	  display.clearDisplay();
	  display.drawBitmap((display.width() - splash1_width) / 2, (display.height() - splash1_height) / 2, splash1_data, splash1_width, splash1_height, 1);
	  display.display();

	  SERIAL_DEBUG.println();
	  SERIAL_DEBUG.println("--------------------------------------------");

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
		SERIAL_DEBUG.println(rs.getResponseDescription());

		c = e32ttl.getConfiguration();
		// It's important get configuration pointer before all other operation
		Configuration configurationNew = *(Configuration*) c.data;
		SERIAL_DEBUG.println(c.status.getResponseDescription());
		SERIAL_DEBUG.println(c.status.code);

		printParameters(configurationNew);


		  delay(2000); // Pause for 2 seconds

		  // Clear the buffer
		  display.clearDisplay();

		  renderScreen(SCREEN_MAIN);

#ifdef START_ON_BOOT
		  changed = true;
		  changedButton = true;

		  currentScreen = SCREEN_MAIN;
		  encoderValue = PUMP_START_MENU;
#endif
}

//bool changed = false;
//bool changedButton = false;
//
unsigned long timePassed = millis();
unsigned long interval = 1000;
unsigned long maxTimeWithoutPingMessage = 15000;
unsigned long maxTimeWithoutMessage = 1000*60*60;

bool pumpIsActive = false;

bool maxLevel = false;
bool minLevel = false;

float batteryLevel = 0;

void setError(const String errorMessagePar){
	errorMessage = errorMessagePar;
	currentScreen = SCREEN_ERROR;
}

bool clientDisconnected = true;

// The loop function is called in an endless loop
void loop()
{
	if (timePassed+interval<millis()){
		if (currentScreen==SCREEN_STATUS_DEVICE || currentScreen==SCREEN_STATUS_DEVICE_DETAILED){
			changed = true;
		}

		if (operationalSelected==OPERATION_NORMAL && pumpIsActive && millis()-lastMillisMessageReceived>maxTimeWithoutMessage) {
			SERIAL_DEBUG.println("NO MESSAGE FROM CLIENT!");
			SERIAL_DEBUG.println(millis());
			SERIAL_DEBUG.println(lastMillisMessageReceived);
			SERIAL_DEBUG.println(maxTimeWithoutMessage);


			stopPump();
			setError("No message from client!!!");
			clientDisconnected = true;
		}
		if (operationalSelected==OPERATION_PING && pumpIsActive && millis()-lastMillisMessageReceived>maxTimeWithoutPingMessage) {
			SERIAL_DEBUG.println("NO MESSAGE FROM CLIENT!");
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
	  ArduinoOTA.handle();
#endif
}

bool fistTimeRead = true;
void updateEncoder(){
	long encoderValuePrec = encoderValue;

	changed = pcf8574.readEncoderValue(ENCODER_PIN_A, ENCODER_PIN_B, &encoderValue);
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
	SERIAL_DEBUG.print(F("mainMenuState --> "));
	SERIAL_DEBUG.println(state);
	ResponseStatus rs;
	switch (state) {
		case PUMP_START_MENU:
			DEBUG_PRINTLN("PUMP_START_MENU");
			stopPump();

			operationalSelected = OPERATION_NORMAL;
			SERIAL_DEBUG.println("OPERATION_NORMAL");

			rs = sendPumpMessageToClient("start", true);

			if (rs.code!=SUCCESS) {
				errorMessage = rs.getResponseDescription();
				return SCREEN_ERROR;
			}

			actionSelected = ACTION_AUTO;
			return SCREEN_STATUS_DEVICE;
			break;
		case PUMP_START_PING_MENU:
			DEBUG_PRINTLN("PUMP_START_PING_MENU");

			stopPump();

			operationalSelected = OPERATION_PING;
			SERIAL_DEBUG.println("OPERATION_PING");

			rs = sendPumpMessageToClient("start", true);

			if (rs.code!=SUCCESS) {
				errorMessage = rs.getResponseDescription();
				return SCREEN_ERROR;
			}
			actionSelected = ACTION_AUTO;
			return SCREEN_STATUS_DEVICE;
			break;
		case PUMP_STOP_MENU:
			DEBUG_PRINTLN("PUMP_STOP_MENU");

			stopPump();
			actionSelected = ACTION_STOP;
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
	SERIAL_DEBUG.println(rsW.getResponseDescription());
	if (rsW.code != SUCCESS) return rsW;

	JsonObject root = doc.to<JsonObject>();

	root["type"] = action;
	root["mode"] = (int)operationalSelected;

	int size = measureJson(doc)+1;

	char buf[size];
	serializeJson(doc, buf, size);
	SERIAL_DEBUG.println(buf);
	SERIAL_DEBUG.println(measureJson(doc));

	SERIAL_DEBUG.print("Send message to client ");
	SERIAL_DEBUG.print(CLIENT_ADDH, DEC);
	SERIAL_DEBUG.print(" ");
	SERIAL_DEBUG.print(CLIENT_ADDL, DEC);
	SERIAL_DEBUG.print(" ");
	SERIAL_DEBUG.print(CLIENT_CHANNEL, HEX);
	SERIAL_DEBUG.println(" ");

	SERIAL_DEBUG.println("Mode --> ");
	SERIAL_DEBUG.println(e32ttl.getMode());

	rsW = e32ttl.sendFixedMessage(CLIENT_ADDH, CLIENT_ADDL, CLIENT_CHANNEL, buf, size);
	SERIAL_DEBUG.println(rsW.getResponseDescription());
	if (rsW.code != SUCCESS) return rsW;

	rsW = setReceiveMode();

	return rsW;
}


SCREEN checkMenuState(){
	return SCREEN_MAIN;
};

SCREEN clickMenu(SCREEN screen, int state){
	SCREEN returningScreen = SCREEN_ERROR;
	switch (screen) {
		case SCREEN_MAIN:
			returningScreen = mainMenuState((MENU_MAIN_STATE)state);
			break;
		case SCREEN_STATUS_DEVICE:
		case SCREEN_STATUS_DEVICE_DETAILED:
		case SCREEN_ERROR:
			returningScreen = checkMenuState();
			break;
		default:
			break;
	}

	encoderValue = 0;
	screenEncoderSizeMax = screenEncoderSize[returningScreen];

	SERIAL_DEBUG.print(F("CLICK --> "));

	SERIAL_DEBUG.print( F("FROM SCREEN --> ") );

	SERIAL_DEBUG.print( screen );
	SERIAL_DEBUG.print(F(" - TO SCREEN --> "));
	SERIAL_DEBUG.print( returningScreen );
	SERIAL_DEBUG.print(F(" - MENU STATE --> "));
	SERIAL_DEBUG.println( state );

	return returningScreen;
}


void displayField(String label, bool selected, int top, int left = 2){
	  display.setTextSize(1);
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
		case PUMP_START_PING_MENU:
			displayIconLabel(START_PUMP_PING_LABEL, 3);
			display.drawBitmap(70, 5, containerDrop, 54, 54, WHITE); // display.drawBitmap(x position, y position, bitmap data, bitmap width, bitmap height, color)
			break;
		case PUMP_STOP_MENU:
			displayIconLabel(STOP_PUMP_MENU_LABEL, 2);
			display.drawBitmap(70, 5, dropSlash, 54, 54, WHITE); // display.drawBitmap(x position, y position, bitmap data, bitmap width, bitmap height, color)

			break;
		case STATUS_MENU:
			displayIconLabel(STATUS_DEVICE_MENU_LABEL, 1);
			display.drawBitmap(70, 5, info, 54, 54, WHITE); // display.drawBitmap(x position, y position, bitmap data, bitmap width, bitmap height, color)

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
	  displayField(BACK_LABEL, true, 10*top++ -3);
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

	if (pumpIsActive){
		display.drawBitmap(12, 22, pumpOpen[iconsNum], 33, 42, WHITE); // display.drawBitmap(x position, y position, bitmap data, bitmap width, bitmap height, color)
	}else{
		display.drawBitmap(12, 22, pumpClosed[iconsNum], 33, 42, WHITE); // display.drawBitmap(x position, y position, bitmap data, bitmap width, bitmap height, color)
	}

	if (minLevel){
		display.drawBitmap(72, 22, waterLevelEmpty[iconsNum], 42, 42, WHITE); // display.drawBitmap(x position, y position, bitmap data, bitmap width, bitmap height, color)
	}else if (!maxLevel) {
		display.drawBitmap(72, 22, waterLevelMiddle[iconsNum], 42, 42, WHITE); // display.drawBitmap(x position, y position, bitmap data, bitmap width, bitmap height, color)
	}else{
		display.drawBitmap(72, 22, waterLevelFull[iconsNum], 42, 42, WHITE); // display.drawBitmap(x position, y position, bitmap data, bitmap width, bitmap height, color)
	}
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
					SERIAL_DEBUG.println("OP DIS MAX LEVEL RAISED --> STOP PUMP!");

					maxLevel = false;
					minLevel = false;
				}
				break;
			case OPERATION_NORMAL:
			case OPERATION_PING:
				if (pumpIsActive && maxLevel) {
					SERIAL_DEBUG.println("MAX LEVEL RAISED --> STOP PUMP!");
					SERIAL_DEBUG.print("PUMP ACTIVE --> ");
					SERIAL_DEBUG.println(pumpIsActive);
					SERIAL_DEBUG.print("MAX LEVEL --> ");
					SERIAL_DEBUG.println(maxLevel);

					stopPump();

					ResponseStatus rs;
					rs = sendPumpMessageToClient("stopp", true);
				}else if (!pumpIsActive && !clientDisconnected && !maxLevel) {
					SERIAL_DEBUG.println("MAX LEVEL NOT RAISED --> START PUMP!");

					SERIAL_DEBUG.print("PUMP ACTIVE --> ");
					SERIAL_DEBUG.println(pumpIsActive);
					SERIAL_DEBUG.print("CLIENT DISCONNECTED --> ");
					SERIAL_DEBUG.println(clientDisconnected);
					SERIAL_DEBUG.print("MAX LEVEL --> ");
					SERIAL_DEBUG.println(maxLevel);

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
		SERIAL_DEBUG.println("Message status received");

		ResponseContainer rs = e32ttl.receiveMessageUntil('\0');
		String message = rs.data;

		SERIAL_DEBUG.println(rs.status.getResponseDescription());

		SERIAL_DEBUG.println(message);
		deserializeJson(doc, message);

		lastMillisMessageReceived = millis();

		String type = doc["ty"];
		SERIAL_DEBUG.print("type --> ");
		SERIAL_DEBUG.println(type);

		if (type=="bl"){
			lastPacketClient = doc["pn"];

			batteryLevel = doc["battLev"];

			SERIAL_DEBUG.print("BATTERY LEVEL ");
			SERIAL_DEBUG.println(batteryLevel);

		}else  if (type=="ppl"){
			lastPacketClient = doc["pn"];
			maxLevel = doc["maxL"];
			minLevel = doc["minL"];

			SERIAL_DEBUG.print("MAX MIN ");
			SERIAL_DEBUG.print(maxLevel);
			SERIAL_DEBUG.println(minLevel);

			clientDisconnected = false;

			bool needAck = ((uint8_t)doc["ack"])==1;
			SERIAL_DEBUG.print("Need ack ");
			SERIAL_DEBUG.println(needAck);
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
	SERIAL_DEBUG.println("----------------------------------------");

	SERIAL_DEBUG.print(F("HEAD BIN: "));  SERIAL_DEBUG.print(configuration.HEAD, BIN);SERIAL_DEBUG.print(" ");SERIAL_DEBUG.print(configuration.HEAD, DEC);SERIAL_DEBUG.print(" ");SERIAL_DEBUG.println(configuration.HEAD, HEX);
	SERIAL_DEBUG.println(F(" "));
	SERIAL_DEBUG.print(F("AddH BIN: "));  SERIAL_DEBUG.println(configuration.ADDH, DEC);
	SERIAL_DEBUG.print(F("AddL BIN: "));  SERIAL_DEBUG.println(configuration.ADDL, DEC);
	SERIAL_DEBUG.print(F("Chan BIN: "));  SERIAL_DEBUG.print(configuration.CHAN, DEC); SERIAL_DEBUG.print(" -> "); SERIAL_DEBUG.println(configuration.getChannelDescription());
	SERIAL_DEBUG.println(F(" "));
	SERIAL_DEBUG.print(F("SpeedParityBit BIN    : "));  SERIAL_DEBUG.print(configuration.SPED.uartParity, BIN);SERIAL_DEBUG.print(" -> "); SERIAL_DEBUG.println(configuration.SPED.getUARTParityDescription());
	SERIAL_DEBUG.print(F("SpeedUARTDataRate BIN : "));  SERIAL_DEBUG.print(configuration.SPED.uartBaudRate, BIN);SERIAL_DEBUG.print(" -> "); SERIAL_DEBUG.println(configuration.SPED.getUARTBaudRate());
	SERIAL_DEBUG.print(F("SpeedAirDataRate BIN  : "));  SERIAL_DEBUG.print(configuration.SPED.airDataRate, BIN);SERIAL_DEBUG.print(" -> "); SERIAL_DEBUG.println(configuration.SPED.getAirDataRate());

	SERIAL_DEBUG.print(F("OptionTrans BIN       : "));  SERIAL_DEBUG.print(configuration.OPTION.fixedTransmission, BIN);SERIAL_DEBUG.print(" -> "); SERIAL_DEBUG.println(configuration.OPTION.getFixedTransmissionDescription());
	SERIAL_DEBUG.print(F("OptionPullup BIN      : "));  SERIAL_DEBUG.print(configuration.OPTION.ioDriveMode, BIN);SERIAL_DEBUG.print(" -> "); SERIAL_DEBUG.println(configuration.OPTION.getIODroveModeDescription());
	SERIAL_DEBUG.print(F("OptionWakeup BIN      : "));  SERIAL_DEBUG.print(configuration.OPTION.wirelessWakeupTime, BIN);SERIAL_DEBUG.print(" -> "); SERIAL_DEBUG.println(configuration.OPTION.getWirelessWakeUPTimeDescription());
	SERIAL_DEBUG.print(F("OptionFEC BIN         : "));  SERIAL_DEBUG.print(configuration.OPTION.fec, BIN);SERIAL_DEBUG.print(" -> "); SERIAL_DEBUG.println(configuration.OPTION.getFECDescription());
	SERIAL_DEBUG.print(F("OptionPower BIN       : "));  SERIAL_DEBUG.print(configuration.OPTION.transmissionPower, BIN);SERIAL_DEBUG.print(" -> "); SERIAL_DEBUG.println(configuration.OPTION.getTransmissionPowerDescription());

	SERIAL_DEBUG.println("----------------------------------------");

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
