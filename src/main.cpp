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
DS18B20 ds18b20(DS18B20_PIN, DS18B20_RESOLUTION);
GY906 gy906(GY906_ADDRESS);

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
			USE_SERIAL.printf("[WSc] Connected to url: %s\n", payload);

			// send message to server when Connected
			//webSocket.sendTXT("Connected");
			break;
		case WStype_TEXT:
			USE_SERIAL.printf("[WSc] get text: %s\n", payload);

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
	gy906.begin();
	ds18b20.begin();

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

void sendValues(unsigned long timestamp, float &temp1, float &temp2) {
	doc.clear();
	doc["temp1"] = temp1;
	doc["temp2_obj"] = temp2;
	//doc["temp2_amb"] = 24.8;
	doc["timestamp_ms"] = timestamp;
	char buffer[1024];
	size_t n = serializeJson(doc, buffer);
	bool status = webSocket.sendTXT(buffer, n);
	if (status) {
		Serial.println(buffer);
	}
}

void loop() {
	webSocket.loop();
	unsigned long report = millis();
	if(report - last_report >= 500) {
	last_report = report;
		float temp1 = ds18b20.getTemperature();
		float temp2 = gy906.readObjectTempC();
		if(webSocket.isConnected()) {
			sendValues(report, temp1, temp2);
		}
		bool electrodes = false;
		int ecg = 0;
		screen.showAllSensors(temp1, temp2, electrodes, ecg);
	}
}