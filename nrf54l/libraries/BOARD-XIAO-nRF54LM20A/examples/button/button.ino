/*********************************************************************
 Count presses of the XIAO nRF54LM20A's user button with an interrupt.

 USR_KEY is P0.09 with a 100K pull-up on the board, so it reads HIGH
 and falls to LOW when pressed. P0 belongs to GPIOTE30, which is what
 makes attachInterrupt possible here.

 The interrupt only raises a flag. Serial and debouncing happen in
 loop(), because an interrupt handler is the wrong place for either.
 Each press toggles the green LED.

 baram-nrf54l-arduino - MIT license
*********************************************************************/

volatile uint32_t edges = 0;

void onPress(void)
{
  edges++;
}

void setup()
{
  Serial.begin(115200);
  delay(300);
  Serial.println("XIAO nRF54LM20A - button interrupt");

  pinMode(PIN_BUTTON1, INPUT_PULLUP);
  attachInterrupt(PIN_BUTTON1, onPress, FALLING);
}

void loop()
{
  static uint32_t seen    = 0;
  static uint32_t presses = 0;
  static uint32_t lastMs  = 0;

  uint32_t now = edges;
  if ( now != seen ) {
    seen = now;

    /* A mechanical switch bounces for a few ms. Count one press per
     * burst of edges, and subtract rather than add so millis() wrapping
     * around does not stall it. */
    if ( (millis() - lastMs) >= 50 ) {
      presses++;
      digitalToggle(LED_GREEN);
      Serial.printf("press %lu   (edges so far %lu)\n",
                    (unsigned long) presses, (unsigned long) now);
    }
    lastMs = millis();
  }

  delay(5);
}
