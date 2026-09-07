/*********************************************************************
 A gamepad over BLE.

 Pair from the host's Bluetooth settings, then open something that
 shows gamepad input - a browser gamepad tester, a game, or on macOS
 any app that reads a controller. The board walks through the D-pad
 directions, then the first few buttons, then the analog sticks, one
 step a second, and prints each step on the serial port so you can
 line the two up.

 A gamepad is its own HID device here, not another report on the
 keyboard, so it advertises its own report map and appearance.

 baram-nrf54l-arduino - MIT license
*********************************************************************/
#include <bluefruit.h>

BLEDis        bledis;
BLEHidGamepad blegamepad;

hid_gamepad_report_t gp;

void setup()
{
  Serial.begin(115200);
  Serial.println("BLE HID gamepad example");

  Bluefruit.begin();
  Bluefruit.setTxPower(4);
  Bluefruit.setName("BARAM Gamepad");

  Bluefruit.Security.setIOCaps(false, false, false);

  bledis.setManufacturer("BARAM");
  bledis.setModel("nRF54L HID");
  bledis.begin();

  if ( blegamepad.begin() ) { Serial.println("blegamepad.begin failed"); while (1) delay(100); }

  startAdv();
  Serial.println("Advertising. Pair, then open a gamepad tester.");
}

void startAdv(void)
{
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addAppearance(BLE_APPEARANCE_HID_GAMEPAD);
  Bluefruit.Advertising.addService(blegamepad);
  Bluefruit.ScanResponse.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.start(0);
}

/* Send the current report and say what it was. */
void step(const char *what)
{
  bool sent = blegamepad.report(&gp);

  Serial.print(what);
  /* A dropped report means the host has not subscribed to the input
   * report characteristic - it is connected but not listening. */
  Serial.println(sent ? "" : "   (not delivered - host is not subscribed)");

  delay(1000);
}

void clear(void)
{
  memset(&gp, 0, sizeof(gp));
}

void loop()
{
  if ( !Bluefruit.connected() ) { delay(100); return; }

  clear();
  step("centred");

  /* D-pad. Only one direction is reported at a time - the hat is a
   * value, not a bitmap, which is why there is a separate code for
   * each diagonal. */
  const uint8_t hats[] = {
    GAMEPAD_HAT_UP,   GAMEPAD_HAT_UP_RIGHT,   GAMEPAD_HAT_RIGHT, GAMEPAD_HAT_DOWN_RIGHT,
    GAMEPAD_HAT_DOWN, GAMEPAD_HAT_DOWN_LEFT,  GAMEPAD_HAT_LEFT,  GAMEPAD_HAT_UP_LEFT,
  };
  const char *hat_names[] = {
    "hat up", "hat up-right", "hat right", "hat down-right",
    "hat down", "hat down-left", "hat left", "hat up-left",
  };

  for (unsigned i = 0; i < sizeof(hats); i++)
  {
    clear();
    gp.hat = hats[i];
    step(hat_names[i]);
  }

  /* Buttons are a bitmap, so several can be held at once. */
  clear();
  gp.buttons = GAMEPAD_BUTTON_0;
  step("button 0");

  gp.buttons = GAMEPAD_BUTTON_0 | GAMEPAD_BUTTON_1;
  step("buttons 0 and 1");

  /* Sticks and triggers run -127..127, centred at 0. */
  clear();
  gp.x = 127;
  step("left stick right");

  clear();
  gp.y = -127;
  step("left stick up");

  clear();
  gp.z = 127; gp.rz = -127;
  step("right stick down-right");

  clear();
  gp.rx = 127; gp.ry = 127;
  step("both triggers");

  clear();
  step("centred");
}
