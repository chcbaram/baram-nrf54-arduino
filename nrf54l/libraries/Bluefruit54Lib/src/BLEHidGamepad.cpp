/*
 * BLEHidGamepad — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 */
#include "bluefruit.h"
#include <string.h>

enum { REPORT_ID_GAMEPAD = 1 };

/*
 * 게임패드 리포트 맵.
 *
 * 상류(Adafruit)는 TinyUSB 의 `TUD_HID_REPORT_DESC_GAMEPAD()` 매크로를 쓰지만
 * 우리는 TinyUSB 를 싣지 않으므로(ble_hid_defs.h 머리말) 같은 바이트열을 직접
 * 적는다. 매크로가 펼쳐지는 결과와 동일하다.
 *
 * 필드 합이 6×8 + 8 + 32 = 88 비트 = 11 바이트이고, 이는
 * `sizeof(hid_gamepad_report_t)` 와 같아야 한다 (아래 static_assert).
 */
static const uint8_t hid_gamepad_report_descriptor[] = {
0x05, 0x01,             /* Usage Page (Generic Desktop)     */
0x09, 0x05,             /* Usage (Gamepad)                  */
0xA1, 0x01,             /* Collection (Application)         */
  0x85, REPORT_ID_GAMEPAD,
  /* 축 6개 — X, Y, Z, Rz, Rx, Ry. 각 8비트 부호 있음 (-127~127) */
  0x05, 0x01,           /*   Usage Page (Generic Desktop)   */
  0x09, 0x30,           /*   Usage (X)                      */
  0x09, 0x31,           /*   Usage (Y)                      */
  0x09, 0x32,           /*   Usage (Z)                      */
  0x09, 0x35,           /*   Usage (Rz)                     */
  0x09, 0x33,           /*   Usage (Rx)                     */
  0x09, 0x34,           /*   Usage (Ry)                     */
  0x15, 0x81,           /*   Logical Minimum (-127)         */
  0x25, 0x7F,           /*   Logical Maximum (127)          */
  0x95, 0x06,           /*   Report Count (6)               */
  0x75, 0x08,           /*   Report Size (8)                */
  0x81, 0x02,           /*   Input (Data, Var, Abs)         */
  /* 방향 패드 — 8방향 + 중립. 물리 범위를 각도(0~315°)로 준다 */
  0x05, 0x01,           /*   Usage Page (Generic Desktop)   */
  0x09, 0x39,           /*   Usage (Hat switch)             */
  0x15, 0x01,           /*   Logical Minimum (1)            */
  0x25, 0x08,           /*   Logical Maximum (8)            */
  0x35, 0x00,           /*   Physical Minimum (0)           */
  0x46, 0x3B, 0x01,     /*   Physical Maximum (315)         */
  0x95, 0x01,           /*   Report Count (1)               */
  0x75, 0x08,           /*   Report Size (8)                */
  0x81, 0x02,           /*   Input (Data, Var, Abs)         */
  /* 버튼 32개 — 1비트씩 */
  0x05, 0x09,           /*   Usage Page (Button)            */
  0x19, 0x01,           /*   Usage Minimum (Button 1)       */
  0x29, 0x20,           /*   Usage Maximum (Button 32)      */
  0x15, 0x00,           /*   Logical Minimum (0)            */
  0x25, 0x01,           /*   Logical Maximum (1)            */
  0x95, 0x20,           /*   Report Count (32)              */
  0x75, 0x01,           /*   Report Size (1)                */
  0x81, 0x02,           /*   Input (Data, Var, Abs)         */
0xC0                    /* End Collection                   */
};

/* 맵과 구조체가 어긋나면 호스트가 값을 엉뚱한 자리에서 읽는다. 빌드에서 막는다. */
static_assert(sizeof(hid_gamepad_report_t) == 11,
              "gamepad report must stay 11 bytes to match the report map");

BLEHidGamepad::BLEHidGamepad(void)
  : BLEHidGeneric(1, 0, 0)
{
}

err_t BLEHidGamepad::begin(void)
{
  const uint16_t input_len[] = { sizeof(hid_gamepad_report_t) };

  setReportLen(input_len, NULL, NULL);
  enableKeyboard(false);          /* boot 키보드 characteristic 을 만들지 않는다 */
  enableMouse(false);
  setReportMap(hid_gamepad_report_descriptor, sizeof(hid_gamepad_report_descriptor));

  err_t err = BLEHidGeneric::begin();
  if (err) return err;

  /* 입력 지연을 줄이려고 11.25~15 ms 를 요청한다. 결정은 호스트가 한다. */
  Bluefruit.Periph.setConnInterval(9, 12);
  return NRF_SUCCESS;
}

/*------------------------------------------------------------------*/
/* 다중 연결                                                         */
/*------------------------------------------------------------------*/

bool BLEHidGamepad::report(uint16_t conn_hdl, hid_gamepad_report_t const* report)
{
  return inputReport(conn_hdl, REPORT_ID_GAMEPAD, report, sizeof(hid_gamepad_report_t));
}

bool BLEHidGamepad::reportButtons(uint16_t conn_hdl, uint32_t button_mask)
{
  hid_gamepad_report_t report;
  memset(&report, 0, sizeof(report));
  report.buttons = button_mask;

  return this->report(conn_hdl, &report);
}

bool BLEHidGamepad::reportHat(uint16_t conn_hdl, uint8_t hat)
{
  hid_gamepad_report_t report;
  memset(&report, 0, sizeof(report));
  report.hat = hat;

  return this->report(conn_hdl, &report);
}

bool BLEHidGamepad::reportJoystick(uint16_t conn_hdl, int8_t x, int8_t y, int8_t z,
                                   int8_t rz, int8_t rx, int8_t ry)
{
  hid_gamepad_report_t report;
  memset(&report, 0, sizeof(report));
  report.x  = x;   report.y  = y;
  report.z  = z;   report.rz = rz;
  report.rx = rx;  report.ry = ry;

  return this->report(conn_hdl, &report);
}

/*------------------------------------------------------------------*/
/* 단일 연결                                                         */
/*------------------------------------------------------------------*/
/*
 * ⚠ 상류는 여기서 BLE_CONN_HANDLE_INVALID 를 내려보내고 그 아래에서 실제
 *   핸들로 바꾼다. 우리 BLECharacteristic::notify() 는 그 치환을 하지 않고
 *   `Bluefruit.connected(conn)` 에서 바로 걸러 내므로 **여기서 핸들을 정해야
 *   한다.** BLEHidAdafruit 도 같은 방식이다.
 *
 *   그대로 옮겼다가 실기에서 리포트가 하나도 안 나갔다. 연결·페어링·리포트
 *   맵이 모두 정상이라 호스트 문제로 보였고, 원인을 찾는 데 오래 걸렸다.
 */

bool BLEHidGamepad::report(hid_gamepad_report_t const* report)
{
  return this->report(Bluefruit.connHandle(), report);
}

bool BLEHidGamepad::reportButtons(uint32_t button_mask)
{
  return this->reportButtons(Bluefruit.connHandle(), button_mask);
}

bool BLEHidGamepad::reportHat(uint8_t hat)
{
  return this->reportHat(Bluefruit.connHandle(), hat);
}

bool BLEHidGamepad::reportJoystick(int8_t x, int8_t y, int8_t z, int8_t rz, int8_t rx, int8_t ry)
{
  return this->reportJoystick(Bluefruit.connHandle(), x, y, z, rz, rx, ry);
}
