/*********************************************************************
 Connect to a BLE UART peripheral and bridge it to the serial monitor.

 Run the Peripheral/bleuart example on another board, then this one.
 Anything typed here goes out over BLE, and anything received is printed.

 baram-nrf54l-arduino - MIT license
*********************************************************************/
#include <bluefruit.h>

BLEClientUart clientUart;

void setup()
{
  Serial.begin(115200);

  Serial.println("Central BLE UART example");

  // 0 peripheral links, 1 central link
  if ( !Bluefruit.begin(0, 1) )
  {
    Serial.println("Bluefruit.begin() failed");
    while (1) delay(100);
  }
  Bluefruit.setTxPower(4);

  clientUart.begin();
  clientUart.setRxCallback(bleuart_rx_callback);

  Bluefruit.Central.setConnectCallback(connect_callback);
  Bluefruit.Central.setDisconnectCallback(disconnect_callback);

  // Only report peripherals advertising the UART service
  Bluefruit.Scanner.filterUuid(clientUart.uuid);
  Bluefruit.Scanner.setRxCallback(scan_callback);
  Bluefruit.Scanner.useActiveScan(false);
  Bluefruit.Scanner.start(0);                 // 0 = scan forever

  Serial.println("Scanning for a BLE UART peripheral ...");
}

void scan_callback(ble_gap_evt_adv_report_t *report)
{
  Serial.print("Found ");
  Serial.printBufferReverse(report->peer_addr.addr, 6, ':');
  Serial.printf("  rssi %d, connecting\n", report->rssi);

  /* Connecting stops the scanner. No resume() here - the connect
   * callback decides what to do next. */
  Bluefruit.Central.connect(report);
}

void connect_callback(uint16_t conn_handle)
{
  Serial.print("Connected, discovering UART service ... ");

  /* Blocking GATT discovery. Safe here: connect callbacks run on their
   * own task, not on the BLE event task. */
  if ( !clientUart.discover(conn_handle) )
  {
    Serial.println("not found");
    Bluefruit.disconnect(conn_handle);
    return;
  }

  if ( !clientUart.enableTXD() )
  {
    Serial.println("could not enable notifications");
    Bluefruit.disconnect(conn_handle);
    return;
  }

  BLEConnection *conn = Bluefruit.Connection(conn_handle);
  Serial.printf("ready, MTU %d\n", conn ? conn->getMtu() : 0);
  Serial.println("Type to send.");
}

void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle;
  Serial.printf("Disconnected, reason 0x%02X. Scanning again.\n", reason);

  Bluefruit.Scanner.start(0);
}

void bleuart_rx_callback(BLEClientUart &uart)
{
  uint8_t buf[64];

  while ( uart.available() )
  {
    int count = uart.read(buf, sizeof(buf));
    if ( count <= 0 ) break;
    Serial.print("[peer] ");
    Serial.write(buf, count);
    Serial.println();
  }
}

void loop()
{
  // Forward serial input to the peer
  while ( Serial.available() )
  {
    delay(2);
    uint8_t buf[64];
    int count = Serial.readBytes(buf, sizeof(buf));
    clientUart.write(buf, count);
  }
}
