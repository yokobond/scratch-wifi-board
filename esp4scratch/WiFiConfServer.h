/*
 * File: WiFiConfServer.h
 * Author: Koji Yokokawa
 */

#ifndef __WIFI_CONF_SERVER_H__
#define __WIFI_CONF_SERVER_H__

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

//#define DEBUG_WIFICONF(...) Serial.printf( __VA_ARGS__ )

#ifndef DEBUG_WIFICONF
#define DEBUG_WIFICONF(...)
#endif


ESP8266WebServer server(80);

#define WIFI_CONF_FORMAT {0, 0, 0, 1}
const uint8_t wifi_conf_format[] = WIFI_CONF_FORMAT;
#define NAME_PREF "e4s-"

String network_html;

#define WIFI_CONF_START 0

struct WiFiConfStruct {
    uint8_t format[4];
    char sta_ssid[32];
    char sta_pwd[64];
    char module_id[32];
} WiFiConf = {
    WIFI_CONF_FORMAT,
    "ssid",
    "password",
    ""
};

void printWiFiConf(void) {
    DEBUG_WIFICONF("SSID: %s\nPassword: %s\nModule ID: %s", WiFiConf.sta_ssid, WiFiConf.sta_pwd, WiFiConf.module_id);
}

bool loadWiFiConf() {
    DEBUG_WIFICONF("\nLoading WiFiConf\n");
    if (EEPROM.read(WIFI_CONF_START + 0) == wifi_conf_format[0] &&
        EEPROM.read(WIFI_CONF_START + 1) == wifi_conf_format[1] &&
        EEPROM.read(WIFI_CONF_START + 2) == wifi_conf_format[2] &&
        EEPROM.read(WIFI_CONF_START + 3) == wifi_conf_format[3])
    {
        for (unsigned int t = 0; t < sizeof(WiFiConf); t++) {
            *((char*)&WiFiConf + t) = EEPROM.read(WIFI_CONF_START + t);
        }
        printWiFiConf();
        return true;
    } else {
        DEBUG_WIFICONF("WiFiConf was not saved on EEPROM.\n");
        return false;
    }
}

void saveWiFiConf(void) {
    DEBUG_WIFICONF("writing WiFiConf\n");
    for (unsigned int t = 0; t < sizeof(WiFiConf); t++) {
        EEPROM.write(WIFI_CONF_START + t, *((char*)&WiFiConf + t));
    }
    EEPROM.commit();
    printWiFiConf();
}

void setDefaultModuleId(char* dst) {
    uint8_t macAddr[WL_MAC_ADDR_LENGTH];
    WiFi.macAddress(macAddr);
    sprintf(dst, "%s%02x%02x", NAME_PREF, macAddr[WL_MAC_ADDR_LENGTH - 2], macAddr[WL_MAC_ADDR_LENGTH - 1]);
}

void resetModuleId(void) {
    uint8_t macAddr[WL_MAC_ADDR_LENGTH];
    WiFi.macAddress(macAddr);
    setDefaultModuleId(WiFiConf.module_id);
    DEBUG_WIFICONF("Reset Module ID to default: %s", WiFiConf.module_id);
}

