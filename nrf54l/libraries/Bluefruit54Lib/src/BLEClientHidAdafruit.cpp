/*
 * BLEClientHidAdafruit — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 */
#include "bluefruit.h"
#include <string.h>

static void kbd_notify_cb(BLEClientCharacteristic *chr, uint8_t *data, uint16_t len)
{
  BLEClientHidAdafruit *hid = (BLEClientHidAdafruit *) chr->parentService();
  if (hid) hid->_handleKbd(data, len);
}

static void mse_notify_cb(BLEClientCharacteristic *chr, uint8_t *data, uint16_t len)
{
  BLEClientHidAdafruit *hid = (BLEClientHidAdafruit *) chr->parentService();
  if (hid) hid->_handleMse(data, len);
}

static void gpd_notify_cb(BLEClientCharacteristic *chr, uint8_t *data, uint16_t len)
{
  BLEClientHidAdafruit *hid = (BLEClientHidAdafruit *) chr->parentService();
  if (hid) hid->_handleGpd(data, len);
}

BLEClientHidAdafruit::BLEClientHidAdafruit(void)
  : BLEClientService(BLEUuid(UUID16_SVC_HUMAN_INTERFACE_DEVICE)),
    _protocol_mode(BLEUuid(UUID16_CHR_PROTOCOL_MODE)),
    _hid_info(BLEUuid(UUID16_CHR_HID_INFORMATION)),
    _hid_control(BLEUuid(UUID16_CHR_HID_CONTROL_POINT)),
    _kbd_boot_input(BLEUuid(UUID16_CHR_BOOT_KEYBOARD_INPUT_REPORT)),
    _kbd_boot_output(BLEUuid(UUID16_CHR_BOOT_KEYBOARD_OUTPUT_REPORT)),
    _mse_boot_input(BLEUuid(UUID16_CHR_BOOT_MOUSE_INPUT_REPORT)),
    _gpd_report(BLEUuid(UUID16_CHR_REPORT))
{
  _kbd_cb = NULL;
  _mse_cb = NULL;
  _gpd_cb = NULL;

  memset(&_last_kbd, 0, sizeof(_last_kbd));
  memset(&_last_mse, 0, sizeof(_last_mse));
  memset(&_last_gpd, 0, sizeof(_last_gpd));
}

bool BLEClientHidAdafruit::begin(void)
{
  if (!BLEClientService::begin()) return false;

  _protocol_mode.begin(this);
  _hid_info.begin(this);
  _hid_control.begin(this);
  _kbd_boot_input.begin(this);
  _kbd_boot_output.begin(this);
  _mse_boot_input.begin(this);
  _gpd_report.begin(this);

  _kbd_boot_input.setNotifyCallback(kbd_notify_cb);
  _mse_boot_input.setNotifyCallback(mse_notify_cb);
  _gpd_report.setNotifyCallback(gpd_notify_cb);
  return true;
}

bool BLEClientHidAdafruit::discover(uint16_t conn_hdl)
{
  if (!BLEClientService::discover(conn_hdl)) return false;

  discoverCharacteristics();

  /* HID Information 과 Control Point 는 규격상 필수다. */
  if (!_hid_info.discovered() || !_hid_control.discovered()) {
    _disconnected();
    return false;
  }

  /* 셋 중 하나라도 있어야 쓸모가 있다. */
  if (!keyboardPresent() && !mousePresent() && !gamepadPresent()) {
    _disconnected();
    return false;
  }
  return true;
}

void BLEClientHidAdafruit::setKeyboardReportCallback(kbd_callback_t fp) { _kbd_cb = fp; }
void BLEClientHidAdafruit::setMouseReportCallback(mse_callback_t fp)    { _mse_cb = fp; }
void BLEClientHidAdafruit::setGamepadReportCallback(gpd_callback_t fp)  { _gpd_cb = fp; }

