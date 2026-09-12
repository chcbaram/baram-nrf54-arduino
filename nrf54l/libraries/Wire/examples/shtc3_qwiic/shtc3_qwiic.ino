/*********************************************************************
 Read a Sensirion SHTC3 temperature / humidity sensor over I2C.

 No sensor library is needed. The SHTC3 protocol is short enough to
 write out, and that is the point of the example - you can see every
 Wire call that goes into one reading, including the CRC the sensor
 sends and most drivers quietly discard.

 Tested on an NU54V-DK with an Adafruit SHTC3 board plugged into the
 on-board Qwiic connector (J5). Nothing else is wired:

     J5 pin   signal   GPIO
     1        GND
     2        3V3
     3        SDA      P1.02      <- Wire, TWIM22
     4        SCL      P1.03

 The board already has 2.1K pull-ups (R29/R30), so do not add your own.

 On a board without a Qwiic connector, wire SDA/SCL to whatever the
 variant defines as Wire and give the sensor 3V3 and GND. See
 docs/PERIPHERAL-PINMAP.md - on nRF54L, I2C reaches P1 and P0 but never
 P2, so those are the pins to pick from.

 baram-nrf54l-arduino - MIT license
*********************************************************************/
#include <Wire.h>

#define SHTC3_ADDR      0x70

/* 16-bit commands, MSB first. */
#define CMD_WAKEUP      0x3517
#define CMD_SLEEP       0xB098
#define CMD_READ_ID     0xEFC8
#define CMD_MEASURE     0x7CA2   /* normal mode, clock stretching, T first */

void setup()
{
  Serial.begin(115200);
  delay(300);
  Serial.println("SHTC3 over Qwiic");

  Wire.begin();

  /*
   * Read the ID first. It separates "the sensor is not answering" from
   * "the sensor answered nonsense", which are different problems.
   * The datasheet fixes bits 11:6 and 2:0, so mask before comparing.
   */
  sendCommand(CMD_WAKEUP);
  delayMicroseconds(500);

  uint16_t id = 0;
  bool ok = sendCommand(CMD_READ_ID);
  /* The sensor needs a moment to have the answer ready. Reading straight
   * after the command comes back with nothing - that cost a debugging
   * session here, and the datasheet does not put a number on it. */
  delayMicroseconds(500);

  if (!ok || !readWord(&id)) {
    Serial.println("no answer at 0x70 - check the cable, and that the");
    Serial.println("Qwiic pins match this board's Wire (see the comment above)");
  } else {
    Serial.printf("ID 0x%04X  %s\n", id,
                  (id & 0x083F) == 0x0807 ? "(SHTC3)" : "(unexpected - not an SHTC3?)");
  }
  sendCommand(CMD_SLEEP);
}

void loop()
{
  float t, rh;

  if (readSensor(&t, &rh)) {
    Serial.print(t, 2);  Serial.print(" C   ");
    Serial.print(rh, 2); Serial.println(" %RH");
  } else {
    /* Either nothing answered or the CRC did not match. */
    Serial.println("read failed");
  }

  delay(1000);
}

/*───────────────────────────────────────────────────────────────────*/

bool sendCommand(uint16_t cmd)
{
  Wire.beginTransmission(SHTC3_ADDR);
  Wire.write((uint8_t) (cmd >> 8));
  Wire.write((uint8_t) (cmd & 0xFF));
  return Wire.endTransmission() == 0;
}

/*
 * CRC-8, polynomial 0x31, initial value 0xFF, no final XOR.
 *
 * Worth doing rather than skipping: a floating SDA or a missing pull-up
 * often reads back as plausible-looking data, and the CRC is what tells
 * the difference between "cold room" and "no sensor".
 */
uint8_t crc8(const uint8_t *data, uint8_t len)
{
  uint8_t crc = 0xFF;
  for (uint8_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t b = 0; b < 8; b++) {
      crc = (crc & 0x80) ? (uint8_t) ((crc << 1) ^ 0x31) : (uint8_t) (crc << 1);
    }
  }
  return crc;
}

/** Reads one word plus its CRC byte. Returns false if the CRC fails. */
bool readWord(uint16_t *out)
{
  uint8_t buf[3];
  if (Wire.requestFrom(SHTC3_ADDR, (uint8_t) 3) != 3) return false;
  for (uint8_t i = 0; i < 3; i++) buf[i] = Wire.read();
  if (crc8(buf, 2) != buf[2]) return false;
  *out = (uint16_t) ((buf[0] << 8) | buf[1]);
  return true;
}

bool readSensor(float *temperature_c, float *humidity_pct)
{
  if (!sendCommand(CMD_WAKEUP)) return false;
  delayMicroseconds(500);              /* wake-up: datasheet says 240 us max, but
                                        * 240 was marginal here - measured */

  if (!sendCommand(CMD_MEASURE)) return false;
  delay(13);                           /* normal mode takes up to 12.1 ms */

  /* Temperature comes first, then humidity, each followed by its CRC. */
  uint8_t buf[6];
  if (Wire.requestFrom(SHTC3_ADDR, (uint8_t) 6) != 6) return false;
  for (uint8_t i = 0; i < 6; i++) buf[i] = Wire.read();

  if (crc8(&buf[0], 2) != buf[2]) return false;
  if (crc8(&buf[3], 2) != buf[5]) return false;

  uint16_t raw_t  = (uint16_t) ((buf[0] << 8) | buf[1]);
  uint16_t raw_rh = (uint16_t) ((buf[3] << 8) | buf[4]);

  *temperature_c = -45.0f + 175.0f * (float) raw_t  / 65536.0f;
  *humidity_pct  =         100.0f * (float) raw_rh / 65536.0f;

  sendCommand(CMD_SLEEP);              /* ~1 uA idle rather than ~45 uA */
  return true;
}
