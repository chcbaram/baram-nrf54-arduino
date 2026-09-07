/*********************************************************************
 A BLE keyboard with exactly one key, driven by the user button.

 Pair from the host's Bluetooth settings, put the cursor where you want
 the character, then press the button. One press sends one 'a'.

 One key on purpose: a full keyboard example types into whatever window
 happens to be focused, which is hard to test and easy to regret.

 baram-nrf54l-arduino - MIT license
*********************************************************************/
#include <bluefruit.h>

BLEDis         bledis;
BLEHidAdafruit blehid;

#define KEY_CHAR   'a'

void setup()
{
  Serial.begin(115200);
  Serial.println("BLE HID single-key example");

  pinMode(PIN_BUTTON1, INPUT_PULLUP);      // pressed reads LOW

  Bluefruit.begin();
  Bluefruit.setTxPower(4);
  Bluefruit.setName("BARAM Key");

  /* HID needs an encrypted link, so bonding has to work first. */
  Bluefruit.Security.setIOCaps(false, false, false);   // Just Works
  Bluefruit.Security.setPairCompleteCallback(pair_cb);

  bledis.setManufacturer("BARAM");
  bledis.setModel("nRF54L HID");
  bledis.begin();

  err_t err = blehid.begin();
  if ( err ) { Serial.printf("blehid.begin failed 0x%02X\n", err); while (1) delay(100); }
  blehid.setKeyboardLedCallback(led_cb);

  startAdv();
  Serial.println("Advertising as a keyboard. Pair from Bluetooth settings.");
}

void startAdv(void)
{
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();

  /* Without this the host does not show a keyboard icon, and some hosts
   * will not offer to pair at all. */
  Bluefruit.Advertising.addAppearance(BLE_APPEARANCE_HID_KEYBOARD);
  Bluefruit.Advertising.addService(blehid);
  Bluefruit.ScanResponse.addName();

  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);
  Bluefruit.Advertising.start(0);
}

void pair_cb(uint16_t conn_handle, uint8_t status)
{
  (void) conn_handle;
  Serial.printf("Pairing %s (0x%02X), LESC=%d\n",
                status == 0 ? "ok" : "failed", status,
                Bluefruit.Security.lastPairingWasLesc());
}

void led_cb(uint16_t conn_handle, uint8_t leds)
{
  (void) conn_handle;
  Serial.printf("Keyboard LEDs: 0x%02X\n", leds);
}

void loop()
{
  /* Show when a host has connected and secured the link. */
  static bool was_conn = false;
  bool conn = Bluefruit.connected();
  if ( conn != was_conn )
  {
    was_conn = conn;
    Serial.println(conn ? "connected" : "disconnected");
  }

  static bool was_down = false;
  bool down = (digitalRead(PIN_BUTTON1) == LOW);

  if ( down != was_down )
  {
    was_down = down;

    if ( Bluefruit.connected() )
    {
      /* Press and release are separate reports. Sending only the press
       * leaves the host repeating the character. */
      if ( down ) blehid.keyPress(KEY_CHAR);
      else        blehid.keyRelease();

      Serial.printf("button %s\n", down ? "down -> key" : "up -> release");
    }
    delay(20);                              // debounce
  }
  delay(10);
}
