/*
 * board_test - LED, button and Serial check for a new board.
 * SPDX-License-Identifier: MIT
 *
 * LED polarity and button pull-ups differ per board; ledOn()/ledOff()
 * apply LED_STATE_ON so you do not have to care.
 */
#include <Arduino.h>

void setup()
{
  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("=== board test ===");
  /* BOARD_NAME comes from boards.txt, so this works for any board. */
  Serial.print("board        : "); Serial.println(BOARD_NAME);
  Serial.print("LED_STATE_ON : "); Serial.println(LED_STATE_ON);
  Serial.print("F_CPU        : "); Serial.println(F_CPU);

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(PIN_BUTTON1, INPUT_PULLUP);

  /* Blink three times to confirm polarity by eye. */
  for (int i = 0; i < 3; i++) {
    ledOn(LED_BUILTIN);  delay(150);
    ledOff(LED_BUILTIN); delay(150);
  }
}

void loop()
{
  static uint32_t last = 0;

  /* Pressed reads LOW. */
  if (digitalRead(PIN_BUTTON1) == LOW) {
    ledOn(LED_BUILTIN);
  } else {
    ledOff(LED_BUILTIN);
  }

  if (millis() - last >= 1000) {
    last = millis();
    Serial.print("millis="); Serial.print(millis());
    Serial.print(" button1="); Serial.println(digitalRead(PIN_BUTTON1));
  }
}
