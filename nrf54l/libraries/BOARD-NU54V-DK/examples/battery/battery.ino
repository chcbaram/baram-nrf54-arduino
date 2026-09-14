/*********************************************************************
 Battery voltage and charge state on the NU54V-DK.

 Two independent sources, shown side by side because they answer
 different questions:

   - the ADC on VBAT_MON tells you the voltage
   - the BQ25186 over I2C tells you what the charger is doing about it

     VBAT_MON   P1.12 = A5    ADC,  divider 470K / 1M
     BQ25186    0x6A          on Wire, the same bus as Qwiic

 ⚠ The divider is 320 kohm out (470K parallel 1M), which is high for
   the SAADC's default 10 us acquisition time - single reads come back
   low and wander by tens of millivolts. This averages instead. The high
   value is deliberate: it costs only ~2.8 uA at 4.1 V, which is what
   you want on a coin-sized battery.

 ⚠ P1.02/P1.03 leave reset as NFC pads rather than GPIO, so Wire finds
   nothing until that is turned off. initVariant() does it for this
   board - see docs/boards/NU54V-DK.md.

 baram-nrf54l-arduino - MIT license
*********************************************************************/

#include <Wire.h>

#define BQ25186_ADDR    0x6A
#define REG_STAT0       0x00

#define SAMPLES         32       /* see the note about acquisition time */

void setup()
{
  Serial.begin(115200);
  delay(300);
  Serial.println("NU54V-DK battery");

  Wire.begin();
}

void loop()
{
  /* ── voltage ────────────────────────────────────────────────────── */
  uint32_t sum = 0;
  for ( int i = 0; i < SAMPLES; i++ ) sum += analogReadMillivolts(PIN_VBAT);
  float vbat = (sum / (float) SAMPLES) * VBAT_DIVIDER / 1000.0f;

  Serial.print(vbat, 3);
  Serial.print(" V");

  /* A rough state of charge. Li-ion voltage is a poor fuel gauge under
   * load, so this is an indication and not a measurement. */
  int pct = (int) ((vbat - 3.30f) / (4.20f - 3.30f) * 100.0f);
  if ( pct < 0 )   pct = 0;
  if ( pct > 100 ) pct = 100;
  Serial.printf("  ~%d%%", pct);

  /* ── what the charger says ──────────────────────────────────────── */
  int stat0 = readRegister(REG_STAT0);

  if ( stat0 < 0 ) {
    Serial.println("   (charger not answering)");
  } else {
    switch ( stat0 & 0x60 ) {            /* CHG_STAT, bits 6:5 */
      case 0x00: Serial.print("   idle");              break;
      case 0x20: Serial.print("   charging (CC)");     break;
      case 0x40: Serial.print("   charging (CV)");     break;
      default:   Serial.print("   full or disabled");  break;
    }
    /* Without a USB or VEXT supply there is nothing to charge from, and
     * that is the first thing to check when the state looks wrong. */
    Serial.println(stat0 & 0x01 ? "   [VIN good]" : "   [no input power]");
  }

  delay(2000);
}

/*───────────────────────────────────────────────────────────────────*/

/** One charger register. Returns -1 so a failed read is visible. */
int readRegister(uint8_t reg)
{
  Wire.beginTransmission(BQ25186_ADDR);
  Wire.write(reg);
  /* false keeps the bus - a stop here would end the transaction before
   * the read, and the charger would answer from the wrong pointer. */
  if ( Wire.endTransmission(false) != 0 ) return -1;

  if ( Wire.requestFrom(BQ25186_ADDR, (size_t) 1) != 1 ) return -1;
  return Wire.read();
}
