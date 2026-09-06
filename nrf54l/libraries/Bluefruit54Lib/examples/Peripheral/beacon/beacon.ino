/*********************************************************************
 iBeacon transmitter.

 Advertises only - no GATT, no connection. Check it with a beacon app
 such as nRF Connect or nRF Beacon.

 baram-nrf54l-arduino - MIT license
*********************************************************************/
#include <bluefruit.h>

/* iBeacon rides in the manufacturer specific data field, so a valid
 * company ID is required. Phone apps often filter on it:
 *   0x004C Apple, 0x0059 Nordic, 0x0822 Adafruit */
#define MANUFACTURER_ID   UUID16_COMPANY_ID_NORDIC

uint8_t beaconUuid[16] = {
  0x01, 0x12, 0x23, 0x34, 0x45, 0x56, 0x67, 0x78,
  0x89, 0x9a, 0xab, 0xbc, 0xcd, 0xde, 0xef, 0xf0
};

// UUID, major, minor, RSSI measured at 1 m
BLEBeacon beacon(beaconUuid, 1, 2, -54);

void setup()
{
  Serial.begin(115200);

  Serial.println("Beacon example");

  Bluefruit.begin();
  Bluefruit.autoConnLed(false);      // keep the LED off, this never connects
  Bluefruit.setTxPower(0);

  beacon.setManufacturer(MANUFACTURER_ID);

  startAdv();

  Serial.printf("Broadcasting, manufacturer ID = 0x%04X\n", MANUFACTURER_ID);

  suspendLoop();                     // nothing to run, save power
}

void startAdv(void)
{
  /* Replaces the advertising payload: 25 bytes of beacon leaves no room
   * for anything else in the 31 byte packet. The name goes in the scan
   * response instead. */
  Bluefruit.Advertising.setBeacon(beacon);
  Bluefruit.ScanResponse.addName();

  // Apple's spec: non-connectable, scannable, fixed 100 ms interval
  Bluefruit.Advertising.setType(BLE_GAP_ADV_TYPE_NONCONNECTABLE_SCANNABLE_UNDIRECTED);
  Bluefruit.Advertising.setInterval(160, 160);   // in unit of 0.625 ms
  Bluefruit.Advertising.start(0);                // 0 = advertise forever
}

void loop()
{
  // suspended
}
