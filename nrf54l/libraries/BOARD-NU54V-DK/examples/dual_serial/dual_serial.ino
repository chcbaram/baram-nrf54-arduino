/*********************************************************************
 The NU54V-DK shows up as two USB serial ports. This is why.

 The on-board debugger carries two UARTs from the MCU, so both appear
 on the host at once:

     first port    Serial    P0.00 TX / P0.01 RX    UARTE30
     second port   Serial1   P1.04 TX / P1.05 RX    UARTE20

 Open both in two terminals: each shows only its own traffic, and typing
 into one echoes back through the other. That is the whole point -
 you can keep a log running on one port while driving a protocol on the
 other, without a second cable or a USB-serial dongle.

 ⚠ Serial1 costs you A0 and A1. P1.04 and P1.05 are AIN0 and AIN1, and
   the solder bridges SB9..SB12 wire them to the debugger. To use
   analogRead on A0..A3 you have to remove those bridges, which removes
   the second port. Both are brought out on the headers - see
   docs/boards/NU54V-DK.md.

 baram-nrf54l-arduino - MIT license
*********************************************************************/


void setup()
{
  Serial.begin(115200);
  Serial1.begin(115200);
  delay(300);

  Serial.println ("port 1 - Serial  (P0.00/P0.01). Type here; it comes out on port 2.");
  Serial1.println("port 2 - Serial1 (P1.04/P1.05). Type here; it comes out on port 1.");
}

void loop()
{
  /* Cross the two ports so one terminal proves the other is alive. */
  while ( Serial.available()  ) Serial1.write(Serial.read());
  while ( Serial1.available() ) Serial.write(Serial1.read());

  /* A slow heartbeat on each, so an idle terminal still shows which is
   * which without anyone typing. */
  static uint32_t last = 0;
  static uint32_t n    = 0;

  if ( millis() - last >= 2000 ) {
    last = millis();
    Serial.printf ("[port 1] %lu\n", (unsigned long) n);
    Serial1.printf("[port 2] %lu\n", (unsigned long) n);
    n++;
  }
}
