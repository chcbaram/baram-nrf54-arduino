/*********************************************************************
 List and erase stored bonds.

 Bonds sit in a 4 KB RRAM partition past the application, so reflashing
 a sketch does not clear them. This is how you clear them on purpose,
 and how you see what is stored.

 Send 'c' over serial to erase everything.

 baram-nrf54l-arduino - MIT license
*********************************************************************/
#include <bluefruit.h>

void setup()
{
  Serial.begin(115200);
  Serial.println("Bond storage");

  /* Bluefruit.begin() brings up the store; nothing is advertised here. */
  Bluefruit.begin();

  dump();
  Serial.println("Send 'c' to erase all bonds.");
}

void dump(void)
{
  Serial.printf("\n%u of %u slots used\n", bondCount(BLE_GAP_ROLE_INVALID), BOND_MAX_COUNT);
  bondPrintList(BLE_GAP_ROLE_INVALID);
}

void loop()
{
  while ( Serial.available() )
  {
    char c = Serial.read();

    if ( c == 'c' || c == 'C' )
    {
      bondClearAll();
      Serial.println("\nErased.");
      dump();

      /* The other side still holds its key. It will usually fail to
       * encrypt and drop the link rather than pair again, so remove this
       * device from the host's Bluetooth settings too. */
      Serial.println("Remove this device on the host as well, or it will "
                     "try to reuse a key we no longer have.");
    }
  }
  delay(50);
}
