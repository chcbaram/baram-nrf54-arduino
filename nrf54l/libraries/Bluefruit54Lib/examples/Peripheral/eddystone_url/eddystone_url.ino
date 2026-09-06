/*********************************************************************
 EddyStone URL beacon.

 Advertises a URL. The scheme and common endings are compressed to one
 byte each, so the encoded URL has to fit in 17 bytes.

 baram-nrf54l-arduino - MIT license
*********************************************************************/
#include <bluefruit.h>

#define URL   "https://github.com/chcbaram"

// RSSI measured at 0 m, then the URL
EddyStoneUrl eddyUrl(-40, URL);

void setup()
{
  Serial.begin(115200);

  Serial.println("EddyStone URL example");

  Bluefruit.begin();
  Bluefruit.setTxPower(4);

  startAdv();

  Serial.println("Broadcasting " URL);
}

void startAdv(void)
{
  /* false means the URL did not fit in 17 bytes after compression. */
  if ( !Bluefruit.Advertising.setBeacon(eddyUrl) )
  {
    Serial.println("URL too long for an EddyStone frame");
    return;
  }
  Bluefruit.ScanResponse.addName();

  Bluefruit.Advertising.setType(BLE_GAP_ADV_TYPE_NONCONNECTABLE_SCANNABLE_UNDIRECTED);
  Bluefruit.Advertising.setInterval(160, 160);   // in unit of 0.625 ms
  Bluefruit.Advertising.start(0);
}

void loop()
{
  digitalToggle(LED_BUILTIN);
  delay(1000);
}
