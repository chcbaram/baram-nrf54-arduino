/*********************************************************************
 Read the XIAO nRF54LM20A's power management IC.

 The board runs from a Nordic nPM1300, and on this board it is more
 than a charger - it makes the rails:

     VSYS_3V3     the nRF54LM20A itself, LEDs    BUCK2
     IMU&MIC_3V3  Sense IMU and microphone       LDO1

 It talks on its own I2C bus (P1.17 SCL / P1.18 SDA, TWIM24), separate
 from Wire and Wire1. The library opens that bus - this sketch does not
 need Wire.

 This example only reads. It prints whether USB power is present, what
 the charger is doing, the battery voltage and the PMIC's die
 temperature. Without a battery connected the voltage is meaningless.

 baram-nrf54l-arduino - MIT license
*********************************************************************/
#include <XIAO_nRF54LM20A.h>

void setup()
{
  Serial.begin(115200);
  delay(300);
  Serial.println("XIAO nRF54LM20A - nPM1300");

  if ( !PMIC.begin() ) {
    Serial.println("no answer from the PMIC at 0x6B.");
    Serial.println("This is the board's own power chip, so a silent bus");
    Serial.println("means the TWIM24 pins or the build board are wrong.");
  }
  Serial.println();
}

void loop()
{
  int stat = PMIC.chargeStatus();
  if ( stat < 0 ) { Serial.println("read failed"); delay(2000); return; }

  Serial.printf("usb      %s\n", PMIC.vbusPresent() ? "present" : "not present");

  Serial.print("charger  ");
  if      ( stat & NPM1300::CHG_COMPLETE ) Serial.print("complete");
  else if ( stat & NPM1300::CHG_TRICKLE )  Serial.print("trickle charging");
  else if ( stat & NPM1300::CHG_CC )       Serial.print("charging - constant current");
  else if ( stat & NPM1300::CHG_CV )       Serial.print("charging - constant voltage");
  else                                     Serial.print("not charging");

  int err = PMIC.chargeError();
  if ( err > 0 ) Serial.printf("   [error reason 0x%02X]", err);
  Serial.println();

  Serial.printf("battery  %ld mV\n", (long) PMIC.batteryMillivolts());
  Serial.printf("die      %.1f C\n", PMIC.dieTemperature());
  Serial.printf("sensors  %s\n", PMIC.sensorPowerOn() ? "powered (LDO1 on)" : "off (LDO1 off)");
  Serial.printf("raw      CHG_STAT %02X\n\n", stat);

  delay(2000);
}
