
#ifndef __ESP_4_SCRATCH_H__
#define __ESP_4_SCRATCH_H__

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"  // for digitalRead, digitalWrite, etc
#else
#include "WProgram.h"
#endif

#ifndef COMMAND_PORT
  #ifdef  ARDUINO_AVR_LEONARDO
    #define COMMAND_PORT Serial1
  #elif  ARDUINO_AVR_MEGA2560
    // Serial 1: 19 (RX) 18 (TX);
    #define COMMAND_PORT Serial1
  #else
    #define COMMAND_PORT Serial // default port (UNO etc.)
  #endif // ARDUINO_AVR_LEONARDO
#endif // COMMAND_PORT

//#define DEBUG

#ifdef DEBUG
#include <SoftwareSerial.h>
SoftwareSerial debugPort(10, 11); // RX, TX
#define DEBUG_PORT debugPort
//#define DEBUG_PORT Serial
#define DEBUG_PRINT(x)  DEBUG_PORT.println (x)
#else
#define DEBUG_PRINT(x)
#endif

#define BAUD 9600

String command_buffer;         // a string to hold incoming data
String lastCommand;

int ledPin = 13;

void setupConnection(void) {
#ifdef DEBUG
  DEBUG_PORT.begin(BAUD);
  DEBUG_PRINT("DEBUG: Started!");
#endif
  command_buffer.reserve(256);
  lastCommand.reserve(256);
  COMMAND_PORT.begin(BAUD);
  delay(5000);  // wait for setup the communication module
  while (!COMMAND_PORT);
  pinMode(3, OUTPUT);
}


void readCommand(void) {
  while (COMMAND_PORT.available()) {
    char in_char = (char)COMMAND_PORT.read();
    if (in_char == '\n') {
      command_buffer += '\0';
      lastCommand = command_buffer;
      command_buffer = "";
    } else {
      command_buffer += in_char;
    }
  }
}

int  digitValue(float value) {
  return (max(0, min(1, value)));
}

int  pmwValue(float value) {
  return (max(0, min(255, value)));
}

void handleCommand(void) {
  readCommand();
  if (lastCommand.length() < 16) {
    return;
  }
  lastCommand.trim();
  if (lastCommand.startsWith("sensor-update ", 0)) {
    String data;
    data.reserve(256);
    data = lastCommand.substring(14);
    DEBUG_PRINT();
    DEBUG_PRINT(String("DEBUG:received:") + data);
    int readIndex = 0;
    while (readIndex < data.length()) {
        DEBUG_PRINT(String("DEBUG:readIndex= ") + readIndex + " / " + data.length());
      readIndex = data.indexOf('"', readIndex);
      int nameEnd = data.indexOf('"', readIndex + 1);
      while (data[nameEnd + 1] == '"') {
        int nextQuote = data.indexOf('"', nameEnd + 2);
        if (nextQuote != -1) {
          nameEnd = nextQuote;
        }
      }
      nameEnd += 1; // to include last quote
      String dataName = data.substring(readIndex, nameEnd);
      dataName.trim();
      DEBUG_PRINT(String("DEBUG:name[") + readIndex + "," + nameEnd + "]" + dataName);
      int valueEnd = nameEnd + 1;
      if (data[valueEnd] == '"') {
        // value is a String
        valueEnd = data.indexOf('"', nameEnd + 2);
        while (data[valueEnd + 1] == '"') {
          int nextQuote = data.indexOf('"', valueEnd + 2);
          if (nextQuote != -1) {
            valueEnd = nextQuote;
          }
        }
        valueEnd += 1; // count last quote
        DEBUG_PRINT(String("DEBUG:String[") + (nameEnd + 2) + "," + valueEnd + "]" + data.substring(nameEnd + 2, valueEnd - 1));
      } else {
        // value is a Number
        valueEnd = data.indexOf(' ', nameEnd + 2);
        if (valueEnd < 0) {
          valueEnd = data.length();
        }
        float dataValue = data.substring(nameEnd + 1, valueEnd).toFloat();
        DEBUG_PRINT(String("DEBUG:Number[") + nameEnd + 2 + "," + valueEnd + "]" + dataValue);
        if (dataName.equalsIgnoreCase(String("\"d13\""))) {
          digitalWrite(ledPin, digitValue(dataValue));
          DEBUG_PRINT(String("DEBUG:d13=") + digitValue(dataValue));
        }
        if (dataName.equalsIgnoreCase(String("\"pwm3\""))) {
          analogWrite(3, pmwValue(dataValue));
          DEBUG_PRINT(String("DEBUG:pwm3=") + pmwValue(dataValue));
        }
      }
      readIndex = valueEnd + 1;
    }
  } else {
    // ignore it
  }
  lastCommand = "";
}


#endif