/*------------------------------------------------------------------*/
/* 정보                                                              */
/*------------------------------------------------------------------*/

bool BLEClientHidAdafruit::getHidInfo(uint8_t info[4])
{
  return _hid_info.read(info, 4) == 4;
}

uint8_t BLEClientHidAdafruit::getCountryCode(void)
{
  uint8_t info[4] = { 0 };
  if (!getHidInfo(info)) return 0;
  return info[2];
}

bool BLEClientHidAdafruit::setBootMode(bool boot)
{
  /* 규격 값이 뒤집혀 있다 — 0 이 부트, 1 이 리포트다. */
  return _protocol_mode.write8(boot ? 0 : 1) > 0;
}

/*------------------------------------------------------------------*/
/* 키보드                                                            */
/*------------------------------------------------------------------*/

bool BLEClientHidAdafruit::keyboardPresent(void)
{
  return _kbd_boot_input.discovered() && _kbd_boot_output.discovered() &&
         _protocol_mode.discovered();
}

bool BLEClientHidAdafruit::enableKeyboard(void)  { return _kbd_boot_input.enableNotify(); }
bool BLEClientHidAdafruit::disableKeyboard(void) { return _kbd_boot_input.disableNotify(); }

void BLEClientHidAdafruit::_handleKbd(const uint8_t *data, uint16_t len)
{
  /* 길이를 자른다 — 상대가 규격보다 긴 값을 보내면 구조체 뒤를 넘어 쓴다. */
  if (len > sizeof(_last_kbd)) len = sizeof(_last_kbd);
  memset(&_last_kbd, 0, sizeof(_last_kbd));
  memcpy(&_last_kbd, data, len);

  if (_kbd_cb) _kbd_cb(&_last_kbd);
}

void BLEClientHidAdafruit::getKeyboardReport(hid_keyboard_report_t *report)
{
  if (report) memcpy(report, &_last_kbd, sizeof(_last_kbd));
}

/*------------------------------------------------------------------*/
/* 마우스                                                            */
/*------------------------------------------------------------------*/

bool BLEClientHidAdafruit::mousePresent(void)
{
  return _mse_boot_input.discovered() && _protocol_mode.discovered();
}

bool BLEClientHidAdafruit::enableMouse(void)  { return _mse_boot_input.enableNotify(); }
bool BLEClientHidAdafruit::disableMouse(void) { return _mse_boot_input.disableNotify(); }

void BLEClientHidAdafruit::_handleMse(const uint8_t *data, uint16_t len)
{
  if (len > sizeof(_last_mse)) len = sizeof(_last_mse);
  memset(&_last_mse, 0, sizeof(_last_mse));
  memcpy(&_last_mse, data, len);

  if (_mse_cb) _mse_cb(&_last_mse);
}

void BLEClientHidAdafruit::getMouseReport(hid_mouse_report_t *report)
{
  if (report) memcpy(report, &_last_mse, sizeof(_last_mse));
}

/*------------------------------------------------------------------*/
/* 게임패드                                                          */
/*------------------------------------------------------------------*/

bool BLEClientHidAdafruit::gamepadPresent(void) { return _gpd_report.discovered(); }

bool BLEClientHidAdafruit::enableGamepad(void)  { return _gpd_report.enableNotify(); }
bool BLEClientHidAdafruit::disableGamepad(void) { return _gpd_report.disableNotify(); }

void BLEClientHidAdafruit::_handleGpd(const uint8_t *data, uint16_t len)
{
  if (len > sizeof(_last_gpd)) len = sizeof(_last_gpd);
  memset(&_last_gpd, 0, sizeof(_last_gpd));
  memcpy(&_last_gpd, data, len);

  if (_gpd_cb) _gpd_cb(&_last_gpd);
}

void BLEClientHidAdafruit::getGamepadReport(hid_gamepad_report_t *report)
{
  if (report) memcpy(report, &_last_gpd, sizeof(_last_gpd));
}
