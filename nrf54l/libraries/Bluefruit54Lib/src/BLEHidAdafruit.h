/*
 * BLEHidAdafruit — 키보드 / 마우스 / 미디어 키
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * BLEHidGeneric 위에 얹은 편의 계층. API 는 상류를 따른다 (R12).
 */
#ifndef _BLE_HID_ADAFRUIT_H_
#define _BLE_HID_ADAFRUIT_H_

#include "BLEHidGeneric.h"

/* 상류 예제가 쓰는 Appearance 값 (Bluetooth SIG). */
#ifndef BLE_APPEARANCE_HID_KEYBOARD
#define BLE_APPEARANCE_HID_KEYBOARD   (961)
#define BLE_APPEARANCE_HID_MOUSE      (962)
#define BLE_APPEARANCE_HID_GAMEPAD    (963)
#endif

class BLEHidAdafruit : public BLEHidGeneric
{
  public:
    typedef void (*kbd_led_cb_t)(uint16_t conn_hdl, uint8_t leds_bitmap);

    BLEHidAdafruit(void);

    virtual err_t begin(void);

    /** 호스트가 CapsLock 등 LED 상태를 보내면 불린다. */
    void setKeyboardLedCallback(kbd_led_cb_t fp);

    /* ── 키보드 ─────────────────────────────────────────────────────── */
    bool keyboardReport(hid_keyboard_report_t *report);
    bool keyboardReport(uint8_t modifier, uint8_t keycode[6]);
    bool keyboardReport(uint16_t conn_hdl, hid_keyboard_report_t *report);

    /**
     * ASCII 한 글자를 누른다. **떼려면 keyRelease() 를 불러야 한다** —
     * 안 부르면 호스트가 계속 눌린 것으로 보고 문자를 반복한다.
     */
    bool keyPress(char ch);
    bool keyRelease(void);
    bool keySequence(const char *str, int interval = 5);

    bool keyPress(uint16_t conn_hdl, char ch);
    bool keyRelease(uint16_t conn_hdl);

    /* ── 미디어 키 ──────────────────────────────────────────────────── */
    bool consumerReport(uint16_t usage_code);
    bool consumerKeyPress(uint16_t usage_code);
    bool consumerKeyRelease(void);

    /* 연결을 지정하는 판. 상류 시그니처와 같다. */
    bool consumerReport(uint16_t conn_hdl, uint16_t usage_code);
    bool consumerKeyPress(uint16_t conn_hdl, uint16_t usage_code);
    bool consumerKeyRelease(uint16_t conn_hdl);

    /* ── 마우스 ─────────────────────────────────────────────────────── */
    bool mouseReport(hid_mouse_report_t *report);
    bool mouseReport(uint8_t buttons, int8_t x, int8_t y, int8_t wheel = 0, int8_t pan = 0);
    bool mouseButtonPress(uint8_t buttons);
    bool mouseButtonRelease(void);
    bool mouseMove(int8_t x, int8_t y);
    bool mouseScroll(int8_t scroll);
    bool mousePan(int8_t pan);

  protected:
    uint8_t      _mse_buttons;
    kbd_led_cb_t _kbd_led_cb;
};

#endif
