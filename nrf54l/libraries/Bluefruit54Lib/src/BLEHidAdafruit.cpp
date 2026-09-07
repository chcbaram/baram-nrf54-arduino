/*
 * BLEHidAdafruit — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 */
#include "bluefruit.h"
#include <string.h>

static BLEHidAdafruit::kbd_led_cb_t _user_led_cb = NULL;

static void hid_kbd_led_cb(uint16_t conn_hdl, BLECharacteristic *chr,
                           uint8_t *data, uint16_t len)
{
  (void) chr;
  if (_user_led_cb && len >= 1) _user_led_cb(conn_hdl, data[0]);
}

BLEHidAdafruit::BLEHidAdafruit(void)
  : BLEHidGeneric(3, 1, 0)      /* 입력 3(키보드/소비자/마우스), 출력 1(LED) */
{
  _mse_buttons = 0;
  _kbd_led_cb  = NULL;
}

err_t BLEHidAdafruit::begin(void)
{
  const uint16_t input_len[]  = { sizeof(hid_keyboard_report_t), 2, sizeof(hid_mouse_report_t) };
  const uint16_t output_len[] = { 1 };

  setReportLen(input_len, output_len, NULL);
  enableKeyboard(true);
  enableMouse(true);
  setReportMap(hid_report_descriptor, hid_report_descriptor_len);

  err_t err = BLEHidGeneric::begin();
  if (err) return err;

  /*
   * 키 입력이 늦으면 체감이 나쁘다. 연결 간격을 11.25~15 ms 로 요청한다.
   * ⚠ 요청일 뿐이고 최종 결정은 호스트가 한다.
   */
  Bluefruit.Periph.setConnInterval(9, 12);
  return NRF_SUCCESS;
}

void BLEHidAdafruit::setKeyboardLedCallback(kbd_led_cb_t fp)
{
  _kbd_led_cb  = fp;
  _user_led_cb = fp;
  setOutputReportCallback(REPORT_ID_KEYBOARD, fp ? hid_kbd_led_cb : NULL);
}

/* ── 키보드 ─────────────────────────────────────────────────────────── */

bool BLEHidAdafruit::keyboardReport(uint16_t conn_hdl, hid_keyboard_report_t *report)
{
  return inputReport(conn_hdl, REPORT_ID_KEYBOARD, report, sizeof(hid_keyboard_report_t));
}

bool BLEHidAdafruit::keyboardReport(hid_keyboard_report_t *report)
{
  return keyboardReport(Bluefruit.connHandle(), report);
}

bool BLEHidAdafruit::keyboardReport(uint8_t modifier, uint8_t keycode[6])
{
  hid_keyboard_report_t r;
  memset(&r, 0, sizeof(r));
  r.modifier = modifier;
  if (keycode) memcpy(r.keycode, keycode, 6);
  return keyboardReport(&r);
}

bool BLEHidAdafruit::keyPress(uint16_t conn_hdl, char ch)
{
  hid_keyboard_report_t r;
  memset(&r, 0, sizeof(r));

  /* 표에 없는 문자는 보낼 수 없다. 조용히 아무 키나 보내지 않는다. */
  if ((uint8_t) ch >= 128) return false;
  uint8_t code = hid_ascii_to_keycode[(uint8_t) ch][1];
  if (code == 0) return false;

  r.modifier   = hid_ascii_to_keycode[(uint8_t) ch][0] ? KEYBOARD_MODIFIER_LEFTSHIFT : 0;
  r.keycode[0] = code;
  return keyboardReport(conn_hdl, &r);
}

bool BLEHidAdafruit::keyRelease(uint16_t conn_hdl)
{
  hid_keyboard_report_t r;
  memset(&r, 0, sizeof(r));      /* 전부 0 = 아무 키도 안 눌림 */
  return keyboardReport(conn_hdl, &r);
}

bool BLEHidAdafruit::keyPress(char ch)   { return keyPress(Bluefruit.connHandle(), ch); }
bool BLEHidAdafruit::keyRelease(void)    { return keyRelease(Bluefruit.connHandle()); }

bool BLEHidAdafruit::keySequence(const char *str, int interval)
{
  if (str == NULL) return false;

  while (*str) {
    /*
     * ⚠ 같은 글자가 이어지면 누름-뗌을 확실히 나눠야 한다. 안 그러면 호스트가
     *   "계속 눌려 있다" 로 보고 한 번만 입력한다. 그래서 매 글자 뒤에 뗀다.
     */
    if (!keyPress(*str)) return false;
    delay(interval);
    if (!keyRelease()) return false;
    delay(interval);
    str++;
  }
  return true;
}

/* ── 미디어 키 ──────────────────────────────────────────────────────── */

bool BLEHidAdafruit::consumerReport(uint16_t conn_hdl, uint16_t usage_code)
{
  return inputReport(conn_hdl, REPORT_ID_CONSUMER_CONTROL, &usage_code, 2);
}

bool BLEHidAdafruit::consumerReport(uint16_t usage_code)
{
  return consumerReport(Bluefruit.connHandle(), usage_code);
}

bool BLEHidAdafruit::consumerKeyPress(uint16_t conn_hdl, uint16_t usage_code)
{
  return consumerReport(conn_hdl, usage_code);
}

/* 0 을 보내는 것이 "뗌" 이다. 안 보내면 호스트가 계속 눌린 것으로 본다. */
bool BLEHidAdafruit::consumerKeyRelease(uint16_t conn_hdl) { return consumerReport(conn_hdl, 0); }

bool BLEHidAdafruit::consumerKeyPress(uint16_t usage_code) { return consumerReport(usage_code); }
bool BLEHidAdafruit::consumerKeyRelease(void)              { return consumerReport(0); }

/* ── 마우스 ─────────────────────────────────────────────────────────── */

bool BLEHidAdafruit::mouseReport(hid_mouse_report_t *report)
{
  return inputReport(REPORT_ID_MOUSE, report, sizeof(hid_mouse_report_t));
}

bool BLEHidAdafruit::mouseReport(uint8_t buttons, int8_t x, int8_t y, int8_t wheel, int8_t pan)
{
  hid_mouse_report_t r = { buttons, x, y, wheel, pan };
  _mse_buttons = buttons;
  return mouseReport(&r);
}

bool BLEHidAdafruit::mouseButtonPress(uint8_t buttons) { return mouseReport(buttons, 0, 0, 0, 0); }
bool BLEHidAdafruit::mouseButtonRelease(void)          { return mouseReport(0, 0, 0, 0, 0); }

/* 버튼 상태를 유지한 채 움직인다 — 드래그가 가능해야 한다. */
bool BLEHidAdafruit::mouseMove(int8_t x, int8_t y)  { return mouseReport(_mse_buttons, x, y, 0, 0); }
bool BLEHidAdafruit::mouseScroll(int8_t scroll)     { return mouseReport(_mse_buttons, 0, 0, scroll, 0); }
bool BLEHidAdafruit::mousePan(int8_t pan)           { return mouseReport(_mse_buttons, 0, 0, 0, pan); }
