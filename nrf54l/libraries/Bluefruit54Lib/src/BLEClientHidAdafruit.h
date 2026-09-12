/*
 * BLEClientHidAdafruit — 상대 HID 기기의 입력을 받는다
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * `BLEHidAdafruit` 의 반대쪽이다 — 보드가 central 로 BLE 키보드·마우스에 붙어
 * 키 입력을 받는다.
 *
 * ⚠ **부트 프로토콜만 쓴다** (상류도 같다). 리포트 맵을 해석하지 않고 규격이
 *   자리를 정해 둔 Boot Input Report(0x2A22 / 0x2A33)를 구독한다. 그래서
 *   상대가 부트 모드를 지원해야 한다 — 대부분의 상용 키보드는 지원한다.
 *   Consumer Control(미디어 키)은 부트 프로토콜에 없어 받을 수 없다.
 *
 * 게임패드만 예외로 일반 Report(0x2A4D)를 본다. 부트 프로토콜에 게임패드가 없다.
 *
 * API 는 상류를 따른다 (R12).
 */
#ifndef _BLE_CLIENT_HID_ADAFRUIT_H_
#define _BLE_CLIENT_HID_ADAFRUIT_H_

#include "BLEClientService.h"
#include "BLEClientCharacteristic.h"
#include "ble_hid_defs.h"

class BLEClientHidAdafruit : public BLEClientService
{
  public:
    typedef void (*kbd_callback_t)(hid_keyboard_report_t *report);
    typedef void (*mse_callback_t)(hid_mouse_report_t *report);
    typedef void (*gpd_callback_t)(hid_gamepad_report_t *report);

    BLEClientHidAdafruit(void);

    virtual bool begin(void);
    virtual bool discover(uint16_t conn_hdl);

    /** HID Information 4바이트 (버전, 국가 코드, 플래그). */
    bool    getHidInfo(uint8_t info[4]);
    uint8_t getCountryCode(void);

    /** true = 부트, false = 리포트. 상대에게 프로토콜을 지정한다. */
    bool setBootMode(bool boot);

    /* 키보드 */
    bool keyboardPresent(void);
    bool enableKeyboard(void);
    bool disableKeyboard(void);
    void getKeyboardReport(hid_keyboard_report_t *report);

    /* 마우스 */
    bool mousePresent(void);
    bool enableMouse(void);
    bool disableMouse(void);
    void getMouseReport(hid_mouse_report_t *report);

    /* 게임패드 */
    bool gamepadPresent(void);
    bool enableGamepad(void);
    bool disableGamepad(void);
    void getGamepadReport(hid_gamepad_report_t *report);

    /**
     * 입력이 올 때 부를 함수.
     *
     * ⚠ 이 콜백은 **이벤트 태스크**에서 돈다. 안에서 GATT 를 다시 부르면
     *   (읽기·쓰기·탐색) 막힌다 — 그때는 플래그만 세우고 `loop()` 에서 처리하라.
     */
    void setKeyboardReportCallback(kbd_callback_t fp);
    void setMouseReportCallback(mse_callback_t fp);
    void setGamepadReportCallback(gpd_callback_t fp);

    /* 내부용 — 정적 콜백이 부른다. */
    void _handleKbd(const uint8_t *data, uint16_t len);
    void _handleMse(const uint8_t *data, uint16_t len);
    void _handleGpd(const uint8_t *data, uint16_t len);

  protected:
    BLEClientCharacteristic _protocol_mode;
    BLEClientCharacteristic _hid_info;
    BLEClientCharacteristic _hid_control;
    BLEClientCharacteristic _kbd_boot_input;
    BLEClientCharacteristic _kbd_boot_output;
    BLEClientCharacteristic _mse_boot_input;
    BLEClientCharacteristic _gpd_report;

    kbd_callback_t _kbd_cb;
    mse_callback_t _mse_cb;
    gpd_callback_t _gpd_cb;

    hid_keyboard_report_t _last_kbd;
    hid_mouse_report_t    _last_mse;
    hid_gamepad_report_t  _last_gpd;
};

#endif
