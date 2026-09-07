/*********************************************************************
 Advertising with a timeout and a slow mode.

 Advertises fast for a while, then slowly, then stops. Fast advertising
 is found quickly but costs current, so battery devices usually back off
 after the first half minute.

 baram-nrf54l-arduino - MIT license
*********************************************************************/
#include <bluefruit.h>

BLEUart bleuart;

void setup()
{
  Serial.begin(115200);
  Serial.println("Advanced advertising example");

  Bluefruit.begin();
  Bluefruit.setName("BARAM Adv");
  Bluefruit.Periph.setConnectCallback(connected);
  Bluefruit.Advertising.setStopCallback(adv_stopped);

  bleuart.begin();

  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(bleuart);
  Bluefruit.ScanResponse.addName();          // no room left in the main packet

  /* Intervals are in units of 0.625 ms: 32 = 20 ms, 244 = 152.5 ms.
   * Apple's guidance is not to advertise faster than 20 ms. */
  Bluefruit.Advertising.setInterval(32, 244);
  Bluefruit.Advertising.setFastTimeout(30);  // seconds in fast mode
  Bluefruit.Advertising.restartOnDisconnect(true);

  Bluefruit.Advertising.start(60);           // stop entirely after 60 s
  Serial.println("Advertising for 60 seconds");
}

void connected(uint16_t conn_handle)
{
  (void) conn_handle;
  Serial.println("Connected");
}

void adv_stopped(void)
{
  /* Reached the timeout without a connection. A real device would sleep
   * here and advertise again on a button press. */
  Serial.println("Advertising stopped");
}

void loop()
{
}
