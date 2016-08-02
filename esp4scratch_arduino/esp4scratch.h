
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

#define DEBUG

#ifdef DEBUG
#define DEBUG_PORT Serial
#define DEBUG_PRINT(x)  DEBUG_PORT.println (x)
#else
#define DEBUG_PRINT(x)
#endif

#define BAUD 9600

String command_buffer;         // a string to hold incoming data
bool command_end = false;  // whether the string is complete

int ledPin = 13;

void setupConnection(void) {
#ifdef DEBUG
  DEBUG_PORT.begin(BAUD);
#endif
  command_buffer.reserve(256);
  COMMAND_PORT.begin(BAUD);
  delay(5000);  // wait for setup the communication module
  while (!COMMAND_PORT);
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

int  pmwValue(float value) {
  return (max(0, min(255, value)));
}

void handleCommand(void) {
  readCommand();
  if (!command_end) {
    return;
  }
  command_buffer.trim();
  if (command_buffer.startsWith("sensor-update ", 0)) {
    String data;
    data.reserve(256);
    data = command_buffer.substring(14);
    DEBUG_PRINT(String("DEBUG:") + String("received:") + command_buffer);
    int readIndex = 0;
    while (readIndex < data.length()) {
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
      DEBUG_PRINT(String("DEBUG:name") + String("[") + String(readIndex, DEC) + String(",") + String(nameEnd, DEC) + String("]") + dataName);
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
        DEBUG_PRINT(String("DEBUG:") + String("String") + String("[") + String(nameEnd + 2, DEC) + String(",") + String(valueEnd, DEC) + String("]") + data.substring(nameEnd + 2, valueEnd));
      } else {
        // value is a Number
        valueEnd = data.indexOf(' ', nameEnd + 2);
        if (valueEnd < 0) {
          valueEnd = data.length();
        }
        float dataValue = data.substring(nameEnd + 1, valueEnd).toFloat();
        DEBUG_PRINT(String("DEBUG:") + String("Number") + String("[") + String(nameEnd + 2, DEC) + String(",") + String(valueEnd, DEC) + String("]") + dataValue);
        if (dataName.equalsIgnoreCase(String("\"d13\""))) {
          analogWrite(ledPin, pmwValue(dataValue));
          DEBUG_PRINT(String("DEBUG:") + String("d13=") + pmwValue(dataValue));
        }
      }
      readIndex = valueEnd + 1;
    }
  } else {
    // ignore it
  }
  command_buffer = "";
  command_end = false;
}


#endif

