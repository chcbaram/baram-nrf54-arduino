/*********************************************************************
 Read a BLE keyboard or mouse as a central.

 This is the other side of blehid_keyboard: instead of being the
 keyboard, the board connects to one and prints what it sends. Point it
 at a BLE keyboard, or at a second board running a HID sketch.

 It speaks the HID boot protocol, the fixed report layout a BIOS would
 use, so there is no report map to interpret. That is also its limit -
 media keys have no place in the boot protocol and never arrive.

 baram-nrf54l-arduino - MIT license
*********************************************************************/
#include <bluefruit.h>

BLEClientHidAdafruit clientHid;

void setup()
{
  Serial.begin(115200);
  Serial.println("BLE central HID example");

  Bluefruit.begin(0, 1);          // no peripheral links, one central link
  Bluefruit.setTxPower(4);
  Bluefruit.setName("BARAM Central");

  clientHid.begin();
  clientHid.setKeyboardReportCallback(keyboard_callback);
  clientHid.setMouseReportCallback(mouse_callback);

  Bluefruit.Central.setConnectCallback(connect_callback);
  Bluefruit.Central.setDisconnectCallback(disconnect_callback);

  /* Only bother with peers that advertise the HID service. */
  Bluefruit.Scanner.setRxCallback(scan_callback);
  Bluefruit.Scanner.filterService(clientHid);
  Bluefruit.Scanner.useActiveScan(false);
  Bluefruit.Scanner.start(0);

  Serial.println("Scanning for a HID peripheral");
}

void scan_callback(ble_gap_evt_adv_report_t *report)
{
  Bluefruit.Central.connect(report);
}

void connect_callback(uint16_t conn_handle)
{
  Serial.println("Connected, looking for HID");

  if ( !clientHid.discover(conn_handle) )
  {
    Serial.println("  no HID service - dropping");
    Bluefruit.disconnect(conn_handle);
    return;
  }

  /* The peer only fills the boot characteristics once it is told to
   * use the boot protocol; in report mode it sends on the report
   * characteristic instead and nothing arrives here. */
  clientHid.setBootMode(true);

  if ( clientHid.keyboardPresent() ) { clientHid.enableKeyboard(); Serial.println("  keyboard"); }
  if ( clientHid.mousePresent() )    { clientHid.enableMouse();    Serial.println("  mouse"); }
}

void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  Serial.printf("Disconnected, reason 0x%02X\n", reason);
}

/* Runs on the BLE event task - keep it short and do not call back into
 * GATT from here. */
void keyboard_callback(hid_keyboard_report_t *report)
{
  Serial.print("keys");

  for (int i = 0; i < 6; i++)
  {
    uint8_t kc = report->keycode[i];
    if ( !kc ) continue;

    Serial.printf(" 0x%02X", kc);

    if ( kc < 128 )
    {
      bool shift = (report->modifier & (0x02 | 0x20)) != 0;   // either shift
      uint8_t ch = hid_keycode_to_ascii[kc][shift ? 1 : 0];
      if ( ch >= ' ' ) Serial.printf("('%c')", ch);
    }
  }
  if ( report->modifier ) Serial.printf("  mod=0x%02X", report->modifier);
  Serial.println();
}

void mouse_callback(hid_mouse_report_t *report)
{
  Serial.printf("mouse buttons=0x%02X x=%d y=%d\n",
                report->buttons, report->x, report->y);
}

void loop()
{
  delay(1000);
}
