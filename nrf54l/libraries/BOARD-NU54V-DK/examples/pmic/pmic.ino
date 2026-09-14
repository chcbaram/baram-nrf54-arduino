/*********************************************************************
 Read the NU54V-DK's battery charger in detail.

 The board carries a TI BQ25186 single-cell linear charger with power
 path, and it sits on the same I2C bus as the Qwiic connector - so the
 charger and whatever you plug into Qwiic share SDA and SCL:

     SDA   P1.02      Wire, TWIM22
     SCL   P1.03
     BQ25186 at 0x6A,  Qwiic device at whatever address it uses

 The charger also brings four signals out to GPIO through solder
 bridges, all of them fitted on this board:

     PMIC_INT   P1.11 = A4   open drain, 10K pull-up
     PMIC_PG    P2.08        input power good
     PMIC_CE    P2.10        charge enable
     VBAT_MON   P1.12 = A5   battery voltage, divided 470K / 1M

 ⚠ P1.02 / P1.03 are the chip's NFC antenna pins and leave reset as NFC
   pads, not GPIO. initVariant() turns that off for this board; without
   it Wire finds nothing at all and there is no error to see.

 This example only reads. Writing the wrong value here can stop the
 board charging, or worse. Register details are in TI's datasheet - the
 decode below covers the fields you normally want.

 baram-nrf54l-arduino - MIT license
*********************************************************************/
#include <Wire.h>

#define BQ25186_ADDR    0x6A

#define REG_STAT0       0x00     /* charge state, power good          */
#define REG_STAT1       0x01     /* live faults                       */
#define REG_FLAG0       0x02     /* latched faults - cleared on read   */
#define REG_VBAT_CTRL   0x03     /* battery regulation voltage        */
#define REG_ICHG_CTRL   0x04     /* charge current, charge enable     */
#define REG_TMR_ILIM    0x08     /* input current limit               */
#define REG_MASK_ID     0x0C     /* device ID                         */

void setup()
{
  Serial.begin(115200);
  delay(300);
  Serial.println("NU54V-DK battery charger (BQ25186)");

  Wire.begin();

  int id = readRegister(REG_MASK_ID);
  if ( id < 0 ) {
    Serial.println("no answer at 0x6A.");
    Serial.println("On a board where P1.02/P1.03 are still NFC pads, every");
    Serial.println("address reads like this - see the note at the top.");
  } else {
    Serial.printf("MASK_ID 0x%02X\n", id);
  }
  Serial.println();
}

void loop()
{
  int stat0 = readRegister(REG_STAT0);
  if ( stat0 < 0 ) { Serial.println("read failed"); delay(2000); return; }

  int stat1 = readRegister(REG_STAT1);
  int flag0 = readRegister(REG_FLAG0);
  int vbatc = readRegister(REG_VBAT_CTRL);
  int ichgc = readRegister(REG_ICHG_CTRL);
  int ilimc = readRegister(REG_TMR_ILIM);

  /* ── what it is doing ───────────────────────────────────────────── */
  Serial.print("state    ");
  switch ( stat0 & 0x60 ) {                    /* CHG_STAT, bits 6:5 */
    case 0x00: Serial.print("idle (enabled, not charging)"); break;
    case 0x20: Serial.print("charging - constant current"); break;
    case 0x40: Serial.print("charging - constant voltage"); break;
    default:   Serial.print("charge done, or charging disabled"); break;
  }
  if ( ichgc >= 0 && (ichgc & 0x80) ) Serial.print("   [CHG_DISABLE set]");
  Serial.println();

  /* ── input ──────────────────────────────────────────────────────── */
  Serial.print("input    ");
  Serial.print(stat0 & 0x01 ? "VIN good" : "no usable input");
  if ( stat0 & 0x10 ) Serial.print("   at input current limit");
  if ( stat0 & 0x04 ) Serial.print("   VINDPM (input sagging)");
  if ( stat0 & 0x08 ) Serial.print("   VDPPM");
  if ( ilimc >= 0 )   Serial.printf("   limit %s", ilimText(ilimc & 0x07));
  Serial.println();

  /* ── settings ───────────────────────────────────────────────────── */
  if ( vbatc >= 0 && ichgc >= 0 ) {
    Serial.printf("setting  target %.2f V   charge %u mA\n",
                  3.5f + 0.01f * (vbatc & 0x7F), ichgCurrent(ichgc & 0x7F));
  }

  /* ── anything wrong ─────────────────────────────────────────────── */
  Serial.print("health   ");
  bool bad = false;
  if ( stat0 & 0x02 ) { Serial.print("[thermal regulation] "); bad = true; }
  if ( stat0 & 0x80 ) { Serial.print("[TS open] ");            bad = true; }

  if ( stat1 >= 0 ) {
    if ( stat1 & 0x80 ) { Serial.print("[VIN over-voltage] ");   bad = true; }
    if ( stat1 & 0x40 ) { Serial.print("[battery under-volt] "); bad = true; }
    if ( stat1 & 0x04 ) { Serial.print("[safety timer] ");       bad = true; }

    /* The thermistor field is two bits, and "normal" is the common case
     * worth not printing. A board with no thermistor reads TS open above. */
    switch ( stat1 & 0x18 ) {
      case 0x08: Serial.print("[battery too hot or cold] "); bad = true; break;
      case 0x10: Serial.print("[battery cool] ");            bad = true; break;
      case 0x18: Serial.print("[battery warm] ");            bad = true; break;
      default: break;
    }
  }
  Serial.println(bad ? "" : "ok");

  /* FLAG0 latches faults that have happened since the last read, so a
   * glitch that has already cleared still shows up exactly once here. */
  if ( flag0 > 0 ) Serial.printf("latched  FLAG0 0x%02X (since last read)\n", flag0);

  Serial.printf("raw      STAT0 %02X  STAT1 %02X  VBAT %02X  ICHG %02X  ILIM %02X\n\n",
                stat0, stat1, vbatc, ichgc, ilimc);

  delay(2000);
}

/*───────────────────────────────────────────────────────────────────*/

/** ICHG_CTRL bits 6:0 to milliamps. Two slopes, per the datasheet. */
uint16_t ichgCurrent(uint8_t code)
{
  return (code > 31) ? (uint16_t) (40 + (code - 31) * 10)
                     : (uint16_t) (code + 5);
}

const char *ilimText(uint8_t code)
{
  static const char *t[] = { "50 mA", "100 mA", "200 mA", "300 mA",
                             "400 mA", "500 mA", "700 mA", "1100 mA" };
  return t[code & 0x07];
}

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
