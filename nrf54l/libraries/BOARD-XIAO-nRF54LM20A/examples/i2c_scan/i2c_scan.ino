/*********************************************************************
 Find what is on the XIAO nRF54LM20A's I2C buses.

 Use this one on this board rather than Wire's generic i2c_scanner.
 Here the Sense IMU and the pull-up resistors on its bus are powered
 by LDO1 of the nPM1300 PMIC, not by a GPIO. A scan that does not
 switch that rail on finds nothing on Wire1 even on a Sense board, and
 looks like a broken bus. This sketch turns the rail on first.

   Wire    TWIM22   SDA P1.03 (D4)  SCL P1.07 (D5)   header pins
   Wire1   TWIM30   SDA P0.08       SCL P0.07        Sense IMU at 0x6A
   PMIC    TWIM24   SDA P1.18       SCL P1.17        nPM1300 at 0x6B

 The PMIC bus belongs to the board library, so it is not scanned
 address by address - PMIC.begin() answering is the check for it.

 On a board without "Sense" Wire1 finds nothing, and that is expected.

 baram-nrf54l-arduino - MIT license
*********************************************************************/
#include <Wire.h>
#include <XIAO_nRF54LM20A.h>

void setup()
{
  Serial.begin(115200);
  delay(300);
  Serial.println("XIAO nRF54LM20A - I2C scan");

  bool pmic = PMIC.begin();
  Serial.printf("PMIC : 0x%02X %s\n", PMIC_I2C_ADDRESS, pmic ? "answers" : "does NOT answer");

  if ( pmic && PMIC.setSensorPower(true) ) {
    delay(50);                         /* rail up, IMU boot */
    Serial.println("sensor rail (LDO1) on");
  } else {
    Serial.println("could not switch the sensor rail on - Wire1 will be empty");
  }

  Wire.begin();
  Wire1.begin();
  Serial.println();
}

void scan(TwoWire &bus, const char *name)
{
  int found = 0;

  Serial.printf("%s:", name);
  for ( uint8_t addr = 1; addr < 0x78; addr++ ) {
    bus.beginTransmission(addr);
    if ( bus.endTransmission() == 0 ) { Serial.printf(" 0x%02X", addr); found++; }
  }
  if ( !found ) Serial.print(" nothing");
  Serial.println();
}

/* Write the register number, then read with a repeated start. false on
 * endTransmission keeps the bus, so the IMU still knows what was asked. */
int readRegister(TwoWire &bus, uint8_t addr, uint8_t reg)
{
  bus.beginTransmission(addr);
  bus.write(reg);
  if ( bus.endTransmission(false) != 0 ) return -1;

  if ( bus.requestFrom(addr, (size_t) 1) != 1 ) return -1;
  return bus.read();
}

void loop()
{
  scan(Wire,  "Wire ");
  scan(Wire1, "Wire1");

  /* An address answering only proves an ACK. Reading a known register
   * back proves data really moves on the bus. */
  int who = readRegister(Wire1, IMU_I2C_ADDRESS, 0x0F);   /* WHO_AM_I */
  if ( who < 0 )         Serial.println("       IMU: no answer (no Sense model, or rail off)");
  else if ( who == 0x6A) Serial.println("       IMU: WHO_AM_I 0x6A (LSM6DS3TR-C)");
  else                   Serial.printf ("       IMU: WHO_AM_I 0x%02X (unexpected)\n", who);

  Serial.println();
  delay(3000);
}
