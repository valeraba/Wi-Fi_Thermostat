/*
Copyright © 2015, BVAgile. All rights reserved.
Contacts: <bvagile@gmail.com>
*/

#include "MgtClient.h"
#include <Arduino.h>
#include <ESP8266WiFi.h>

void debugLog(const __FlashStringHelper* aFormat, ...);

extern const char* WIFI_SSID;
extern const char* WIFI_PASSWORD;

TimeStamp shiftTime;

WiFiClient socket;
bool isConnected = false;

TimeStamp getUTCTime() {
	static TimeStamp delta = 0;
	static __uint32 prev = 0;
	__uint32 curr = millis();
	if (prev > curr)
		delta += 0xffffffffUL;
	prev = curr;
	return delta + curr + shiftTime;
}

void sleepms(__uint32 aMilliseconds) {
	delay(aMilliseconds);
}

static bool socket_open(const char* aHost, __uint16 aPort) {
	debugLog(F("socket open\n"));
	if (WiFi.status() != WL_CONNECTED) {
		WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
		while (WiFi.status() != WL_CONNECTED) {
			delay(500);
			debugLog(F("."));
		}
	}

	if (!socket.connect(aHost, aPort)) {
		debugLog(F("socket connection failed\n"));
		return false;
	}
	isConnected = true;
	socket.setNoDelay(true);
	return true;
}

static void socket_close() {
	if (isConnected) {
		socket.stop();
		isConnected = false;
	}
}

static bool socket_send(const void* aBuf, __uint16 aSize) {
	const __uint8* data = (const __uint8*)aBuf;
	while (aSize > 0) {
	  if (!socket.connected()) {
  	  debugLog(F("socket send error\n"));
	    socket_close();
	    return false;
	  }
	  else {
	    int writtenSize = socket.write(data, aSize);
	    if (writtenSize < 0) {
	      debugLog(F("socket send error\n"));
	      return false;
	    }
	    aSize -= writtenSize;
	    data += writtenSize;
	  }
	}
	return true;
}


static int socket_receive(void* aBuf, __uint16 aSize) {
	int readSize = socket.available();
	if (readSize) {
		if (readSize > aSize)
			readSize = aSize;
		socket.read((__uint8*)aBuf, readSize);
		return readSize;
	}

	if (!socket.connected()) {
		debugLog(F("socket connection abort\n"));
		socket_close();
		return -1;
	}
	return 0;
}


static void socket_shutdown() {
}

static bool socket_isTxBusy() {
	return false;
}

static bool socket_isConnected() {
	return isConnected;
}

struct PortableSocket mySocket = {
	socket_open,
	socket_receive,
	socket_send,
	socket_shutdown,
	socket_close,
	socket_isTxBusy,
	socket_isConnected
};
