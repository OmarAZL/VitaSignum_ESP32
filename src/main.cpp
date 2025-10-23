/*
 * WebSocketClient.ino
 *
 *  Created on: 24.05.2015
 *
 */

#include <Arduino.h>

#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClientSecure.h>

#include <WebSocketsClient.h>
#include <ArduinoJson.h>

#include "Sensors/DS18B20.h"
#include "Sensors/GY906.h"
#include "Screen.h"
#include "config.h"
#include "env.h" // Variables sensibles.

Screen screen(SCREEN_ADDRESS, SCREEN_WIDTH, SCREEN_HEIGHT);

JsonDocument doc;

WiFiMulti wifiMulti;
WebSocketsClient webSocket;

#define PORT 52211

#define USE_SERIAL Serial

void hexdump(const void *mem, uint32_t len, uint8_t cols = 16) {
	const uint8_t* src = (const uint8_t*) mem;
	USE_SERIAL.printf("\n[HEXDUMP] Address: 0x%08X len: 0x%X (%d)", (ptrdiff_t)src, len, len);
	for(uint32_t i = 0; i < len; i++) {
		if(i % cols == 0) {
			USE_SERIAL.printf("\n[0x%08X] 0x%08X: ", (ptrdiff_t)src, i);
		}
		USE_SERIAL.printf("%02X ", *src);
		src++;
	}
	USE_SERIAL.printf("\n");
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {

	switch(type) {
		case WStype_DISCONNECTED:
			USE_SERIAL.printf("[WSc] Disconnected!\n");
			break;
		case WStype_CONNECTED:
		{
			USE_SERIAL.printf("[WSc] Connected to url: %s\n", payload);

			doc.clear();
			doc["device"] = WiFi.macAddress();
			char buffer[64];
			size_t n = serializeJson(doc, buffer);
			bool status = webSocket.sendTXT(buffer, n);
			if (status) {
				webSocket.sendTXT(buffer);
			}
			
			break;
		}
		case WStype_TEXT:
			USE_SERIAL.printf("[WSc] %s\n", payload);

			// send message to server
			// webSocket.sendTXT("message here");
			break;
		case WStype_BIN:
			USE_SERIAL.printf("[WSc] get binary length: %u\n", length);
			hexdump(payload, length);

			// send data to server
			// webSocket.sendBIN(payload, length);
			break;
		case WStype_ERROR:			
		case WStype_FRAGMENT_TEXT_START:
		case WStype_FRAGMENT_BIN_START:
		case WStype_FRAGMENT:
		case WStype_FRAGMENT_FIN:
			break;
	}

}

void setup() {
	// USE_SERIAL.begin(921600);
	USE_SERIAL.begin(115200);
	
	Wire.begin(PIN_SDA, PIN_SCL);
	screen.begin();

	//Serial.setDebugOutput(true);
	USE_SERIAL.setDebugOutput(true);

	USE_SERIAL.println();
	USE_SERIAL.println();
	USE_SERIAL.println();

	bool screenCheck = screen.isConnected();
    while (!screenCheck) {
        Serial.println("Esperando pantalla OLED...");
        screenCheck = screen.isConnected();
        delay(2000);
    }

	for(uint8_t t = 4; t > 0; t--) {
		USE_SERIAL.printf("[SETUP] BOOT WAIT %d...\n", t);
		screen.clear();
		screen.getDisplay().printf("[SETUP] BOOT WAIT %d...", t);
		screen.updateDisplay();
		USE_SERIAL.flush();
		delay(1000);
	}

	wifiMulti.addAP(SSID, PASS);

	//WiFi.disconnect();
	while(wifiMulti.run() != WL_CONNECTED) {
		delay(100);
	}

	// server address, port and URL
	webSocket.begin(IP, PORT, "/");

	// event handler
	webSocket.onEvent(webSocketEvent);

	// use HTTP Basic Authorization this is optional remove if not needed
	//webSocket.setAuthorization("user", "Password");

	// try ever 5000 again if connection has failed
	webSocket.setReconnectInterval(5000);

	screen.showMessage("Hola Mundo, welcome back");
	delay(500);

}

unsigned long last_report = 0;
int cont = 0;

void sendValues(unsigned long timestamp, float &temp1, float &temp2_obj, float &temp2_amb) {
	String mac = WiFi.macAddress();
	doc.clear();
	doc["device"] = mac;
	doc[mac]["temp1"] = temp1;
	doc[mac]["temp2_obj"] = temp2_obj;
	doc[mac]["temp2_amb"] = temp2_amb;
	doc[mac]["timestamp_ms"] = timestamp;
	char buffer[1024];
	size_t n = serializeJson(doc, buffer);
	bool status = webSocket.sendTXT(buffer, n);
	if (status) {
		Serial.println(buffer);
	}
}

void showValues(unsigned long timestamp, float &temp1, float &temp2) {
	screen.getDisplay().clearDisplay();
	screen.getDisplay().setCursor(0, 0);
	screen.getDisplay().println("MAC: " + WiFi.macAddress());
	screen.getDisplay().println("Temperatura 1: " + String(temp1));
	screen.getDisplay().println("Temperatura 2: " + String(temp2));
	screen.getDisplay().println("Timestamp MS: " + String(timestamp));
	screen.getDisplay().display();
}

float temp1 = 0;
float temp2_obj = 0;
float temp2_amb = 0;

void loop() {
	webSocket.loop();
	unsigned long report = millis();
	if(report - last_report >= 1500) {
	last_report = report;
		sendValues(report, temp1, temp2_obj, temp2_amb);
		showValues(report, temp1, temp2_obj);
	}
}