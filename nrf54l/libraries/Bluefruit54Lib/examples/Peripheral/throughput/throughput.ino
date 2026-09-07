/*
 * throughput — measure how fast BLE actually moves bytes.
 *
 * Prints the negotiated PHY, MTU and connection interval, then sends a fixed
 * amount of data and reports KB/s. Bytes arriving from the other side are
 * counted the same way, so both directions can be measured.
 *
 * Throughput is not set by one knob. It is the product of PHY (1M or 2M),
 * ATT MTU, link layer data length and connection interval. Ask for all of
 * them - the peer may refuse any one, which is why the negotiated values are
 * printed rather than assumed.
 *
 * Run extras/mac_throughput.py on a Mac as the other side, or use a phone's
 * BLE UART app.
 *
 * MIT license.
 */
#include <bluefruit.h>

BLEUart bleuart;

#define PACKET_NUM  1000    // packets to send in one test run

static uint8_t  test_data[247];
static uint32_t rx_count, rx_start_ms, rx_last_ms;
static volatile bool notify_on;

void setup()
{
  Serial.begin(115200);

  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
  Bluefruit.begin();
  Bluefruit.setTxPower(4);
  Bluefruit.setName("BARAM Throughput");
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);
  Bluefruit.Periph.setConnInterval(6, 12);   // 7.5 - 15 ms

  bleuart.begin();
  bleuart.setRxCallback(rx_callback);
  bleuart.setNotifyCallback(notify_callback);

  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(bleuart);
  Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.start(0);

  Serial.println("\nthroughput - waiting for a connection");
}

void connect_callback(uint16_t conn_hdl)
{
  BLEConnection *conn = Bluefruit.Connection(conn_hdl);
  if (conn == NULL) return;

  // Every one of these is a request. The peer decides.
  conn->requestPHY();                    // AUTO - 2M if both sides can
  conn->requestDataLengthUpdate();       // NULL - let the stack pick the max
  conn->requestMtuExchange(247);
  conn->requestConnectionParameter(6);   // 7.5 ms

  // The negotiations run on the radio, so give them a few intervals to land.
  delay(1000);

  uint16_t itv = conn->getConnectionInterval();   // 1.25 ms units
  Serial.printf("connected  PHY %s  MTU %u  payload %u  interval %u.%02u ms\n",
                phy_name(conn->getPHY()), conn->getMtu(),
                Bluefruit.maxPayload(conn_hdl),
                (itv * 125) / 100, (itv * 125) % 100);
  Serial.println("send any byte over BLE, or press a key here, to start");
}

void disconnect_callback(uint16_t conn_hdl, uint8_t reason)
{
  (void) conn_hdl;
  notify_on = false;
  Serial.printf("disconnected, reason 0x%02X\n", reason);

  if (rx_count) {
    print_speed("received ", rx_count, rx_last_ms - rx_start_ms);
    rx_count = 0;
  }
}

void notify_callback(uint16_t conn_hdl, bool enabled)
{
  (void) conn_hdl;
  notify_on = enabled;
}

void rx_callback(uint16_t conn_hdl)
{
  (void) conn_hdl;

  rx_last_ms = millis();
  if (rx_count == 0) rx_start_ms = rx_last_ms;

  rx_count += bleuart.available();
  bleuart.flush();     // count and drop - we only care about the rate
}

void loop()
{
  if (!notify_on) { delay(10); return; }

  // Either side can start the test.
  bool go = false;
  while (Serial.available()) { Serial.read(); go = true; }
  if (rx_count == 1) { rx_count = 0; go = true; }   // a single byte = "go"

  if (go) test_throughput();
  delay(10);
}

const char *phy_name(uint8_t phy)
{
  const char *name;

  switch (phy) {
    case BLE_GAP_PHY_1MBPS:
      name = "1M";
      break;

    case BLE_GAP_PHY_2MBPS:
      name = "2M";
      break;

    case BLE_GAP_PHY_CODED:
      name = "coded";
      break;

    default:
      name = "?";
      break;
  }

  return name;
}

void print_speed(const char *what, uint32_t bytes, uint32_t ms)
{
  if (ms == 0) ms = 1;
  Serial.printf("%s%lu bytes in %lu.%03lu s = %lu.%02lu KB/s\n",
                what, bytes, ms / 1000, ms % 1000,
                (bytes / ms), ((bytes * 100 / ms) % 100));
}

void test_throughput(void)
{
  uint16_t conn_hdl = Bluefruit.connHandle();
  uint16_t chunk    = Bluefruit.maxPayload(conn_hdl);

  if (chunk > sizeof(test_data)) chunk = sizeof(test_data);
  memset(test_data, '1', chunk);

  uint32_t total = (uint32_t) chunk * PACKET_NUM;
  Serial.printf("sending %lu bytes in %u byte packets ...\n", total, chunk);

  uint32_t sent  = 0;
  uint32_t start = millis();

  while (sent < total && Bluefruit.connected(conn_hdl)) {
    // write() blocks until the queue drains, so this is not a busy loop.
    size_t n = bleuart.write(conn_hdl, test_data, chunk);
    if (n == 0) break;
    sent += n;
  }

  print_speed("sent ", sent, millis() - start);
}
