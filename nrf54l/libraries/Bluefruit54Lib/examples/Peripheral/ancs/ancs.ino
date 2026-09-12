/*********************************************************************
 Show the iPhone's notifications on the serial port.

 ANCS is Apple's notification service. The phone runs the server; the
 board connects as a client and is told whenever a notification is
 added, changed or removed. For each one it asks for the app name,
 title and message and prints them.

 The board advertises asking for ANCS rather than offering it, and iOS
 only hands the service to a bonded peer, so pairing is part of the
 flow. iOS may not list a plain peripheral in its Bluetooth settings -
 connect from nRF Connect instead and accept the pairing prompt the
 board asks for.

 baram-nrf54l-arduino - MIT license
*********************************************************************/
#include <bluefruit.h>

BLEAncs bleancs;

void setup()
{
  Serial.begin(115200);
  Serial.println("BLE ANCS example");

  Bluefruit.begin();
  Bluefruit.setTxPower(4);
  Bluefruit.setName("BARAM Notify");

  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);
  Bluefruit.Security.setSecuredCallback(secured_callback);

  bleancs.begin();
  bleancs.setNotificationCallback(notification_callback);

  startAdv();
  Serial.println("Advertising. Connect and pair from the phone.");
}

void startAdv(void)
{
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();

  /* Asks the phone for ANCS - a solicited UUID, not a service we host. */
  Bluefruit.Advertising.addService(bleancs);

  Bluefruit.Advertising.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.start(0);
}

void connect_callback(uint16_t conn_handle)
{
  Serial.println("Connected. Asking to pair.");

  /* iOS gives ANCS to bonded peers only, and an app connecting to us
   * does not pair on its own. */
  BLEConnection *conn = Bluefruit.Connection(conn_handle);
  if ( conn && !conn->secured() ) conn->requestPairing();
}

void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  Serial.printf("Disconnected, reason 0x%02X\n", reason);
}

void secured_callback(uint16_t conn_handle)
{
  Serial.println("Link secured, looking for ANCS");

  if ( !bleancs.discover(conn_handle) )
  {
    Serial.println("  no ANCS on this peer");
    return;
  }

  Serial.println("  found - notifications will appear below");
  bleancs.enableNotification();
}

const char *categoryName(uint8_t id)
{
  switch (id)
  {
    case ANCS_CAT_INCOMING_CALL: return "call";
    case ANCS_CAT_MISSED_CALL:   return "missed call";
    case ANCS_CAT_VOICE_MAIL:    return "voicemail";
    case ANCS_CAT_SOCIAL:        return "social";
    case ANCS_CAT_SCHEDULE:      return "schedule";
    case ANCS_CAT_EMAIL:         return "email";
    case ANCS_CAT_NEWS:          return "news";
    case ANCS_CAT_LOCATION:      return "location";
    case ANCS_CAT_ENTERTAINMENT: return "entertainment";
    default:                     return "other";
  }
}

/* Cut a truncated UTF-8 string back to a character boundary.
 *
 * The phone sends as much as the buffer asked for and stops, which
 * lands mid-character for anything outside ASCII - Korean text came
 * back ending in a broken glyph. Drop the trailing bytes of a
 * sequence that did not fit. */
void trimUtf8(char *s)
{
  int i = strlen(s) - 1;
  int cont = 0;

  /* Walk back over continuation bytes to the byte that leads them. */
  while ( i >= 0 && (s[i] & 0xC0) == 0x80 ) { cont++; i--; }
  if ( i < 0 ) { s[0] = 0; return; }          // nothing but continuations

  unsigned char lead = s[i];
  int need = (lead & 0x80) == 0x00 ? 1 :
             (lead & 0xE0) == 0xC0 ? 2 :
             (lead & 0xF0) == 0xE0 ? 3 :
             (lead & 0xF8) == 0xF0 ? 4 : 1;

  /* Only drop it when the character is actually short of its bytes.
   * A complete one ends in continuation bytes too, and cutting those
   * eats the last real character - which is what happened first try:
   * the Messages app came through as a two-syllable stub. */
  if ( cont + 1 < need ) s[i] = 0;
}

/* Runs on the core's callback task, not the BLE event pump, so the
 * blocking getters below are safe to call from here. */
void notification_callback(AncsNotification_t *notif)
{
  if ( notif->eventID == ANCS_EVT_NOTIFICATION_REMOVED )
  {
    Serial.printf("removed  uid=%lu\n", (unsigned long) notif->uid);
    return;
  }

  /* Generous enough that most notifications arrive whole - non-ASCII
   * text costs three bytes a character. */
  char app[64]   = { 0 };
  char title[96] = { 0 };
  char body[192] = { 0 };

  bleancs.getAppName(notif->uid, app, sizeof(app) - 1);
  bleancs.getTitle(notif->uid, title, sizeof(title) - 1);
  bleancs.getMessage(notif->uid, body, sizeof(body) - 1);

  trimUtf8(app);
  trimUtf8(title);
  trimUtf8(body);

  Serial.printf("%s [%s] %s: %s\n",
                (notif->eventID == ANCS_EVT_NOTIFICATION_ADDED) ? "added  " : "changed",
                categoryName(notif->categoryID), app, title);
  if ( body[0] ) Serial.printf("         %s\n", body);
}

void loop()
{
  delay(1000);
}
