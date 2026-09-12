/*********************************************************************
 List the files on an SD card.

 Uses the standard SD library, which you install from Library Manager
 ("SD" by Arduino) - it is not bundled here. That is the point of the
 example: a stock Arduino library driving this core's SPI.

 It exercises far more of the bus than a JEDEC ID read does. Mounting a
 card means starting slow and switching fast, multi-byte commands, and
 512-byte sector reads, so clock mode, continuous transfers and long
 DMA blocks all have to be right before a directory listing appears.

 Tested on an NU54-DK with an SD socket soldered onto the expansion
 header. The stock board has nothing on SPI - P2 is brought out as
 plain header pins - so this wiring is a modification:

     signal   pin      header
     SCK      P2.01    19
     MOSI     P2.02    17
     MISO     P2.04    15
     CS       P2.05    14        <- set SD_CS below to match yours

 Card detect is on P2.00 and works: the socket's switch shorts it to
 ground while the slot is empty and opens once a card is in, so with an
 internal pull-up a card reads high. That matches the active-high the
 board's Zephyr definition declares.

 baram-nrf54l-arduino - MIT license
*********************************************************************/
#include <SPI.h>
#include <SD.h>

/* Chip select belongs to the sketch. Change it to wherever yours is. */
#define SD_CS    _PINNUM(2, 5)      // NU54-DK header pin 14

/*
 * SCK, MOSI and MISO come from the variant (PIN_SPI_*). Uncomment to
 * use different ones - setPins goes before any SPI use, and the pins
 * have to stay in the instance's domain: SPIM00 reaches P2 only.
 */
// #define SD_SCK    _PINNUM(2, 1)
// #define SD_MOSI   _PINNUM(2, 2)
// #define SD_MISO   _PINNUM(2, 4)

/*
 * Card detect. Comment out if your socket has no switch wired - with
 * the pull-up below an unconnected pin just reads "card present", so
 * leaving it on costs nothing either way.
 *
 * Polarity differs between sockets, and one reading will not tell you:
 * the switch is open with a card in, so the pin only follows the
 * internal pull and looks exactly like an unconnected one. Read it
 * with a card and again without. Here it is driven low when the slot
 * is empty and floats when full, so pull-up plus active high.
 */
#define SD_CD                _PINNUM(2, 0)   // NU54-DK header pin 20
#define SD_CD_ACTIVE_LEVEL   HIGH           // level the pin reads with a card in

void setup()
{
  Serial.begin(115200);
  delay(300);
  Serial.println("SD card listing");

#if defined(SD_SCK)
  SPI.setPins(SD_SCK, SD_MOSI, SD_MISO);
#endif

#if defined(SD_CD)
  pinMode(SD_CD, INPUT_PULLUP);
#endif
}

bool cardPresent(void)
{
#if defined(SD_CD)
  return digitalRead(SD_CD) == SD_CD_ACTIVE_LEVEL;
#else
  return true;                 // no switch to ask - assume it is there
#endif
}

void listFiles(void)
{
  File root = SD.open("/");
  if ( !root ) { Serial.println("cannot open root"); return; }

  while ( true )
  {
    File entry = root.openNextFile();
    if ( !entry ) break;

    /* Names come back in 8.3 form, so TEST2.WAV may have been something
     * longer on the machine that wrote it. The sdfat_card example shows
     * the real names. */
    Serial.printf("  %-14s %s%lu\n", entry.name(),
                  entry.isDirectory() ? "<dir> " : "",
                  (unsigned long) entry.size());
    entry.close();
  }
  root.close();
}

void loop()
{
  static bool mounted = false;
  bool present = cardPresent();

  if ( present && !mounted )
  {
    /* SD.begin() starts SPI itself, so there is no SPI.begin() here.
     * Give the card a moment - contacts bounce on the way in. */
    delay(100);

    if ( SD.begin(SD_CS) )
    {
      Serial.println("mounted");
      listFiles();
      mounted = true;
    }
    else
    {
      Serial.println("SD.begin failed - card seated? CS right? formatted FAT?");
      delay(1000);             // do not hammer a card that will not mount
    }
  }
  else if ( !present && mounted )
  {
    /* Without a card-detect line this branch never runs, and a card
     * pulled out mid-write is simply lost - that is what the switch
     * buys you. */
    Serial.println("card removed");
    SD.end();
    mounted = false;
  }

  delay(200);
}
