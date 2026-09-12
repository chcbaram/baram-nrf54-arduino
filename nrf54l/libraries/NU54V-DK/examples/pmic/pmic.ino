/*********************************************************************
 Read the NU54V-DK's battery charger over I2C.

 The board carries a TI BQ25186 single-cell linear charger with power
 path, and it sits on the same I2C bus as the Qwiic connector - so the
 charger and whatever you plug into Qwiic share SDA and SCL:

     SDA   P1.02      Wire, TWIM22
     SCL   P1.03
     BQ25186 at 0x6A,  Qwiic device at whatever address it uses

 ⚠ These two pins are the chip's NFC antenna pins, and they come out of
   reset as NFC pads - not GPIO. The variant turns that off in
   initVariant(); without it Wire finds nothing at all and there is no
   error to see. Keep that in mind if you port this to your own board.

 This example only reads. It does not change any charger setting -
 writing the wrong value here can stop the board charging, or worse.
 The register map is in TI's BQ25186 datasheet.

 baram-nrf54l-arduino - MIT license
*********************************************************************/
#include <Wire.h>

#define BQ25186_ADDR    0x6A

#define REG_STAT0       0x00     /* charge status, power good */
#define REG_STAT1       0x01     /* faults */
#define REG_VBAT_CTRL   0x03     /* battery regulation voltage */
#define REG_ICHG_CTRL   0x04     /* charge current */
#define REG_MASK_ID     0x0C     /* device ID */
#define REG_COUNT       0x0D

void setup()
{
  Serial.begin(115200);
  delay(300);
  Serial.println("NU54V-DK battery charger (BQ25186)");

#if !defined(ARDUINO_NU54VDK)
  Serial.println("This example is for the NU54V-DK - other boards have no PMIC.");
  Serial.println("Nothing below will find anything.");
#endif

  Wire.begin();

  int id = readRegister(REG_MASK_ID);
  if ( id < 0 ) {
    Serial.println("no answer at 0x6A.");
    Serial.println("On a board where P1.02/P1.03 are still NFC pads, every");
    Serial.println("address reads like this - see the note at the top.");
  } else {
    Serial.printf("MASK_ID 0x%02X\n", id);
  }
}

void loop()
{
  int stat0 = readRegister(REG_STAT0);

  if ( stat0 < 0 ) {
    Serial.println("read failed");
  } else {
    Serial.print("charge: ");
    switch ( stat0 & 0x60 ) {            /* CHG_STAT, bits 6:5 */
      case 0x00: Serial.print("idle (enabled, not charging)"); break;
      case 0x20: Serial.print("constant current");             break;
      case 0x40: Serial.print("constant voltage");             break;
      default:   Serial.print("done or disabled");             break;
    }

    /* Bit 0 says whether the input supply is usable at all, which is
     * the first thing to check when nothing seems to be charging. */
    Serial.print(stat0 & 0x01 ? "   VIN good" : "   VIN not good");

    if ( stat0 & 0x10 ) Serial.print("   [input current limited]");
    if ( stat0 & 0x02 ) Serial.print("   [thermal regulation]");
    Serial.println();

    float vbatreg = 3.5f + 0.01f * (readRegister(REG_VBAT_CTRL) & 0x7F);
    Serial.printf("  target %.2f V   STAT0 0x%02X  STAT1 0x%02X  ICHG 0x%02X\n",
                  vbatreg, stat0, readRegister(REG_STAT1),
                  readRegister(REG_ICHG_CTRL));
  }

  delay(2000);
}

/*───────────────────────────────────────────────────────────────────*/

/** One register. Returns -1 rather than a byte so a failure is visible. */
int readRegister(uint8_t reg)
{
  Wire.beginTransmission(BQ25186_ADDR);
  Wire.write(reg);
  /* false keeps the bus: a stop here would end the transaction before
   * the read, and the charger would answer from the wrong pointer. */
  if ( Wire.endTransmission(false) != 0 ) return -1;

  if ( Wire.requestFrom(BQ25186_ADDR, (size_t) 1) != 1 ) return -1;
  return Wire.read();
}
