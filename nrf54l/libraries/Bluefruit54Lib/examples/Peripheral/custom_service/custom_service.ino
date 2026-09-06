/*
 * custom_service - your own GATT service with a 128-bit UUID.
 * Shows read, notify and write. SPDX-License-Identifier: MIT
 *
 * UUIDs are written in REVERSE byte order - the SoftDevice takes a
 * little-endian array.
 */
#include <bluefruit.h>

/* 6E400001-B5A3-F393-E0A9-E50E24DCCA9E, byte-reversed */
const uint8_t SVC_UUID[16] = {
  0x9E,0xCA,0xDC,0x24,0x0E,0xE5,0xA9,0xE0, 0x93,0xF3,0xA3,0xB5,0x01,0x00,0x40,0x6E
};
const uint8_t CHR_TX_UUID[16] = {   /* ...0003 : read + notify */
  0x9E,0xCA,0xDC,0x24,0x0E,0xE5,0xA9,0xE0, 0x93,0xF3,0xA3,0xB5,0x03,0x00,0x40,0x6E
};
const uint8_t CHR_RX_UUID[16] = {   /* ...0002 : write */
  0x9E,0xCA,0xDC,0x24,0x0E,0xE5,0xA9,0xE0, 0x93,0xF3,0xA3,0xB5,0x02,0x00,0x40,0x6E
};

BLEService        svc(SVC_UUID);
BLECharacteristic chrTx(CHR_TX_UUID);
BLECharacteristic chrRx(CHR_RX_UUID);

void onWrite(uint16_t conn, BLECharacteristic *chr, uint8_t *data, uint16_t len)
{
  (void) conn; (void) chr;
  Serial.print("[RX] ");
  Serial.write(data, len);
  Serial.println();
}

void onConnect(uint16_t conn)
{
  Serial.print("Connected, handle "); Serial.println(conn);
}

void onDisconnect(uint16_t conn, uint8_t reason)
{
  (void) conn;
  Serial.print("Disconnected, reason = 0x"); Serial.println(reason, HEX);
}

void setup()
{
  Serial.begin(115200);
  delay(300);
  Serial.println("Bluefruit54 custom service example");

  Bluefruit.setName("BARAM-CUSTOM");
  Bluefruit.begin();
  Bluefruit.Periph.setConnectCallback(onConnect);
  Bluefruit.Periph.setDisconnectCallback(onDisconnect);

  /* service.begin() first, then its characteristics. */
  svc.begin();

  chrTx.setProperties(CHR_PROPS_READ | CHR_PROPS_NOTIFY);
  chrTx.setMaxLen(20);
  chrTx.begin();
  chrTx.write("hello");

  chrRx.setProperties(CHR_PROPS_WRITE | CHR_PROPS_WRITE_WO_RESP);
  chrRx.setMaxLen(20);
  chrRx.setWriteCallback(onWrite);
  chrRx.begin();

  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addService(svc);
  Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.start(0);

  Serial.println("Advertising...");
}

void loop()
{
  static uint32_t n = 0;
  char buf[24];
  snprintf(buf, sizeof(buf), "tick %lu", (unsigned long) n++);

  /* Without a subscriber this only updates the stored value. */
  chrTx.notify(buf, strlen(buf));

  delay(2000);
}
