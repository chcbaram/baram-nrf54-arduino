/*********************************************************************
 Fade the XIAO nRF54LM20A's RGB LED.

 All three colours sit on P1 - red P1.22, blue P1.23, green P1.24 - and
 P1 belongs to PWM20..22, so analogWrite works on every one of them.
 (The XIAO nRF54L15's only LED is on P2.00, where there is no PWM.)

 The LED is common anode: a pin driven LOW turns its colour on. The
 core does not flip PWM polarity for you, so full brightness is a
 duty of 0 and off is 255.

 baram-nrf54l-arduino - MIT license
*********************************************************************/

const uint32_t leds[] = { LED_RED, LED_GREEN, LED_BLUE };
const char    *names[] = { "red", "green", "blue" };

void setup()
{
  Serial.begin(115200);
  delay(300);
  Serial.println("XIAO nRF54LM20A - RGB fade");

  /* analogWriteOk says whether the pin can do PWM at all - on a P2 pin
   * it returns false instead of silently doing nothing. */
  for ( int i = 0; i < 3; i++ ) {
    Serial.printf("%-5s P1.%02lu  PWM %s\n", names[i], (unsigned long) (leds[i] & 31),
                  analogWriteOk(leds[i], 255) ? "ok" : "NOT available");
  }
}

void loop()
{
  for ( int i = 0; i < 3; i++ ) {
    for ( int b = 0; b <= 255; b += 5 ) {        /* up */
      analogWrite(leds[i], 255 - b);             /* active LOW */
      delay(8);
    }
    for ( int b = 255; b >= 0; b -= 5 ) {        /* down */
      analogWrite(leds[i], 255 - b);
      delay(8);
    }
  }
}
