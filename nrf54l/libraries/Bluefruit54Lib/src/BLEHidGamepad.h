/*
 * BLEHidGamepad — 게임패드 HID
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * BLEHidGeneric 위에 얹은 편의 계층. API 는 상류를 따른다 (R12).
 * BLEHidAdafruit 과 달리 리포트 맵을 **자기 것 하나만** 쓴다 — 게임패드는
 * 키보드·마우스와 같은 기기로 묶이지 않는 것이 상류 동작이다.
 */
#ifndef _BLE_HID_GAMEPAD_H_
#define _BLE_HID_GAMEPAD_H_

#include "BLEHidGeneric.h"

class BLEHidGamepad : public BLEHidGeneric
{
  public:
    BLEHidGamepad(void);

    virtual err_t begin(void);

    /* 단일 연결 — 마지막으로 연결된 링크로 보낸다. */
    bool report(hid_gamepad_report_t const* report);
    bool reportButtons(uint32_t button_mask);
    bool reportHat(uint8_t hat);
    bool reportJoystick(int8_t x, int8_t y, int8_t z, int8_t rz, int8_t rx, int8_t ry);

    /* 다중 연결 — 링크를 지정한다. */
    bool report(uint16_t conn_hdl, hid_gamepad_report_t const* report);
    bool reportButtons(uint16_t conn_hdl, uint32_t button_mask);
    bool reportHat(uint16_t conn_hdl, uint8_t hat);
    bool reportJoystick(uint16_t conn_hdl, int8_t x, int8_t y, int8_t z, int8_t rz, int8_t rx, int8_t ry);
};

#endif
