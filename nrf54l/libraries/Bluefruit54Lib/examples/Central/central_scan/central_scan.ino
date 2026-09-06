/*********************************************************************
 Scan for nearby BLE advertisements and print them.

 Central role, no connection. Shows the address, RSSI, raw advertising
 bytes and the name if one is advertised.

 baram-nrf54l-arduino - MIT license
*********************************************************************/
#include <bluefruit.h>

void setup()
{
  Serial.begin(115200);

  Serial.println("Central scan example");
  Serial.println("Timestamp Addr              Rssi Data");

  // 0 peripheral links, 1 central link
  if ( !Bluefruit.begin(0, 1) )
  {
    Serial.println("Bluefruit.begin() failed");
    while (1) delay(100);
  }
  Bluefruit.setTxPower(4);

  Bluefruit.Scanner.setRxCallback(scan_callback);
  Bluefruit.Scanner.useActiveScan(true);      // also ask for scan responses
  Bluefruit.Scanner.setInterval(160, 80);     // in unit of 0.625 ms
  Bluefruit.Scanner.start(0);                 // 0 = scan forever

  Serial.println("Scanning ...");
}

void scan_callback(ble_gap_evt_adv_report_t *report)
{
  Serial.printf("%09lu ", millis());

  // The address is little endian, so print it back to front
  Serial.printBufferReverse(report->peer_addr.addr, 6, ':');
  Serial.printf(" %4d  ", report->rssi);

  Serial.printBuffer(report->data.p_data, report->data.len, '-');

  uint8_t name[32] = { 0 };
  uint8_t len = Bluefruit.Scanner.parseReportByType(
      report, BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME, name, sizeof(name) - 1);
  if ( len == 0 )
  {
    len = Bluefruit.Scanner.parseReportByType(
        report, BLE_GAP_AD_TYPE_SHORT_LOCAL_NAME, name, sizeof(name) - 1);
  }
  if ( len ) Serial.printf("  '%s'", (char *) name);

  if ( Bluefruit.Scanner.checkReportForUuid(report, BLEUART_UUID_SERVICE) )
  {
    Serial.print("  [BLE UART]");
  }
  Serial.println();

  /* Required: the scanner stops after every report, so it has to be told
   * to carry on. Without this only the first advertisement ever arrives. */
  Bluefruit.Scanner.resume();
}

void loop()
{
  // nothing to do
}
