/*********************************************************************
 Pair, bond, and prove the bond survives a reconnect.

 Pair from the host's Bluetooth settings. On the next connection the
 link is encrypted with no pairing, and notifications are already on
 because the CCCD was restored from the bond.

 Bonds live in RRAM, past the application, so they survive reflashing.

 baram-nrf54l-arduino - MIT license
*********************************************************************/
#include <bluefruit.h>

BLEDis  bledis;
BLEUart bleuart;

/* Reading this needs an encrypted link, which is what makes the host
 * pair in the first place. */
BLEService        secureSvc(0x1234);
BLECharacteristic secureChr(0x5678);

void setup()
{
  Serial.begin(115200);
  Serial.println("BLE pairing / bonding example");

  Bluefruit.begin();
  Bluefruit.setName("BARAM Pairing");

  /* Just Works: no passkey, no user interaction. For a PIN instead, use
   * Bluefruit.Security.setPIN("123456") - that also turns on MITM. */
  Bluefruit.Security.setIOCaps(false, false, false);
  Bluefruit.Security.setMITM(false);
  Bluefruit.Security.setPairCompleteCallback(pair_complete);
  Bluefruit.Security.setSecuredCallback(secured);

  Bluefruit.Periph.setConnectCallback(connected);
  Bluefruit.Periph.setDisconnectCallback(disconnected);

  bledis.setManufacturer("BARAM");
  bledis.setModel("nRF54L");
  bledis.begin();
  bleuart.begin();

  secureSvc.begin();
  secureChr.setProperties(CHR_PROPS_READ);
  secureChr.setPermission(SECMODE_ENC_NO_MITM, SECMODE_NO_ACCESS);
  secureChr.setFixedLen(4);
  secureChr.begin();
  secureChr.write("BOND");

  Serial.printf("Stored bonds: %u\n", bondCount(BLE_GAP_ROLE_INVALID));
  bondPrintList(BLE_GAP_ROLE_INVALID);

  startAdv();
  Serial.println("Advertising. Pair from Bluetooth settings.");
}

void startAdv(void)
{
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addService(bleuart);
  Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.start(0);
}

void connected(uint16_t conn_handle)
{
  /* Already true here on a reconnect: the CCCD came back with the bond,
   * so the peer does not have to subscribe again. */
  Serial.printf("Connected. notify already on: %d\n", bleuart.notifyEnabled(conn_handle));
}

void secured(uint16_t conn_handle)
{
  BLEConnection *conn = Bluefruit.Connection(conn_handle);
  Serial.printf("Link encrypted. bonded=%d\n", conn ? conn->bonded() : 0);
}

void pair_complete(uint16_t conn_handle, uint8_t status)
{
  (void) conn_handle;

  if ( status == 0 )
  {
    Serial.printf("Paired. LESC=%d, bonds now %u\n",
                  Bluefruit.Security.lastPairingWasLesc(),
                  bondCount(BLE_GAP_ROLE_INVALID));
    bondPrintList(BLE_GAP_ROLE_INVALID);
  }
  else
  {
    Serial.printf("Pairing failed, status 0x%02X\n", status);
  }
}

void disconnected(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle;
  Serial.printf("Disconnected, reason 0x%02X. Reconnect to see the bond used.\n", reason);
}

void loop()
{
  while ( bleuart.available() ) Serial.write(bleuart.read());
}
