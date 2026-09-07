/*********************************************************************
 Peripheral and central at the same time - a BLE UART bridge.

 The board advertises a UART service for a phone to connect to, and at
 the same time scans for another UART peripheral and connects to it.
 Whatever arrives on one side goes out the other.

 Run Peripheral/bleuart on a second board, then connect to this one from
 a phone: what you type on the phone shows up on the second board.

 baram-nrf54l-arduino - MIT license
*********************************************************************/
#include <bluefruit.h>

BLEUart       bleuart;      // our own service, for the phone
BLEClientUart clientUart;   // the peer's service, as a central

void setup()
{
  Serial.begin(115200);
  Serial.println("Dual role BLE UART bridge");

  /* One link in each role. The split is chosen here, not at build time,
   * and begin() fails rather than quietly capping if the RAM the linker
   * reserved cannot hold it. */
  if ( !Bluefruit.begin(1, 1) )
  {
    Serial.println("Bluefruit.begin(1, 1) failed");
    while (1) delay(100);
  }
  Bluefruit.setTxPower(4);
  Bluefruit.setName("BARAM Bridge");

  /* Peripheral side */
  Bluefruit.Periph.setConnectCallback(prph_connected);
  Bluefruit.Periph.setDisconnectCallback(prph_disconnected);
  bleuart.begin();
  bleuart.setRxCallback(prph_rx);

  /* Central side */
  Bluefruit.Central.setConnectCallback(central_connected);
  Bluefruit.Central.setDisconnectCallback(central_disconnected);
  clientUart.begin();
  clientUart.setRxCallback(central_rx);

  Bluefruit.Scanner.filterUuid(clientUart.uuid);
  Bluefruit.Scanner.setRxCallback(scan_callback);
  Bluefruit.Scanner.useActiveScan(false);
  Bluefruit.Scanner.start(0);

  startAdv();
  Serial.println("Advertising and scanning at once.");
}

void startAdv(void)
{
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addService(bleuart);
  Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.start(0);
}

void scan_callback(ble_gap_evt_adv_report_t *report)
{
  /* Do not connect to ourselves or to the phone - only to something
   * advertising the UART service, which the filter already ensures. */
  Serial.print("Peripheral found, connecting: ");
  Serial.printBufferReverse(report->peer_addr.addr, 6, ':');
  Serial.println();

  Bluefruit.Central.connect(report);
}

/* ── peripheral side ──────────────────────────────────────────────── */

void prph_connected(uint16_t conn_handle)
{
  (void) conn_handle;
  Serial.println("[periph] phone connected");

  /* Connecting stopped the scanner earlier; keep looking for the peer. */
  if ( !clientUart.discovered() ) Bluefruit.Scanner.start(0);
}

void prph_disconnected(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle; (void) reason;
  Serial.println("[periph] phone disconnected");
}

void prph_rx(uint16_t conn_handle)
{
  (void) conn_handle;
  uint8_t buf[64];

  while ( bleuart.available() )
  {
    int n = bleuart.read(buf, sizeof(buf));
    if ( n <= 0 ) break;

    Serial.write(buf, n);
    if ( clientUart.discovered() ) clientUart.write(buf, n);   // forward
  }
}

/* ── central side ─────────────────────────────────────────────────── */

void central_connected(uint16_t conn_handle)
{
  if ( clientUart.discover(conn_handle) )
  {
    clientUart.enableTXD();
    Serial.println("[central] peer UART ready");
  }
  else
  {
    Serial.println("[central] no UART service, dropping");
    Bluefruit.disconnect(conn_handle);
  }
}

void central_disconnected(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle; (void) reason;
  Serial.println("[central] peer disconnected, scanning again");
  Bluefruit.Scanner.start(0);
}

void central_rx(BLEClientUart &uart)
{
  uint8_t buf[64];

  while ( uart.available() )
  {
    int n = uart.read(buf, sizeof(buf));
    if ( n <= 0 ) break;

    Serial.write(buf, n);
    bleuart.write(buf, n);                                     // forward
  }
}

void loop()
{
}
