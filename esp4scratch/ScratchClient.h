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

//#define DEBUG
//#define DEBUG_E4S(...) Serial.printf( __VA_ARGS__ )
//#define DEBUG_E4S(x) Serial.print(x)

#ifndef DEBUG_E4S
#define DEBUG_E4S(x)
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
    DEBUG_E4S(String("Scratch multicast = ") + String((scratch_multicast ? "true" : "false")));
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

#define SCRATCH_CLIENTS_FILE_NAME "/scratch_clients.json"

void initScratchClients() {
    for (int i = 0; i < SCRATCH_CLIENT_SIZE; i++) {
        scratch_clients[i].ip = IPAddress(0U);
        scratch_clients[i].wifi = new WiFiClient();
    }
}

bool loadScratchClients() {
    DEBUG_E4S("\nloading scratch_clients\n");
    initScratchClients();
    File configFile = SPIFFS.open(SCRATCH_CLIENTS_FILE_NAME, "r");
    if (!configFile) {
        DEBUG_E4S("Failed to open scratch_clients file");
        return false;
    }
    size_t size = configFile.size();
    if (size > 1024) {
        DEBUG_E4S("scratch_clients file size is too large");
        return false;
    }
    std::unique_ptr<char[]> buf(new char[size]);
    configFile.readBytes(buf.get(), size);
    StaticJsonBuffer<SCRATCH_CLIENT_SIZE> jsonBuffer;
    JsonArray& array = jsonBuffer.parseArray(buf.get());
#ifdef DEBUG
    array.prettyPrintTo(Serial);
#endif
    if (!array.success()) {
        DEBUG_E4S("Failed to parse scratch_clients file");
        return false;
    }
    int i = 0;
    for (JsonArray::iterator it = array.begin(); it != array.end(); ++it) {
        JsonArray& ipJson = *it;
        DEBUG_E4S(String("\nLoad scratch_client")
                + String("[") + String(i, DEC) + String("]")
                + String(ipJson[0].as<int>(), DEC) + String(".")
                + String(ipJson[1].as<int>(), DEC) + String(".")
                + String(ipJson[2].as<int>(), DEC) + String(".")
                + String(ipJson[3].as<int>(), DEC));
        IPAddress ip(ipJson[0].as<int>(), ipJson[1].as<int>(), ipJson[2].as<int>(), ipJson[3].as<int>());
        scratch_clients[i].ip = ip;
        i++;
    }
    return true;
}

bool saveScratchClients() {
    DEBUG_E4S("\nSaving scratch_clients\n");
    StaticJsonBuffer<SCRATCH_CLIENT_SIZE> jsonBuffer;
    JsonArray& json = jsonBuffer.createArray();
    for (int i = 0; i < SCRATCH_CLIENT_SIZE; i++) {
        if (scratch_clients[i].ip != IPAddress(0U)) {
            JsonArray& ip = json.createNestedArray();
            ip.add(scratch_clients[i].ip[0]);
            ip.add(scratch_clients[i].ip[1]);
            ip.add(scratch_clients[i].ip[2]);
            ip.add(scratch_clients[i].ip[3]);
        }
    }
    File configFile = SPIFFS.open(SCRATCH_CLIENTS_FILE_NAME, "w");
    if (!configFile) {
        Serial.println("Failed to open config file for writing");
        return false;
    }
    json.printTo(configFile);
#ifdef DEBUG
    json.prettyPrintTo(Serial);
#endif
    return true;
}

void setupScratch(void) {
    pinMode(din4_pin, INPUT_PULLUP);
    multi_ip_sta[0] = WiFi.localIP()[0];
    multi_ip_sta[1] = WiFi.localIP()[1];
    multi_ip_sta[2] = WiFi.localIP()[2];
    // read eeprom for ScratchConf
    if (!loadScratchConfig()) {
        // config was not initialized.
        saveScratchConfig();
    }
    if (0 == UdpSta.beginMulticast(WiFi.localIP(), multi_ip_sta, scratch_port)) { // return 0 for success?
        DEBUG_E4S(String("\nUDP begin for multicasting on: ") + scratch_port);
    } else {
        DEBUG_E4S(String("\nFail to begin UDP on: \n") + scratch_port);
    }
    loadScratchClients();
}

int receiveScratchMessageUDP(byte* buff) {
    // Scratch Remote Sensor Protocol on UDP
    int readSize = UdpSta.parsePacket();
    if ( readSize ) {
        UdpSta.read(buff, readSize); // read the packet into the buffer
//        DEBUG_E4S(String("\n") + millis() / 1000 + ":Packet of " + readSize + " received");
//        DEBUG_E4S(" from %d.%d.%d.%d\n", UdpSta.remoteIP()[0], UdpSta.remoteIP()[1], UdpSta.remoteIP()[2], UdpSta.remoteIP()[3]);
//        DEBUG_E4S("%s\n", buff);
    }
    // TODO: process the message
    return readSize;
}

void sendScratchMessageMulticast(char* message_data, uint16_t message_size) {
    UdpSta.beginPacketMulticast(multi_ip_sta, scratch_port, WiFi.localIP());
    if (0 != message_size) {
        DEBUG_E4S(String("UDP:[") + message_size + "] " + message_data);
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
            DEBUG_E4S(String("connection already exists [") + i + "] "
                    + scratch_clients[i].ip[0] + "." + scratch_clients[i].ip[1] + "." + scratch_clients[i].ip[2] + "." + scratch_clients[i].ip[3]);
            return &scratch_clients[i];
        }
    }
    DEBUG_E4S("find space for new WiFiClient\n");
    for (int i = 0; i < SCRATCH_CLIENT_SIZE; i++) {
        if (scratch_clients[i].ip == IPAddress(0U)) {
            DEBUG_E4S(String("found empty at scratch_clients[") + i + "]");
            scratch_clients[i].ip = client_ip;
//            DEBUG_E4S("new connection %d.%d.%d.%d\n", scratch_clients[i].ip[0], scratch_clients[i].ip[1], scratch_clients[i].ip[2], scratch_clients[i].ip[3]);
            saveScratchClients();
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
    saveScratchClients();
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
                    DEBUG_E4S(String("Scratch connected: ") + scratch_clients[i].ip[0] + "." + scratch_clients[i].ip[1] + "." + scratch_clients[i].ip[2] + "." + scratch_clients[i].ip[3]);
                } else {
//                    DEBUG_E4S(String("fail to connect: ") + scratch_clients[i].ip[0] + "." + scratch_clients[i].ip[1] + "." + scratch_clients[i].ip[2] + "." + scratch_clients[i].ip[3]);
                    continue;
                }
            }
            wifi->write((const uint8_t*)size_data, 4);
            wifi->write((const uint8_t*)message_data, message_size);
            scratch_clients[i].last_connected = millis();
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
//                    DEBUG_E4S(String("Scratch connected: ") + scratch_clients[i].ip[0] + "." + scratch_clients[i].ip[1] + "." + scratch_clients[i].ip[2] + "." + scratch_clients[i].ip[3]);
                } else {
//                    DEBUG_E4S(String("fail connected: ") + scratch_clients[i].ip[0] + "." + scratch_clients[i].ip[1] + "." + scratch_clients[i].ip[2] + "." + scratch_clients[i].ip[3]);
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
            message_data[message_size] = '\n';
//            DEBUG_E4S(String("Received[") + message_size + "]:" + message_data);
            dispatchSensorUpdateReceivedP2P(message_data, message_size);
            scratch_clients[i].last_connected = millis();
        }
    }
}

#endif
