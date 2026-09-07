/*********************************************************************
 Read a peer's Device Information and Battery Level as a central.

 Run Peripheral/bleuart on another board, then this one. It scans for
 the UART service, connects, and reads the manufacturer, model and
 battery level off the peer.

 Shows BLEClientDis and BLEClientBas, which sit on the generic
 BLEClientService / BLEClientCharacteristic pair.

 baram-nrf54l-arduino - MIT license
*********************************************************************/
#include <bluefruit.h>

BLEClientDis  clientDis;
BLEClientBas  clientBas;
BLEClientUart clientUart;

void setup()
{
  Serial.begin(115200);
  Serial.println("Central client example");

  if ( !Bluefruit.begin(0, 1) )      // no peripheral links, one central
  {
    Serial.println("Bluefruit.begin() failed");
    while (1) delay(100);
  }
  Bluefruit.setTxPower(4);

  clientDis.begin();
  clientBas.begin();
  clientUart.begin();

  Bluefruit.Central.setConnectCallback(connected);
  Bluefruit.Central.setDisconnectCallback(disconnected);

  Bluefruit.Scanner.filterUuid(clientUart.uuid);
  Bluefruit.Scanner.setRxCallback(scan_callback);
  Bluefruit.Scanner.useActiveScan(false);
  Bluefruit.Scanner.start(0);

  Serial.println("Scanning ...");
}

void scan_callback(ble_gap_evt_adv_report_t *report)
{
  Serial.print("Found ");
  Serial.printBufferReverse(report->peer_addr.addr, 6, ':');
  Serial.println(", connecting");

  Bluefruit.Central.connect(report);
}

void connected(uint16_t conn_handle)
{
  char buf[32];

  /* Blocking discovery and reads. Safe here: connect callbacks run on
   * their own task, not on the BLE event task. */
  if ( clientDis.discover(conn_handle) )
  {
    if ( clientDis.getManufacturer(buf, sizeof(buf)) ) Serial.printf("Manufacturer: %s\n", buf);
    if ( clientDis.getModel(buf, sizeof(buf)) )        Serial.printf("Model       : %s\n", buf);
  }
  else Serial.println("No Device Information service");

  if ( clientBas.discover(conn_handle) ) Serial.printf("Battery     : %u%%\n", clientBas.read());
  else Serial.println("No Battery service");

  if ( clientUart.discover(conn_handle) )
  {
    clientUart.enableTXD();
    Serial.println("UART ready");
  }
}

void disconnected(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle;
  Serial.printf("Disconnected, reason 0x%02X. Scanning again.\n", reason);
  Bluefruit.Scanner.start(0);
}

void loop()
{
  while ( clientUart.available() ) Serial.write(clientUart.read());
}
