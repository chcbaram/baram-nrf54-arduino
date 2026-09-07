/*********************************************************************
 Blink the on-board LED.

 The first sketch to try on a new board. If this runs, the toolchain,
 the upload path and the variant pin map are all working.

 baram-nrf54l-arduino - MIT license
*********************************************************************/

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop()
{
  /* ledOn/ledOff follow the board's LED polarity. Some boards drive the
   * LED low to light it, and digitalWrite(HIGH) would be backwards. */
  ledOn(LED_BUILTIN);
  delay(500);
  ledOff(LED_BUILTIN);
  delay(500);
}
