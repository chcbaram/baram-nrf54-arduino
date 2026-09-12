/*********************************************************************
 List an SD card with full filenames.

 The sd_card example uses the standard SD library, which is built on an
 old SdFat fork and only reports 8.3 short names - TEST2.WAV for a file
 written as test2.wav, SPOTLI~1 for .Spotlight-V100. This one uses
 SdFat (Bill Greiman, Library Manager: "SdFat"), which reads the long
 name entries and gives the real thing.

 Side by side on the same card:

     SD library        SdFat
     SPOTLI~1          .Spotlight-V100
     TEST2.WAV         test2.wav
     TRASHE~1          .Trashes

 What it costs: about 6 KB more flash, and SdFat's API is its own - not
 the Arduino File class. Worth it when names matter, and it also reads
 exFAT, which the SD library cannot.

 Tested on an NU54-DK with an SD socket soldered onto the expansion
 header. The stock board has nothing on SPI; this wiring is a
 modification:

     signal   pin      header
     SCK      P2.01    19
     MOSI     P2.02    17
     MISO     P2.04    15
     CS       P2.05    14        <- set SD_CS below to match yours
     CD       P2.00    20        card detect (see below)

 baram-nrf54l-arduino - MIT license
*********************************************************************/
#include <SPI.h>
#include <SdFat.h>

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

SdFat sd;

void setup()
{
  Serial.begin(115200);
  delay(300);
  Serial.println("SD card listing (long names)");

#if defined(SD_SCK)
  SPI.setPins(SD_SCK, SD_MOSI, SD_MISO);
#endif

  /* 4 MHz is plenty here and keeps header wiring out of the picture.
   * Raise it once the card is known to work. */
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
  FsFile root, entry;
  if ( !root.open("/") ) { Serial.println("cannot open root"); return; }

  char name[128];

  while ( entry.openNext(&root, O_RDONLY) )
  {
    entry.getName(name, sizeof(name));

    /* Directories report a size of zero here, unlike the SD library,
     * which hands back the cluster size. Zero is the honest answer. */
    Serial.printf("  %-40s %s%lu\n", name,
                  entry.isDir() ? "<dir> " : "",
                  (unsigned long) entry.fileSize());
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
    delay(100);                // contacts bounce on the way in

    /* 4 MHz is plenty here and keeps header wiring out of the picture.
     * Raise it once the card is known to work. */
    if ( sd.begin(SD_CS, SD_SCK_MHZ(4)) )
    {
      Serial.println("mounted");
      listFiles();
      mounted = true;
    }
    else
    {
      Serial.println("begin failed - card seated? CS right? formatted?");
      delay(1000);
    }
  }
  else if ( !present && mounted )
  {
    Serial.println("card removed");
    mounted = false;
  }

  delay(200);
}
