#include <Arduino.h>

#include "esp4scratch.h"


void setup() {
  pinMode(2, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);
  setupConnection();
  // indicate end of the setup
  for (int i = 0; i < 10; i++) {
    digitalWrite(ledPin, HIGH);
    delay(100);
    analogWrite(ledPin, LOW);
    delay(100);
  }
}

#define scratch_update_cycle 200U
unsigned long scratch_last_update = 0;

void loop() {
  handleCommand();
  if (cycleCheck(&scratch_last_update, scratch_update_cycle)) {
    char message[256] = {0};
    sprintf(message,
            "send:sensor-update \"A0\" %d \"A1\" %d \"A2\" %d \"A3\" %d \"A4\" %d \"A5\" %d \"D2\" %d ",
            analogRead(A0), analogRead(A1), analogRead(A2), analogRead(A3), analogRead(A4), analogRead(A5), digitalRead(2));
    COMMAND_PORT.println(message);
//    DEBUG_PRINT(String("DEBUG:") + message);
  }
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

