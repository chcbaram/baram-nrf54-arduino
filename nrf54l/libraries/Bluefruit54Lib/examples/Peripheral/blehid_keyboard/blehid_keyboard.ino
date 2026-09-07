/*********************************************************************
 A full BLE keyboard. What you type in the serial monitor is typed on
 the host.

 Pair from the host's Bluetooth settings, put the cursor where you want
 the text, then type here.

 ⚠ The characters land in whatever window has focus on the host, not in
   the serial monitor. Click into a text editor first, or you will type
   into something you did not mean to. Peripheral/blehid_button sends a
   single character on a button press if you want something tamer.

 baram-nrf54l-arduino - MIT license
*********************************************************************/
#include <bluefruit.h>

BLEDis         bledis;
BLEHidAdafruit blehid;

void setup()
{
  Serial.begin(115200);
  Serial.println("BLE HID keyboard example");

  Bluefruit.begin();
  Bluefruit.setTxPower(4);
  Bluefruit.setName("BARAM Keyboard");

  /* HID needs an encrypted link, so pairing has to work. */
  Bluefruit.Security.setIOCaps(false, false, false);

  bledis.setManufacturer("BARAM");
  bledis.setModel("nRF54L HID");
  bledis.begin();

  if ( blehid.begin() ) { Serial.println("blehid.begin failed"); while (1) delay(100); }
  blehid.setKeyboardLedCallback(led_cb);

  startAdv();
  Serial.println("Pair from Bluetooth settings, then type here.");
}

void startAdv(void)
{
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addAppearance(BLE_APPEARANCE_HID_KEYBOARD);
  Bluefruit.Advertising.addService(blehid);
  Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.start(0);
}

/* CapsLock, NumLock and friends, as the host sees them. */
void led_cb(uint16_t conn_handle, uint8_t leds)
{
  (void) conn_handle;
  Serial.printf("LEDs 0x%02X%s\n", leds, (leds & 0x02) ? "  (CapsLock on)" : "");
}

void loop()
{
  if ( Serial.available() && Bluefruit.connected() )
  {
    char ch = (char) Serial.read();

    /* Press then release. Sending only the press leaves the host
     * repeating the character until something else arrives. */
    blehid.keyPress(ch);
    delay(5);
    blehid.keyRelease();
    delay(5);
  }
  delay(2);
}
