#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>

#include "WiFiConfServer.h"

#define BAUD 9600
#define LED 5

void setup() {
    Serial.begin(BAUD);
    setupWiFiConf();
    setupWeb();
    setupCommandPort();
    pinMode(LED, OUTPUT);
    for (int i = 0; i < 3; i++) {
        digitalWrite(LED, HIGH);
        delay(100);
        digitalWrite(LED, LOW);
        delay(100);
    }
}


//
// Application
//

#include "ScratchClient.h"


void setupWeb(void) {
    server.on("/", []() {
        IPAddress ip = WiFi.localIP();
        String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
        String content = "<!DOCTYPE HTML>\r\n<html><head><title>";
        content += WiFiConf.module_id;
        content += ".local";
        content += "</title></head><body>";
        content += "<h1>Scratch WiFi Board</h1>";
        content += "<p>LAN: ";
        content += WiFiConf.sta_ssid;
        content += "</br>IP: ";
        content += ipStr;
        content += " ( ";
        content += WiFiConf.module_id;
        content += ".local";
        content += " )</p>";
        content += "<p>Scratch Remote Sensor";
        content += "</p>";
        content += "<ul>";
        content += "<li><a href='/change_scratch_ip'>P2P Connect</a>";
        content += "<li><a href='/scratch_conf'>Multicast</a>";
        content += "</ul>";
        content += "<hr>";
        content += "<p>ESP8266 configuration";
        content += "</p>";
        content += "<ul>";
        content += "<li><a href='/wifi_conf'>Setup WiFi</a>";
        content += "<li><a href='/module_id'>Setup Module ID</a>";
        content += "<li><a href='/update'>Update Sketch</a>";
        content += "</ul>";
        content += "</body></html>";
        server.send(200, "text/html", content);
    });

    setupScratch();
    setupScratchWeb();
}

