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
  if ( !sd.begin(SD_CS, SD_SCK_MHZ(4)) )
  {
    Serial.println("begin failed - card inserted? CS right? formatted?");
    return;
  }
  Serial.println("mounted");

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
  delay(1000);
}
