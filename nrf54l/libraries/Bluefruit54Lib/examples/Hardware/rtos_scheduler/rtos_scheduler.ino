/*
 * rtos_scheduler - a second task via Scheduler.startLoop().
 * Same API as rtos.h in the Adafruit nRF52 core. SPDX-License-Identifier: MIT
 *
 * millis and micros are printed together because tickless drift only shows
 * in the difference. Healthy: 2000 and 2000000.
 */
#include <Arduino.h>

void loop2()
{
  digitalToggle(LED_CONN);
  delay(500);
}

void setup()
{
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_CONN, OUTPUT);

  Scheduler.startLoop(loop2);
}

void loop()
{
  digitalToggle(LED_BUILTIN);
  delay(1000);
  digitalToggle(LED_BUILTIN);
  delay(1000);

  Serial.print("millis="); Serial.print(millis());
  Serial.print(" micros="); Serial.println(micros());
}