void setupScratchWeb(void) {

    server.on("/scratch_conf", []() {
        String content = "<!DOCTYPE HTML>\r\n<html><head><title>";
        content += "Scratch Configuration";
        content += "</title></head><body>";
        content += "<h1>Scratch Configuration</h1>";
        content += "<p>";
        content += "</p><form method='get' action='set_scratch_conf'><label>Scratch Configuration: </label>";
        content += "<input type='checkbox' name='scratch_multicast' value='1' id= 'multicast'";
        if (ScratchConf.multicast) {
            content += " checked='checked'";
        }
        content += "><label for='multicast'>Multicast Module-IP</label> ";
        content += "<input type='submit'></form>";
        content += "<p><a href='/'>Return to Top</a></p>";
        content += "</html>";
        server.send(200, "text/html", content);
    });

    server.on("/set_scratch_conf", []() {
        if (server.arg("scratch_multicast").equals(String("1"))) {
            DEBUG_E4S("Set scratch_multicast to true\n");
            ScratchConf.multicast = true;
        } else {
            DEBUG_E4S("Set scratch_multicast to false\n");
            ScratchConf.multicast = false;
        }
        saveScratchConf(SCRATCH_CONFIG_START);
        String content;
        content = "<!DOCTYPE HTML>\r\n<html>";
        content += "<p>Saved to EEPROM </p>";
        content += "<p>Multicast Module-IP: ";
        if (ScratchConf.multicast) {
            content += "true";
        } else {
            content += "false";
        }
        content += "</p>";
        content += "</body></html>";
        server.send(200, "text/html", content);
    });

    server.on("/change_scratch_ip", []() {
        IPAddress scratch_ip = IPAddress(server.arg("scratch_ip_0").toInt(), server.arg("scratch_ip_1").toInt(), server.arg("scratch_ip_2").toInt(), server.arg("scratch_ip_3").toInt());
        String content = "<!DOCTYPE HTML>\r\n<html><head><title>";
        content += "Scratch List";
        content += "</title></head><body>";
        content += "<h1>Scratch List</h1>";
        if (server.arg("operation").equals(String("remove"))) {
            removeScratch(scratch_ip);
            content += "<p>Removed: ";
            content += String(scratch_ip[0]) + '.' + String(scratch_ip[1]) + '.' + String(scratch_ip[2]) + '.' + String(scratch_ip[3]);
            content += "</p>";
        } else if (server.arg("operation").equals(String("add"))) {
            ScratchClient* client = registerScratch(scratch_ip);
            if (client) {
                content += "<p>Success to register: ";
            } else {
                content += "<p>Fail to register: ";
            }
            content += String(scratch_ip[0]) + '.' + String(scratch_ip[1]) + '.' + String(scratch_ip[2]) + '.' + String(scratch_ip[3]);
            content += "</p>";
        } else {
            content += "<p>";
            content += "<a href='/add_scratch_ip'>Add New Scratch</a>";
            content += " | ";
            content += "<a href='/remove_scratch_ip'>Remove Registered Scratch</a>";
            content += "</p>";
        }
        content += "<p><a href='/'>Return to Top</a></p>";
        content += "<p>Registered: </p>";
        content += "<ul>";
        for (int i = 0; i < SCRATCH_CLIENT_SIZE; i++) {
            if (scratch_clients[i].ip != IPAddress(0U)) {
                content += "<li>";
                content += String(scratch_clients[i].ip[0]) + '.' + String(scratch_clients[i].ip[1]) + '.' + String(scratch_clients[i].ip[2]) + '.' + String(scratch_clients[i].ip[3]);
            }
        }
        content += "</ul>";
        content += "</body></html>";
        server.send(200, "text/html", content);
    });

    server.on("/add_scratch_ip", []() {
        IPAddress client_ip = server.client().remoteIP();
        String content = "<!DOCTYPE HTML>\r\n<html><head><title>";
        content += "Add New Scratch";
        content += "</title></head><body>";
        content += "<h1>Add New Scratch</h1>";
        content += "<form method='get' action='change_scratch_ip'><label>Scratch IP: </label>";
        content += "<input name='operation' value='add' type='hidden'>";
        content += "<input name='scratch_ip_0' maxlength=3 value='";
        content += String(client_ip[0]);
        content += "'>.";
        content += "<input name='scratch_ip_1' maxlength=3 value='";
        content += String(client_ip[1]);
        content += "'>.";
        content += "<input name='scratch_ip_2' maxlength=3 value='";
        content += String(client_ip[2]);
        content += "'>.";
        content += "<input name='scratch_ip_3' maxlength=3 value='";
        content += String(client_ip[3]);
        content += "'>";
        content += "<input type='submit'></form></p>";
        content += "<p><a href='/change_scratch_ip'>Return to Scratch List</a></p>";
        content += "<p>Registered: </p>";
        content += "<ul>";
        for (int i = 0; i < SCRATCH_CLIENT_SIZE; i++) {
            if (scratch_clients[i].ip != IPAddress(0U)) {
                content += "<li>";
                content += String(scratch_clients[i].ip[0]) + '.' + String(scratch_clients[i].ip[1]) + '.' + String(scratch_clients[i].ip[2]) + '.' + String(scratch_clients[i].ip[3]);
            }
        }
        content += "</ul>";
        content += "</body></html>";
        server.send(200, "text/html", content);
    });

    server.on("/remove_scratch_ip", []() {
        IPAddress client_ip = server.client().remoteIP();
        String content = "<!DOCTYPE HTML>\r\n<html><head><title>";
        content += "Remove Scratch";
        content += "</title></head><body>";
        content += "<h1>Remove Scratch</h1>";
        content += "<p><a href='/change_scratch_ip'>Return to Scratch List</a></p>";
        content += "<p>Registered: </p>";
        for (int i = 0; i < SCRATCH_CLIENT_SIZE; i++) {
            if (scratch_clients[i].ip != IPAddress(0U)) {
                content += "<form method='get' action='change_scratch_ip'>";
                content += "<input name='operation' value='remove' type='hidden'>";
                content += "<label>";
                content += String(scratch_clients[i].ip[0]) + '.' + String(scratch_clients[i].ip[1]) + '.' + String(scratch_clients[i].ip[2]) + '.' + String(scratch_clients[i].ip[3]);
                content += "</label>";
                content += "<input name='scratch_ip_0' maxlength=3 value='";
                content += String(client_ip[0]);
                content += "' type='hidden'>";
                content += "<input name='scratch_ip_1' maxlength=3 value='";
                content += String(client_ip[1]);
                content += "' type='hidden'>";
                content += "<input name='scratch_ip_2' maxlength=3 value='";
                content += String(client_ip[2]);
                content += "' type='hidden'>";
                content += "<input name='scratch_ip_3' maxlength=3 value='";
                content += String(client_ip[3]);
                content += "' type='hidden'>";
                content += " <input type='submit' value='remove'>";
                content += "</form><br/>";
            }
        }
        content += "</body></html>";
        server.send(200, "text/html", content);
    });
}


