/*********************************************************************
 Talk to the XIAO nRF54LM20A's on-board 8 MB flash (PY25Q64).

 The chip is wired for quad SPI, but plain SPI reaches it on the same
 pins - IO0 is MOSI and IO1 is MISO. The variant puts it on SPI1
 (SPIM00, the only SPI that can drive P2):

     SCK   P2.01    MOSI  P2.02    MISO  P2.04
     CS    P2.05    WP    P2.03    HOLD  P2.00

 WP and HOLD are active low. Both are driven HIGH here so a floating
 line can neither block writes nor pause the chip mid-transfer.

 Two reads, neither of which writes anything:
   JEDEC ID  (0x9F)  manufacturer, type, capacity
   SFDP      (0x5A)  the header must start with the letters "SFDP",
                     which proves data is really coming back rather
                     than a stuck line reading as some value

 baram-nrf54l-arduino - MIT license
*********************************************************************/
#include <SPI.h>

void select(bool on) { digitalWrite(PIN_FLASH_CS, on ? LOW : HIGH); }

void setup()
{
  Serial.begin(115200);
  delay(300);
  Serial.println("XIAO nRF54LM20A - on-board flash");

  pinMode(PIN_FLASH_CS,   OUTPUT);  select(false);
  pinMode(PIN_FLASH_WP,   OUTPUT);  digitalWrite(PIN_FLASH_WP,   HIGH);
  pinMode(PIN_FLASH_HOLD, OUTPUT);  digitalWrite(PIN_FLASH_HOLD, HIGH);

  SPI1.begin();
}

void loop()
{
  uint8_t id[3];
  uint8_t sfdp[8];

  SPI1.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));

  select(true);
  SPI1.transfer(0x9F);
  for ( int i = 0; i < 3; i++ ) id[i] = SPI1.transfer(0x00);
  select(false);

  select(true);
  SPI1.transfer(0x5A);                       /* READ SFDP */
  SPI1.transfer(0x00); SPI1.transfer(0x00); SPI1.transfer(0x00);   /* address 0 */
  SPI1.transfer(0x00);                       /* one dummy byte */
  for ( int i = 0; i < 8; i++ ) sfdp[i] = SPI1.transfer(0x00);
  select(false);

  SPI1.endTransaction();

  Serial.printf("JEDEC %02X %02X %02X", id[0], id[1], id[2]);
  if ( id[0] == 0x00 || id[0] == 0xFF ) {
    Serial.println("   - nothing answered");
  } else {
    /* The capacity byte is a power of two: 0x17 is 2^23 bytes = 8 MB. */
    if ( id[2] >= 0x10 && id[2] <= 0x1B )
      Serial.printf("   %lu KB", (unsigned long) ((1UL << id[2]) / 1024));
    Serial.println();
  }

  bool ok = (sfdp[0] == 'S' && sfdp[1] == 'F' && sfdp[2] == 'D' && sfdp[3] == 'P');
  Serial.printf("SFDP  %c%c%c%c  rev %u.%u  %s\n\n",
                isPrintable(sfdp[0]) ? sfdp[0] : '.', isPrintable(sfdp[1]) ? sfdp[1] : '.',
                isPrintable(sfdp[2]) ? sfdp[2] : '.', isPrintable(sfdp[3]) ? sfdp[3] : '.',
                sfdp[5], sfdp[4], ok ? "ok" : "- signature missing");

  delay(2000);
}
