/*********************************************************************
 Read the time from a phone.

 The board advertises asking for the Current Time Service rather than
 offering one, so this is the board acting as a GATT client while it is
 the peripheral. iOS runs a Current Time server and hands it over once
 the link is encrypted, so pairing is part of the flow, not optional.

 Pair from the phone's Bluetooth settings. On iOS the board may show up
 as "Accessory". Once it is paired the time appears on the serial port,
 and it reappears whenever the phone changes its clock.

 baram-nrf54l-arduino - MIT license
*********************************************************************/
#include <bluefruit.h>

BLEClientCts bleCTime;

bool printed = false;

void setup()
{
  Serial.begin(115200);
  Serial.println("BLE current time client example");

  Bluefruit.begin();
  Bluefruit.setTxPower(4);
  Bluefruit.setName("BARAM Clock");

  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);
  Bluefruit.Security.setSecuredCallback(secured_callback);

  bleCTime.begin();
  bleCTime.setAdjustCallback(adjust_callback);

  startAdv();
  Serial.println("Advertising. Pair from the phone's Bluetooth settings.");
}

void startAdv(void)
{
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addAppearance(BLE_APPEARANCE_GENERIC_CLOCK);

  /* Asks the phone for its clock - this goes out as a solicited UUID,
   * not as a service we host. */
  Bluefruit.Advertising.addService(bleCTime);

  Bluefruit.Advertising.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.start(0);
}

void connect_callback(uint16_t conn_handle)
{
  Serial.println("Connected. Asking to pair.");
  printed = false;

  /* Ask for pairing ourselves rather than waiting for the phone to do
   * it. Upstream leaves this to the user pairing from the phone's
   * Bluetooth settings, but a plain BLE peripheral does not always show
   * up in that list - and connecting from an app like nRF Connect pairs
   * nothing on its own, so the link would stay unencrypted and the
   * phone would never hand over its clock. */
  BLEConnection *conn = Bluefruit.Connection(conn_handle);
  if ( conn && !conn->secured() ) conn->requestPairing();
}

void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  Serial.printf("Disconnected, reason 0x%02X\n", reason);
  printed = false;
}

/* Discovery has to wait for encryption - an unencrypted link is not
 * given the phone's Current Time service at all.
 *
 * Discovering from here is safe: the core runs this callback on its own
 * task, not on the BLE event pump, so blocking on the discovery replies
 * does not stop the events those replies arrive in. */
void secured_callback(uint16_t conn_handle)
{
  Serial.println("Link secured, looking for the Current Time service");

  if ( !bleCTime.discover(conn_handle) )
  {
    Serial.println("  no Current Time service on this peer");
    return;
  }
  Serial.println("  found");

  bleCTime.enableAdjust();      // tell me when the phone's clock moves
}

void printTime(const char *what)
{
  Serial.printf("%s: %04u-%02u-%02u %02u:%02u:%02u  weekday=%u  adjust=0x%02X\n",
                what,
                bleCTime.Time.year, bleCTime.Time.month, bleCTime.Time.day,
                bleCTime.Time.hour, bleCTime.Time.minute, bleCTime.Time.second,
                bleCTime.Time.weekday, bleCTime.Time.adjust_reason);
}

/* The notification carries the new time, so Time is already up to date
 * by the time this runs - there is nothing to read back. */
void adjust_callback(uint8_t reason)
{
  printTime("adjusted");
}

void loop()
{
  if ( printed || !bleCTime.discovered() ) { delay(200); return; }

  if ( bleCTime.getCurrentTime() )
  {
    printTime("time");

    /* Timezone is optional - plenty of peers leave it out. */
    if ( bleCTime.getLocalTimeInfo() )
      Serial.printf("timezone: %d quarter-hours, dst offset %u\n",
                    bleCTime.LocalInfo.timezone, bleCTime.LocalInfo.dst_offset);
    else
      Serial.println("timezone: not published by this peer");

    printed = true;
  }
  delay(200);
}
