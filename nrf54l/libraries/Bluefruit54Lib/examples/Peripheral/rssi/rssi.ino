/*********************************************************************
 Report the signal strength of a connection.

 Connect with any BLE app and watch the serial monitor. RSSI is in dBm,
 so bigger is closer: -40 is next to the host, -90 is far away.

 Reporting has to be started per connection with monitorRssi(); it is
 off by default because it costs radio time.

 baram-nrf54l-arduino - MIT license
*********************************************************************/
#include <bluefruit.h>

BLEUart bleuart;

void setup()
{
  Serial.begin(115200);
  Serial.println("BLE RSSI example");

  Bluefruit.begin();
  Bluefruit.setName("BARAM RSSI");
  Bluefruit.setRssiCallback(rssi_changed);
  Bluefruit.Periph.setConnectCallback(connected);
  Bluefruit.Periph.setDisconnectCallback(disconnected);

  bleuart.begin();

  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addService(bleuart);
  Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.start(0);

  Serial.println("Advertising. Connect to see RSSI.");
}

void connected(uint16_t conn_handle)
{
  BLEConnection *conn = Bluefruit.Connection(conn_handle);
  if ( conn == NULL ) return;

  /* 0 means report every change. A threshold in dBm cuts the traffic. */
  conn->monitorRssi(0);
  Serial.println("Connected, RSSI reporting on");
}

void rssi_changed(uint16_t conn_handle, int8_t rssi)
{
  (void) conn_handle;
  Serial.printf("RSSI %d dBm\n", rssi);
}

void disconnected(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle; (void) reason;
  Serial.println("Disconnected");
}

void loop()
{
  /* Polling works too, if a callback does not suit. */
  static uint32_t t = 0;
  uint16_t h = Bluefruit.connHandle();

  if ( h != BLE_CONN_HANDLE_INVALID && millis() - t > 5000 )
  {
    t = millis();
    BLEConnection *conn = Bluefruit.Connection(h);
    if ( conn ) Serial.printf("   (polled) %d dBm\n", conn->getRssi());
  }
  delay(50);
}
