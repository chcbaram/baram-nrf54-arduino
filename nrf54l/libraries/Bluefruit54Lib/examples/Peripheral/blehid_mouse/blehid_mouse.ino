/*********************************************************************
 A BLE mouse driven by the user button.

 Pair from the host's Bluetooth settings, then hold the button: the
 cursor slides to the right for as long as it is held.

 Moving rather than clicking on purpose - a click lands on whatever is
 under the cursor. The line for a click is there, commented out.

 baram-nrf54l-arduino - MIT license
*********************************************************************/
#include <bluefruit.h>

BLEDis         bledis;
BLEHidAdafruit blehid;

#define STEP_PX   5        // pixels per report while held

void setup()
{
  Serial.begin(115200);
  Serial.println("BLE HID mouse example");

  pinMode(PIN_BUTTON1, INPUT_PULLUP);      // pressed reads LOW

  Bluefruit.begin();
  Bluefruit.setTxPower(4);
  Bluefruit.setName("BARAM Mouse");

  /* HID needs an encrypted link. */
  Bluefruit.Security.setIOCaps(false, false, false);

  bledis.setManufacturer("BARAM");
  bledis.setModel("nRF54L HID");
  bledis.begin();

  if ( blehid.begin() ) { Serial.println("blehid.begin failed"); while (1) delay(100); }

  startAdv();
  Serial.println("Advertising as a mouse. Pair, then hold the button.");
}

void startAdv(void)
{
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addAppearance(BLE_APPEARANCE_HID_MOUSE);
  Bluefruit.Advertising.addService(blehid);
  Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.start(0);
}

void loop()
{
  if ( Bluefruit.connected() && digitalRead(PIN_BUTTON1) == LOW )
  {
    /* Relative motion: each report nudges the cursor from where it is. */
    blehid.mouseMove(STEP_PX, 0);

    // blehid.mouseButtonPress(MOUSE_BUTTON_LEFT);   // a click, if you want one
    // blehid.mouseButtonRelease();

    delay(20);
  }
  delay(10);
}
