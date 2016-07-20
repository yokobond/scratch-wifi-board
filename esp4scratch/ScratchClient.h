/*
 * File: ScratchClient.h
 * Author: Koji Yokokawa
 */

#ifndef __SCRATCH_CLIENT_H__
#define __SCRATCH_CLIENT_H__

#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include "FS.h"

//#define DEBUG_E4S(...) Serial.printf( __VA_ARGS__ )

#ifndef DEBUG_E4S
#define DEBUG_E4S(...)
#endif

struct ScratchClient {
    IPAddress ip;
    WiFiClient* wifi;
    unsigned long last_connected;
};

boolean scratch_multicast = false;
#define SCRATCH_CONFIG_FILE_NAME "/scratch.json"

bool loadScratchConfig() {
    DEBUG_E4S("\nloading ScratchConfig\n");
    File configFile = SPIFFS.open(SCRATCH_CONFIG_FILE_NAME, "r");
    if (!configFile) {
      Serial.println("Failed to open config file");
      return false;
    }
    size_t size = configFile.size();
    if (size > 1024) {
      Serial.println("Config file size is too large");
      return false;
    }
    std::unique_ptr<char[]> buf(new char[size]);
    configFile.readBytes(buf.get(), size);
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& json = jsonBuffer.parseObject(buf.get());
    if (!json.success()) {
      Serial.println("Failed to parse config file");
      return false;
    }
    scratch_multicast = json["multicast"];
    DEBUG_E4S("Scratch multicast = %s\n", (scratch_multicast ? "true" : "false"));
    return true;
}

bool saveScratchConfig() {
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["multicast"] = scratch_multicast;
  File configFile = SPIFFS.open(SCRATCH_CONFIG_FILE_NAME, "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return false;
  }
  json.printTo(configFile);
  return true;
}

#define SCRATCH_CLIENT_SIZE 128
ScratchClient scratch_clients[SCRATCH_CLIENT_SIZE];

IPAddress multi_ip_sta(255, 255, 255, 255);
const int scratch_port = 42001;
const int din4_pin = 4;
WiFiUDP UdpSta;
WiFiUDP UdpAp;


void setupScratch(void) {
    multi_ip_sta[0] = WiFi.localIP()[0];
    multi_ip_sta[1] = WiFi.localIP()[1];
    multi_ip_sta[2] = WiFi.localIP()[2];
    // read eeprom for ScratchConf
    if (!loadScratchConfig()) {
        // EEPROM was not initialized.
        saveScratchConfig();
    }

    // initialize scratch_clients
    for (int i = 0; i < SCRATCH_CLIENT_SIZE; i++) {
        scratch_clients[i].ip = IPAddress(0U);
        scratch_clients[i].wifi = new WiFiClient();
    }

    pinMode(din4_pin, INPUT_PULLUP);

    if (0 == UdpSta.beginMulticast(WiFi.localIP(), multi_ip_sta, scratch_port)) { // return 0 for success?
        DEBUG_E4S("\nUDP begin for multicasting on: %i\n", scratch_port);
    } else {
        DEBUG_E4S("\nFail to begin UDP on: %i\n", scratch_port);
    }
}

int receiveScratchMessageUDP(byte* buff) {
    // Scratch Remote Sensor Protocol on UDP
    int readSize = UdpSta.parsePacket();
    if ( readSize ) {
        UdpSta.read(buff, readSize); // read the packet into the buffer
        DEBUG_E4S("\n%d:Packet of %d received", millis() / 1000, readSize);
        DEBUG_E4S(" from %d.%d.%d.%d\n", UdpSta.remoteIP()[0], UdpSta.remoteIP()[1], UdpSta.remoteIP()[2], UdpSta.remoteIP()[3]);
        DEBUG_E4S("%s\n", buff);
    }
    // TODO: process the message
    return readSize;
}

void sendScratchMessageMulticast(char* message_data, uint16_t message_size) {
    UdpSta.beginPacketMulticast(multi_ip_sta, scratch_port, WiFi.localIP());
    if (0 != message_size) {
        DEBUG_E4S("UDP:[%d]%s\n", message_size, message_data);
        if (UdpSta.write((const uint8_t*)message_data, message_size)) {
        } else {
            DEBUG_E4S("send err\n");
        }
    }
    UdpSta.endPacket();
}

//
// TCP
//

