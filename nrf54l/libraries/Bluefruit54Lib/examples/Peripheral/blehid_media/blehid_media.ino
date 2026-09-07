/*********************************************************************
 Media keys over BLE, driven by the user button.

 Pair from the host's Bluetooth settings, start playing something, then
 press the button to toggle play/pause.

 These are HID consumer control usages, a separate report from the
 keyboard, so they reach the media keys rather than the focused window.

 baram-nrf54l-arduino - MIT license
*********************************************************************/
#include <bluefruit.h>

BLEDis         bledis;
BLEHidAdafruit blehid;

void setup()
{
  Serial.begin(115200);
  Serial.println("BLE HID media key example");

  pinMode(PIN_BUTTON1, INPUT_PULLUP);      // pressed reads LOW

  Bluefruit.begin();
  Bluefruit.setTxPower(4);
  Bluefruit.setName("BARAM Media");

  Bluefruit.Security.setIOCaps(false, false, false);

  bledis.setManufacturer("BARAM");
  bledis.setModel("nRF54L HID");
  bledis.begin();

  if ( blehid.begin() ) { Serial.println("blehid.begin failed"); while (1) delay(100); }

  startAdv();
  Serial.println("Advertising. Pair, play something, then press the button.");
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

void loop()
{
  static bool was_down = false;
  bool down = (digitalRead(PIN_BUTTON1) == LOW);

  if ( down != was_down )
  {
    was_down = down;

    if ( down && Bluefruit.connected() )
    {
      /* Press then release. Sending only the press leaves the host
       * thinking the key is still held. */
      blehid.consumerKeyPress(HID_USAGE_CONSUMER_PLAY_PAUSE);
      delay(10);
      blehid.consumerKeyRelease();

      Serial.println("play/pause");
    }
    delay(20);                              // debounce
  }
  delay(10);
}
