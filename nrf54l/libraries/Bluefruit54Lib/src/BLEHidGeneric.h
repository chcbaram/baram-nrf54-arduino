/*
 * BLEHidGeneric — HID over GATT 서비스
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * API 는 Adafruit Bluefruit52Lib 을 따른다 (R12). 다만 HID 정의는 TinyUSB 가
 * 아니라 `ble_hid_defs.h` 에서 온다 (그 파일 주석 참조).
 *
 * ⚠ **HID 는 암호화된 링크를 요구한다.** 리포트 characteristic 의 읽기 권한이
 *   암호화 이상이어야 호스트가 붙는다. 본딩(BLESecurity)이 먼저 돼 있어야 한다.
 */
#ifndef _BLE_HID_GENERIC_H_
#define _BLE_HID_GENERIC_H_

#include "BLEService.h"
#include "BLECharacteristic.h"
#include "ble_hid_defs.h"

/** 리포트 개수 상한. 고정 배열이라 런타임 할당이 없다. */
#ifndef BLE_HID_MAX_REPORT
#define BLE_HID_MAX_REPORT   (4)
#endif

class BLEHidGeneric : public BLEService
{
  public:
    BLEHidGeneric(uint8_t num_input, uint8_t num_output = 0, uint8_t num_feature = 0);

    /** 부트 프로토콜용 characteristic 을 만든다. 호스트가 요구할 수 있다. */
    void enableKeyboard(bool enable) { _has_keyboard = enable; }
    void enableMouse(bool enable)    { _has_mouse = enable; }

    void setHidInfo(uint16_t bcd, uint8_t country, uint8_t flags);
    void setReportLen(const uint16_t input_len[], const uint16_t output_len[] = NULL,
                      const uint16_t feature_len[] = NULL);
    void setReportMap(const uint8_t *report_map, size_t len);

    void setOutputReportCallback(uint8_t report_id, write_cb_t fp);

    virtual err_t begin(void);

    /** 호스트가 부트 프로토콜을 골랐는가. */
    bool isBootMode(void) const { return !_report_mode; }

    bool inputReport(uint8_t report_id, const void *data, int len);
    bool inputReport(uint16_t conn_hdl, uint8_t report_id, const void *data, int len);

  protected:
    uint8_t _num_input, _num_output, _num_feature;
    bool    _has_keyboard, _has_mouse;
    bool    _report_mode;

    uint8_t  _hid_info[4];
    const uint8_t *_report_map;
    size_t   _report_map_len;

    uint16_t _input_len[BLE_HID_MAX_REPORT];
    uint16_t _output_len[BLE_HID_MAX_REPORT];
    uint16_t _feature_len[BLE_HID_MAX_REPORT];

    BLECharacteristic _chr_protocol;
    BLECharacteristic _chr_control;
    BLECharacteristic _chr_report_map;
    BLECharacteristic _chr_hid_info;

    BLECharacteristic _chr_input[BLE_HID_MAX_REPORT];
    BLECharacteristic _chr_output[BLE_HID_MAX_REPORT];

    BLECharacteristic _chr_boot_kbd_in;
    BLECharacteristic _chr_boot_kbd_out;
    BLECharacteristic _chr_boot_mouse_in;

    static void protocol_mode_cb(uint16_t conn_hdl, BLECharacteristic *chr,
                                 uint8_t *data, uint16_t len);
};

#endif