ScratchClient* registerScratch(IPAddress client_ip) {
    DEBUG_E4S("register_scratch_client\n");
    for (int i = 0; i < SCRATCH_CLIENT_SIZE; i++) {
        if (scratch_clients[i].ip == client_ip) {
            DEBUG_E4S("connection already exists [%d] %d.%d.%d.%d\n",
                      i,
                      scratch_clients[i].ip[0], scratch_clients[i].ip[1], scratch_clients[i].ip[2], scratch_clients[i].ip[3]);
            return &scratch_clients[i];
        }
    }
    DEBUG_E4S("find space for new WiFiClient\n");
    for (int i = 0; i < SCRATCH_CLIENT_SIZE; i++) {
        if (scratch_clients[i].ip == IPAddress(0U)) {
            DEBUG_E4S("found empty at scratch_clients[%d]\n", i);
            scratch_clients[i].ip = client_ip;
            DEBUG_E4S("new connection %d.%d.%d.%d\n", scratch_clients[i].ip[0], scratch_clients[i].ip[1], scratch_clients[i].ip[2], scratch_clients[i].ip[3]);
            return &scratch_clients[i];
        }
    }
    // There is no empty room in scratch_clients.
    return NULL;
}

void removeScratch(IPAddress client_ip) {
    for (int i = 0; i < SCRATCH_CLIENT_SIZE; i++) {
        if ((scratch_clients[i].ip == client_ip)) {
            scratch_clients[i].ip = IPAddress(0U);
            if (scratch_clients[i].wifi) {
                scratch_clients[i].wifi->flush();
                scratch_clients[i].wifi->stop();
            }
        }
    }
}

void sendScratchMessageP2P(char* message_data, uint32_t message_size) {
    uint8_t size_data[4];
    size_data[0] = (uint8_t)((message_size >> 24) & 0xFF);
    size_data[1] = (uint8_t)((message_size >> 16) & 0xFF);
    size_data[2] = (uint8_t)((message_size >>  8) & 0xFF);
    size_data[3] = (uint8_t)((message_size >>  0) & 0xFF);
    for (int i = 0; i < SCRATCH_CLIENT_SIZE; i++) {
        WiFiClient* wifi = scratch_clients[i].wifi;
        if ((scratch_clients[i].ip != IPAddress(0U)) && wifi) {
            if (!wifi->connected()) {
                if (wifi->connect(scratch_clients[i].ip, scratch_port)) {
                    DEBUG_E4S("Scratch connected: %d.%d.%d.%d\n", scratch_clients[i].ip[0], scratch_clients[i].ip[1], scratch_clients[i].ip[2], scratch_clients[i].ip[3]);
                } else {
                    //DEBUG_E4S("fail to connect: %d.%d.%d.%d\n", scratch_clients[i].ip[0], scratch_clients[i].ip[1], scratch_clients[i].ip[2], scratch_clients[i].ip[3]);
                    continue;
                }
            }
            wifi->write((const uint8_t*)size_data, 4);
            wifi->write((const uint8_t*)message_data, message_size);
            scratch_clients[i].last_connected = millis();
            DEBUG_E4S("TCP:[%i]%s\n", message_size, message_data);
        }
    }
}

void (*messageReceivedCallback)(char* message_data, int message_size);

void attachMessageReceivedP2P(void (*handler)(char* message_data, int message_size)) {
    messageReceivedCallback = handler;
}

void dispatchSensorUpdateReceivedP2P(char* message_data, uint32_t message_size) {
    messageReceivedCallback(message_data, message_size);
}

void readScratchMessageP2P(void) {
    for (int i = 0; i < SCRATCH_CLIENT_SIZE; i++) {
        WiFiClient* wifi = scratch_clients[i].wifi;
        if ((scratch_clients[i].ip != IPAddress(0U)) && wifi) {
            if (!wifi->connected()) {
                if (wifi->connect(scratch_clients[i].ip, scratch_port)) {
                    DEBUG_E4S("Scratch connected: %d.%d.%d.%d\n", scratch_clients[i].ip[0], scratch_clients[i].ip[1], scratch_clients[i].ip[2], scratch_clients[i].ip[3]);
                } else {
                    //DEBUG_E4S("fail to connect: %d.%d.%d.%d\n", scratch_clients[i].ip[0], scratch_clients[i].ip[1], scratch_clients[i].ip[2], scratch_clients[i].ip[3]);
                    continue;
                }
            }
            if (wifi->available() <= 4) {
                continue;
            }
            uint8_t size_data[4];
            wifi->readBytes(size_data, 4);
            uint32_t message_size;
            message_size = (uint32_t) size_data[0] << 24;
            message_size |=  (uint32_t) size_data[1] << 16;
            message_size |= (uint32_t) size_data[2] << 8;
            message_size |= (uint32_t) size_data[3];
            char message_data[256];
            wifi->readBytes(message_data, message_size);
            dispatchSensorUpdateReceivedP2P(message_data, message_size);
            scratch_clients[i].last_connected = millis();
        }
    }
}

#endif