void scanWiFi(void) {
    int founds = WiFi.scanNetworks();
    DEBUG_WIFICONF("\nScan was done\n");
    if (founds == 0) {
        DEBUG_WIFICONF("\nNo networks found\n");
    } else {
        DEBUG_WIFICONF("%i networks found\n", founds);
        for (int i = 0; i < founds; ++i) {
            // Print SSID and RSSI for each network found
            DEBUG_WIFICONF("%i: %s (%i) %s\n", i + 1, WiFi.SSID(i), WiFi.RSSI(i), (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");
            delay(10);
        }
    }
    network_html = "<ol>";
    for (int i = 0; i < founds; ++i)
    {
        // Print SSID and RSSI for each network found
        network_html += "<li>";
        network_html += WiFi.SSID(i);
        network_html += " (";
        network_html += WiFi.RSSI(i);
        network_html += ")";
        network_html += (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*";
        network_html += "</li>";
    }
    network_html += "</ol>";
}

int waitConnected(void) {
    int wait = 0;
    DEBUG_WIFICONF("Waiting for WiFi to connect\n");
    while ( wait < 60 ) {
        if (WiFi.status() == WL_CONNECTED) {
            DEBUG_WIFICONF("\nWiFi connected\n");
            return (1);
        }
        delay(500);
        DEBUG_WIFICONF("%i", WiFi.status());
        wait++;
    }
    DEBUG_WIFICONF("Connect timed out\n");
    return (0);
}

void printIP(void) {
    DEBUG_WIFICONF("Name: %s.local \nLAN: %i.%i.%i.%i\nAP: %i.%i.%i.%i\n",
                   WiFiConf.module_id,
                   WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3],
                   WiFi.softAPIP()[0], WiFi.softAPIP()[1], WiFi.softAPIP()[2], WiFi.softAPIP()[3]);
}

void setupWiFiConfWeb(void) {
    server.on("/wifi_conf", [] () {
        IPAddress ip = WiFi.localIP();
        String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
        String content = "<!DOCTYPE HTML>\r\n<html><head><title>";
        content += WiFiConf.module_id;
        content += ".local - Configuration";
        content += "</title></head><body>";
        content += "<h1>Configuration of ESP8266</h1>";
        content += "<p>LAN: ";
        content += WiFiConf.sta_ssid;
        content += "</br>IP: ";
        content += ipStr;
        content += " ( ";
        content += WiFiConf.module_id;
        content += ".local";
        content += " )</p>";
        content += "<p>";
        content += "</p><form method='get' action='set_wifi_conf'><label for='ssid'>SSID: </label><input name='ssid'id='ssid' maxlength=32 value=''>";
        content += "<label for='pwd'>PASS: </label> <input type='password' name='pwd' id='pwd' />";
        content += "<input type='submit' onclick='return confirm(\"Are you sure you want to change the WiFi settings?\");'></form>";
        content += network_html;
        content += "</body></html>";
        server.send(200, "text/html", content);
    });

    server.on("/set_wifi_conf", [] () {
        String new_ssid = server.arg("ssid");
        String new_pwd = server.arg("pwd");
        String content = "<!DOCTYPE HTML>\r\n<html><head><title>";
        content += WiFiConf.module_id;
        content += ".local - set WiFi";
        content += "</title></head><body>";
        content += "<h1>Set WiFi of ESP8266</h1>";
        if (new_ssid.length() > 0) {
            new_ssid.toCharArray(WiFiConf.sta_ssid, sizeof(WiFiConf.sta_ssid));
            new_pwd.toCharArray(WiFiConf.sta_pwd, sizeof(WiFiConf.sta_pwd));
            saveWiFiConf();
            content += "<p>saved '";
            content += WiFiConf.sta_ssid;
            content += "'... Reset to boot into new WiFi</p>";
            content += "<body></html>";
        } else {
            content += "<p>Empty SSID is not acceptable. </p>";
            content += "<body></html>";
            DEBUG_WIFICONF("Rejected empty SSID.\n");
        }
        server.send(200, "text/html", content);
    });

    server.on("/module_id", [] () {
        IPAddress ip = WiFi.localIP();
        String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
        char defaultId[sizeof(WiFiConf.module_id)];
        setDefaultModuleId(defaultId);
        String content = "<!DOCTYPE HTML>\r\n<html><head><title>";
        content += WiFiConf.module_id;
        content += ".local - Module ID";
        content += "</title></head><body>";
        content += "<h1>Module ID of ESP8266</h1>";
        content += "<p>Module ID: ";
        content += WiFiConf.module_id;
        content += "</br>IP: ";
        content += ipStr;
        content += "</p>";
        content += "<p>";
        content += "<form method='get' action='set_module_id'><label for='module_id'>New Module ID: </label><input name='module_id' id='module_id' maxlength=32 value='";
        content += WiFiConf.module_id;
        content += "'><input type='submit' onclick='return confirm(\"Are you sure you want to change the Module ID?\");'></form>";
        content += " Empty will reset to default ID '";
        content += defaultId;
        content += "'</p>";
        content += "</body></html>";
        server.send(200, "text/html", content);
    });

    server.on("/set_module_id", [] () {
        String new_id = server.arg("module_id");
        String content = "<!DOCTYPE HTML>\r\n<html><head>";
        content += "<title>";
        content += WiFiConf.module_id;
        content += ".local - set WiFi";
        content += "</title>";
        content += "</head><body>";
        if (new_id.length() > 0) {
            new_id.toCharArray(WiFiConf.module_id, sizeof(WiFiConf.module_id));
        } else {
            resetModuleId();
        }
        saveWiFiConf();
        content += "<h1>Set WiFi of ESP8266</h1>";
        content += "<p>Set Module ID to '";
        content += WiFiConf.module_id;
        content += "' ... Restart to applay it. </p>";
        content += "</body></html>";
        server.send(200, "text/html", content);
    });
}

const char* sketchUploadForm = "<form method='POST' action='/upload_sketch' enctype='multipart/form-data'><input type='file' name='sketch'><input type='submit' value='Upload' onclick='return confirm(\"Are you sure you want to update the Sketch?\");'></form>";

void setupWebUpdate(void) {
    server.on("/update", HTTP_GET, [] () {
        server.sendHeader("Connection", "close");
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(200, "text/html", sketchUploadForm);
    });
    server.onFileUpload([] () {
        if (server.uri() != "/upload_sketch") return;
        HTTPUpload& upload = server.upload();
        if (upload.status == UPLOAD_FILE_START) {
            //      Serial.setDebugOutput(true);
            WiFiUDP::stopAll();
            DEBUG_WIFICONF("Sketch: %s\n", upload.filename.c_str());
            uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
            if (!Update.begin(maxSketchSpace)) { //start with max available size
                Update.printError(Serial);
            }
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                Update.printError(Serial);
            }
        } else if (upload.status == UPLOAD_FILE_END) {
            if (Update.end(true)) { //true to set the size to the current progress
                DEBUG_WIFICONF("Update Success: %u\nRebooting...\n", upload.totalSize);
            } else {
                Update.printError(Serial);
            }
            //      Serial.setDebugOutput(false);
        }
        yield();
    });
    server.on("/upload_sketch", HTTP_POST, [] () {
        server.sendHeader("Connection", "close");
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
        ESP.restart();
    });
}

void setupWiFiConf() {
    EEPROM.begin(512);
    delay(10);
    DEBUG_WIFICONF("\n\nStartup\n");

    // read eeprom for ssid and pass
    if (!loadWiFiConf()) {
        // EEPROM was not initialized.
        resetModuleId();
        saveWiFiConf();
    }

    // scan Access Points
    scanWiFi();

    // start WiFi
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(WiFiConf.sta_ssid, WiFiConf.sta_pwd);
    waitConnected();
    if (WiFi.status() == WL_CONNECTED) {
        WiFi.softAP(WiFiConf.module_id);
    } else {
        WiFi.mode(WIFI_AP);
        WiFi.softAP(WiFiConf.module_id);
    }
    printIP();

    // setup Web Interface
    setupWiFiConfWeb();
    setupWebUpdate();

    // start Web Server
    server.begin();
    DEBUG_WIFICONF("\nServer started\n");

    // start mDNS responder
    if (!MDNS.begin(WiFiConf.module_id)) {
        DEBUG_WIFICONF("\nError setting up MDNS responder!\n");
        while (1) {
            delay(1000);
        }
    }
    DEBUG_WIFICONF("\nmDNS responder started\n");

    // Add service to MDNS-SD
    MDNS.addService("http", "tcp", 80);
}

#endif
