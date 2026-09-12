/*********************************************************************
 Find what is on the I2C bus.

 Walks every 7-bit address and prints the ones that answer. On a board
 with a second bus - the XIAO has its IMU on one - both are scanned,
 and the IMU is read back to prove the bus really works rather than
 just acknowledging an address.

 Which pins and which TWIM instance the bus uses is set by the variant.
 The instance is not a free choice: TWIM30 shares hardware with UARTE30,
 so a board whose Serial is UARTE30 cannot put Wire there, and there is
 no TWIM00 at all, so P2 pins cannot carry I2C. See
 docs/PERIPHERAL-PINMAP.md.

 baram-nrf54l-arduino - MIT license
*********************************************************************/
#include <Wire.h>

void setup()
{
  Serial.begin(115200);
  delay(300);
  Serial.println("I2C scanner");

#ifdef PIN_SENSOR_POWER
  /* The XIAO's onboard sensors sit behind a switch. Without this the
   * second bus scans clean and looks broken. */
  pinMode(PIN_SENSOR_POWER, OUTPUT);
  digitalWrite(PIN_SENSOR_POWER, HIGH);
  delay(50);
#endif

  Wire.begin();
#ifdef WIRE1_TWIM_INSTANCE
  Wire1.begin();
#endif
}

void scan(TwoWire &bus, const char *name)
{
  int found = 0;

  Serial.printf("%s:", name);
  for (uint8_t addr = 1; addr < 0x78; addr++)
  {
    bus.beginTransmission(addr);
    if ( bus.endTransmission() == 0 ) { Serial.printf(" 0x%02X", addr); found++; }
  }
  if ( !found ) Serial.print(" nothing");
  Serial.println();
}

/* Read one register, the usual write-then-read with a repeated start.
 * The false on endTransmission is what keeps the bus - a stop here and
 * many devices forget which register was asked for. */
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
  scan(Wire, "Wire ");

#ifdef WIRE1_TWIM_INSTANCE
  scan(Wire1, "Wire1");

#ifdef IMU_I2C_ADDRESS
  int who = readRegister(Wire1, IMU_I2C_ADDRESS, 0x0F);    // WHO_AM_I
  Serial.printf("       IMU WHO_AM_I = 0x%02X %s\n", who,
                (who == 0x6A) ? "(LSM6DS3TR-C)" : "(unexpected)");
#endif
#endif

  Serial.println();
  delay(3000);
}
