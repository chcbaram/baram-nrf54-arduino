/*********************************************************************
 Echo everything received on the serial port back out.

 Type in the serial monitor and the characters come back. Useful for
 checking that serial receive works, which is easy to get wrong.

 baram-nrf54l-arduino - MIT license
*********************************************************************/

void setup()
{
  Serial.begin(115200);
  Serial.println("Type something.");
}

void loop()
{
  /* Bytes arrive one at a time, so short input shows up immediately
   * rather than waiting for a buffer to fill. */
  while ( Serial.available() )
  {
    Serial.write(Serial.read());
  }
}