//
// Serial Interface
//

#define COMMAND_PORT Serial
String command_buffer;         // a string to hold incoming data
boolean command_end = false;  // whether the string is complete

void receivedCallback(char* message_data, int message_size) {
    DEBUG_E4S("callback:[%d]%s\n", message_size, message_data);
}

void setupCommandPort(void) {
    COMMAND_PORT.begin(9600);
    command_buffer.reserve(256);
    COMMAND_PORT.println("ready:esp4scratch");
    attachMessageReceivedP2P(receivedCallback);
}

boolean cycleCheck(unsigned long *lastMillis, unsigned int cycle)
{
    unsigned long currentMillis = millis();
    if (currentMillis - *lastMillis >= cycle)
    {
        *lastMillis = currentMillis;
        return true;
    } else {
        return false;
    }
}

void readCommand(void) {
    while (COMMAND_PORT.available()) {
        char in_char = (char)COMMAND_PORT.read();
        if (in_char == '\n') {
            command_end = true;
            break;
        } else {
            command_buffer += in_char;
        }
    }
}

#define scratch_update_cycle 10000U
unsigned long scratch_last_update = 0;

void loop() {
    // handle requests for web
    server.handleClient();

    readScratchMessageP2P();

    readCommand();
    if (command_end) {
        command_buffer.trim();
        if (command_buffer.startsWith("multicast:", 0)) {
            uint16_t message_size = command_buffer.length() - 10;
            char message_data[message_size];
            command_buffer.substring(10).toCharArray(message_data, message_size + 1);
            digitalWrite(LED, HIGH);
            sendScratchMessageMulticast(message_data, message_size);
            digitalWrite(LED, LOW);
        } else if (command_buffer.startsWith("send:", 0)) {
            uint32_t message_size = command_buffer.length() - 5;
            char message_data[message_size];
            command_buffer.substring(5).toCharArray(message_data, message_size + 1);
            digitalWrite(LED, HIGH);
            sendScratchMessageP2P(message_data, message_size);
            digitalWrite(LED, LOW);
        } else if (command_buffer.startsWith("module_id?", 0)) {
            COMMAND_PORT.printf("module_id=%s\n", WiFiConf.module_id);
        } else {
            COMMAND_PORT.printf("ERROR=%s\n", command_buffer.c_str());
        }
        command_buffer = "";
        command_end = false;
    }

    if (cycleCheck(&scratch_last_update, scratch_update_cycle)) {
        if (ScratchConf.multicast) {
            IPAddress ip = WiFi.localIP();
            String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
            char message[32] = {0};
            strcpy(message, "sensor-update \"");
            strcat(message, WiFiConf.module_id);
            strcat(message, "\" \"");
            strcat(message, ipStr.c_str());
            strcat(message, "\" ");
            digitalWrite(LED, HIGH);
            sendScratchMessageMulticast(message, strlen(message));
            digitalWrite(LED, LOW);
        }
    }
}
