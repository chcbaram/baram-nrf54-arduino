/*********************************************************************
 Read an SPI flash chip's JEDEC ID.

 One command, three bytes back: manufacturer, memory type, capacity.
 It needs no filesystem and no chip-specific driver, so it is a direct
 answer to "is this SPI bus actually working" - unlike a MOSI-to-MISO
 loopback, which passes even when the clock mode is wrong, because the
 board is both talking and listening.

 Tested on an NU54-DK with a flash soldered onto the expansion header.
 The stock board has nothing on SPI - P2 is brought out as plain header
 pins - so this wiring is a modification:

     signal   pin      header
     SCK      P2.01    19
     MOSI     P2.02    17
     MISO     P2.04    15
     CS       P2.03    16        <- set FLASH_CS below to match yours

 SPI lives on SPIM00, which can only drive P2. Moving it to P1 means
 SPIM20..22, and those share hardware with the UARTE and TWIM of the
 same number - see docs/PERIPHERAL-PINMAP.md.

 baram-nrf54l-arduino - MIT license
*********************************************************************/
#include <SPI.h>

/* ── Wiring ──────────────────────────────────────────────────────────
 *
 * Chip select belongs to the sketch, always - a bus can carry several
 * devices, so the library never touches it. Change this to wherever
 * yours is wired. SS is the variant's conventional pin.
 */
#define FLASH_CS    _PINNUM(2, 3)      // NU54-DK header pin 16

/*
 * SCK, MOSI and MISO come from the variant (PIN_SPI_*). Uncomment
 * these three to use different ones - setPins has to be called before
 * begin(), and the pins have to stay in the instance's domain: SPIM00
 * can only reach P2, SPIM20..22 only P1.
 */
// #define FLASH_SCK   _PINNUM(2, 1)
// #define FLASH_MOSI  _PINNUM(2, 2)
// #define FLASH_MISO  _PINNUM(2, 4)

void setup()
{
  Serial.begin(115200);
  delay(300);
  Serial.println("SPI flash JEDEC ID");

  pinMode(FLASH_CS, OUTPUT);
  digitalWrite(FLASH_CS, HIGH);        // idle high - the chip ignores us

#if defined(FLASH_SCK)
  SPI.setPins(FLASH_SCK, FLASH_MOSI, FLASH_MISO);
#endif
  SPI.begin();

  Serial.printf("SCK=P%u.%02u MOSI=P%u.%02u MISO=P%u.%02u CS=P%u.%02u\n",
                PIN_SPI_SCK  >> 5, PIN_SPI_SCK  & 31,
                PIN_SPI_MOSI >> 5, PIN_SPI_MOSI & 31,
                PIN_SPI_MISO >> 5, PIN_SPI_MISO & 31,
                FLASH_CS     >> 5, FLASH_CS     & 31);
}

void loop()
{
  uint8_t id[3] = { 0, 0, 0 };

  /* Mode 0 at 4 MHz suits every flash chip worth the name, and slow
   * enough that header wiring is not the variable under test. */
  SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  digitalWrite(FLASH_CS, LOW);

  SPI.transfer(0x9F);                  // RDID
  id[0] = SPI.transfer(0x00);          // manufacturer
  id[1] = SPI.transfer(0x00);          // memory type
  id[2] = SPI.transfer(0x00);          // capacity

  digitalWrite(FLASH_CS, HIGH);
  SPI.endTransaction();

  Serial.printf("JEDEC: %02X %02X %02X", id[0], id[1], id[2]);

  /* 0x00 and 0xFF are what an absent or unselected chip leaves on the
   * line, so they mean "nothing answered", not "answered zero". */
  if ( id[0] == 0x00 || id[0] == 0xFF )
  {
    Serial.println("   - nothing answered. Check CS, wiring and power.");
  }
  else
  {
    /* The capacity byte is a power of two: 0x15 is 2^21 bytes. */
    uint32_t bytes = (id[2] >= 0x10 && id[2] <= 0x1B) ? (1UL << id[2]) : 0;

    Serial.print("   ");
    switch (id[0])
    {
      case 0xEF: Serial.print("Winbond"); break;
      case 0xC2: Serial.print("Macronix"); break;
      case 0x20: Serial.print("Micron"); break;
      case 0x1F: Serial.print("Adesto"); break;
      case 0x9D: Serial.print("ISSI"); break;
      default:   Serial.print("unknown maker"); break;
    }
    if ( bytes ) Serial.printf(", %lu KB", (unsigned long) (bytes / 1024));
    Serial.println();
  }

  delay(2000);
}
