/*********************************************************************
 Multiple concurrent BLE UART connections.

 Anything received on one link is echoed to the serial monitor and to
 every other connected link, so two phones can talk through the board.

 baram-nrf54l-arduino - MIT license
*********************************************************************/
#include <bluefruit.h>

/* How many peripheral links to ask for. The ceiling is the SoftDevice RAM the
 * linker reserved, not a compile-time constant, so begin() below reports if
 * this does not fit. BLE_DEFAULT_PERIPH_COUNT is what the board sets in
 * boards.txt: 4 on nRF54L15, 2 on nRF54L05. */
#define MAX_PRPH_CONNECTION   BLE_DEFAULT_PERIPH_COUNT

BLEDis  bledis;
BLEUart bleuart;

void setup()
{
  Serial.begin(115200);

  Serial.println("BLEUART multi-connection example");
  Serial.print  ("Max connections: "); Serial.println(MAX_PRPH_CONNECTION);

  Bluefruit.autoConnLed(true);

  /* Fails instead of silently falling back to one link, so a board that
   * cannot hold this many says so. */
  if ( !Bluefruit.begin(MAX_PRPH_CONNECTION, 0) )
  {
    Serial.println("Bluefruit.begin() failed");
    while (1) delay(100);
  }
  Bluefruit.setTxPower(4);
  Bluefruit.setName("BARAM Multi");

  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);

  bledis.setManufacturer("BARAM");
  bledis.setModel("nRF54L");
  bledis.begin();

  bleuart.begin();

  startAdv();
}

void startAdv(void)
{
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(bleuart);
  Bluefruit.ScanResponse.addName();

  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);    // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.start(0);                // 0 = advertise forever
}

/* Send to every link except 'skip'. Pass BLE_CONN_HANDLE_INVALID for all.
 *
 * The handle-less bleuart.write() only reaches Bluefruit.connHandle(),
 * so reaching everyone means walking the links yourself.
 *
 * Walk slots with connHandleAt(), not handle values: a handle is not a slot
 * index and can be larger than the link count. Looping conn_hdl from 0 to
 * MAX_PRPH_CONNECTION silently misses those links. */
void sendToAll(uint16_t skip, const uint8_t *buf, int count)
{
  for (uint8_t i = 0; i < BLE_MAX_CONNECTION; i++)
  {
    uint16_t conn_hdl = Bluefruit.connHandleAt(i);

    if ( conn_hdl == BLE_CONN_HANDLE_INVALID ) continue;
    if ( conn_hdl == skip ) continue;

    bleuart.write(conn_hdl, buf, count);
  }
}

/* Called from the BLE event task once per write, with the sender's handle.
 * The RX FIFO itself is shared, so this is the only place that knows who
 * sent it - drain it here rather than in loop(). */
void rx_callback(uint16_t conn_hdl)
{
  uint8_t buf[64];

  while ( bleuart.available() )
  {
    int count = bleuart.read(buf, sizeof(buf));
    if ( count <= 0 ) break;

    Serial.print("["); Serial.print(conn_hdl); Serial.print("] ");
    Serial.write(buf, count);
    Serial.println();

    sendToAll(conn_hdl, buf, count);
  }
}

void loop()
{
  // Forward serial input to every connected link
  while ( Serial.available() )
  {
    delay(2);                                  // wait for the rest of the line

    uint8_t buf[64];
    int count = Serial.readBytes(buf, sizeof(buf));
    sendToAll(BLE_CONN_HANDLE_INVALID, buf, count);
  }

  // Forward anything that arrived while no rx callback was set
  while ( bleuart.available() )
  {
    Serial.write((uint8_t) bleuart.read());
  }
}

void connect_callback(uint16_t conn_handle)
{
  Serial.print("Connected, handle "); Serial.print(conn_handle);
  Serial.print(", total ");           Serial.println(Bluefruit.connected());

  bleuart.setRxCallback(rx_callback);

  /* Connecting stops the advertiser, so restart it while slots remain.
   * The library does not do this for you - same as Adafruit. */
  if ( Bluefruit.connected() < MAX_PRPH_CONNECTION )
  {
    Serial.println("Keep advertising");
    Bluefruit.Advertising.start(0);
  }
}

void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  /* This link is already counted out here, so connected() is the number left. */
  Serial.print("Disconnected, handle "); Serial.print(conn_handle);
  Serial.print(", reason 0x");           Serial.print(reason, HEX);
  Serial.print(", left ");               Serial.println(Bluefruit.connected());
}
