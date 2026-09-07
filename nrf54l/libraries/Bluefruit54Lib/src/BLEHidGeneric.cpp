/*
 * BLEHidGeneric — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 */
#include "bluefruit.h"
#include <string.h>

/* 리포트 종류. Report Reference 디스크립터의 두 번째 바이트다. */
enum { HID_REPORT_TYPE_INPUT = 1, HID_REPORT_TYPE_OUTPUT, HID_REPORT_TYPE_FEATURE };

static BLEHidGeneric *_hid_instance = NULL;
static write_cb_t     _output_cb[BLE_HID_MAX_REPORT] = { NULL };

BLEHidGeneric::BLEHidGeneric(uint8_t num_input, uint8_t num_output, uint8_t num_feature)
  : BLEService(UUID16_SVC_HUMAN_INTERFACE_DEVICE),
    _chr_protocol(UUID16_CHR_PROTOCOL_MODE),
    _chr_control(UUID16_CHR_HID_CONTROL_POINT),
    _chr_report_map(UUID16_CHR_REPORT_MAP),
    _chr_hid_info(UUID16_CHR_HID_INFORMATION),
    _chr_boot_kbd_in(UUID16_CHR_BOOT_KEYBOARD_INPUT_REPORT),
    _chr_boot_kbd_out(UUID16_CHR_BOOT_KEYBOARD_OUTPUT_REPORT),
    _chr_boot_mouse_in(UUID16_CHR_BOOT_MOUSE_INPUT_REPORT)
{
  _num_input   = (num_input   > BLE_HID_MAX_REPORT) ? BLE_HID_MAX_REPORT : num_input;
  _num_output  = (num_output  > BLE_HID_MAX_REPORT) ? BLE_HID_MAX_REPORT : num_output;
  _num_feature = (num_feature > BLE_HID_MAX_REPORT) ? BLE_HID_MAX_REPORT : num_feature;

  _has_keyboard = _has_mouse = false;
  _report_mode  = true;                 /* 기본은 리포트 프로토콜 */

  /* bcdHID 1.11, 국가코드 없음, RemoteWake | NormallyConnectable */
  _hid_info[0] = 0x11; _hid_info[1] = 0x01; _hid_info[2] = 0x00; _hid_info[3] = 0x02;

  _report_map = NULL;
  _report_map_len = 0;
  memset(_input_len, 0, sizeof(_input_len));
  memset(_output_len, 0, sizeof(_output_len));
  memset(_feature_len, 0, sizeof(_feature_len));

  for (uint8_t i = 0; i < BLE_HID_MAX_REPORT; i++) {
    _chr_input[i].setUuid(BLEUuid(UUID16_CHR_REPORT));
    _chr_output[i].setUuid(BLEUuid(UUID16_CHR_REPORT));
  }
}

void BLEHidGeneric::setHidInfo(uint16_t bcd, uint8_t country, uint8_t flags)
{
  _hid_info[0] = (uint8_t) bcd;
  _hid_info[1] = (uint8_t) (bcd >> 8);
  _hid_info[2] = country;
  _hid_info[3] = flags;
}

void BLEHidGeneric::setReportMap(const uint8_t *report_map, size_t len)
{
  _report_map     = report_map;
  _report_map_len = len;
}

void BLEHidGeneric::setReportLen(const uint16_t input_len[], const uint16_t output_len[],
                                 const uint16_t feature_len[])
{
  if (input_len)   memcpy(_input_len,   input_len,   _num_input   * sizeof(uint16_t));
  if (output_len)  memcpy(_output_len,  output_len,  _num_output  * sizeof(uint16_t));
  if (feature_len) memcpy(_feature_len, feature_len, _num_feature * sizeof(uint16_t));
}

void BLEHidGeneric::setOutputReportCallback(uint8_t report_id, write_cb_t fp)
{
  if (report_id == 0 || report_id > BLE_HID_MAX_REPORT) return;
  _output_cb[report_id - 1] = fp;
}

static void hid_output_dispatch(uint16_t conn_hdl, BLECharacteristic *chr,
                                uint8_t *data, uint16_t len)
{
  /* 어느 리포트인지 characteristic 포인터로 되짚는다. */
  (void) chr;
  for (uint8_t i = 0; i < BLE_HID_MAX_REPORT; i++) {
    if (_output_cb[i]) { _output_cb[i](conn_hdl, chr, data, len); return; }
  }
}

void BLEHidGeneric::protocol_mode_cb(uint16_t conn_hdl, BLECharacteristic *chr,
                                     uint8_t *data, uint16_t len)
{
  (void) conn_hdl; (void) chr;
  /*
   * 호스트가 부트/리포트 프로토콜을 고른다. 0 = boot, 1 = report.
   * 대부분의 호스트는 리포트를 쓰지만, 부트만 아는 호스트도 있다.
   */
  if (len >= 1 && _hid_instance) _hid_instance->_report_mode = (data[0] == 1);
}

err_t BLEHidGeneric::begin(void)
{
  if (_report_map == NULL || _report_map_len == 0) return NRF_ERROR_INVALID_STATE;

  _hid_instance = this;

  err_t err = BLEService::begin();
  if (err) return err;

  /* Protocol Mode — 부트 모드를 지원할 때만 의미가 있지만 늘 올려 둔다. */
  _chr_protocol.setProperties(CHR_PROPS_READ | CHR_PROPS_WRITE_WO_RESP);
  _chr_protocol.setPermission(SECMODE_ENC_NO_MITM, SECMODE_ENC_NO_MITM);
  _chr_protocol.setFixedLen(1);
  _chr_protocol.setWriteCallback(protocol_mode_cb);
  err = _chr_protocol.begin();
  if (err) return err;
  uint8_t mode = 1;
  _chr_protocol.write(&mode, 1);

  /* Report Map — 호스트가 리포트 구조를 읽어 간다. */
  _chr_report_map.setProperties(CHR_PROPS_READ);
  _chr_report_map.setPermission(SECMODE_ENC_NO_MITM, SECMODE_NO_ACCESS);
  _chr_report_map.setFixedLen(_report_map_len);
  err = _chr_report_map.begin();
  if (err) return err;
  _chr_report_map.write(_report_map, _report_map_len);

  /* 입력 리포트 — 우리가 notify 로 보낸다. */
  for (uint8_t i = 0; i < _num_input; i++) {
    _chr_input[i].setProperties(CHR_PROPS_READ | CHR_PROPS_NOTIFY);
    _chr_input[i].setPermission(SECMODE_ENC_NO_MITM, SECMODE_NO_ACCESS);
    _chr_input[i].setFixedLen(_input_len[i]);
    err = _chr_input[i].begin();
    if (err) return err;

    /*
     * ⚠ Report Reference 가 없으면 호스트가 이 리포트의 ID 와 종류를 모른다.
     *   그러면 HID 가 통째로 동작하지 않는다 — 연결은 되는데 키가 안 먹는다.
     */
    uint8_t ref[2] = { (uint8_t) (i + 1), HID_REPORT_TYPE_INPUT };
    err = _chr_input[i].addDescriptor(BLEUuid(UUID16_DESCRIPTOR_REPORT_REFERENCE),
                                      ref, sizeof(ref), SECMODE_ENC_NO_MITM);
    if (err) return err;
  }

  /* 출력 리포트 — 호스트가 쓴다 (키보드 LED 등). */
  for (uint8_t i = 0; i < _num_output; i++) {
    _chr_output[i].setProperties(CHR_PROPS_READ | CHR_PROPS_WRITE | CHR_PROPS_WRITE_WO_RESP);
    _chr_output[i].setPermission(SECMODE_ENC_NO_MITM, SECMODE_ENC_NO_MITM);
    _chr_output[i].setFixedLen(_output_len[i]);
    _chr_output[i].setWriteCallback(hid_output_dispatch);
    err = _chr_output[i].begin();
    if (err) return err;

    uint8_t ref[2] = { (uint8_t) (i + 1), HID_REPORT_TYPE_OUTPUT };
    err = _chr_output[i].addDescriptor(BLEUuid(UUID16_DESCRIPTOR_REPORT_REFERENCE),
                                       ref, sizeof(ref), SECMODE_ENC_NO_MITM);
    if (err) return err;
  }

  /* 부트 프로토콜용. 호스트가 부트 모드를 고르면 이쪽을 쓴다. */
  if (_has_keyboard) {
    _chr_boot_kbd_in.setProperties(CHR_PROPS_READ | CHR_PROPS_NOTIFY);
    _chr_boot_kbd_in.setPermission(SECMODE_ENC_NO_MITM, SECMODE_NO_ACCESS);
    _chr_boot_kbd_in.setFixedLen(sizeof(hid_keyboard_report_t));
    err = _chr_boot_kbd_in.begin();
    if (err) return err;

    _chr_boot_kbd_out.setProperties(CHR_PROPS_READ | CHR_PROPS_WRITE | CHR_PROPS_WRITE_WO_RESP);
    _chr_boot_kbd_out.setPermission(SECMODE_ENC_NO_MITM, SECMODE_ENC_NO_MITM);
    _chr_boot_kbd_out.setFixedLen(1);
    err = _chr_boot_kbd_out.begin();
    if (err) return err;
  }
  if (_has_mouse) {
    _chr_boot_mouse_in.setProperties(CHR_PROPS_READ | CHR_PROPS_NOTIFY);
    _chr_boot_mouse_in.setPermission(SECMODE_ENC_NO_MITM, SECMODE_NO_ACCESS);
    _chr_boot_mouse_in.setFixedLen(sizeof(hid_mouse_report_t));
    err = _chr_boot_mouse_in.begin();
    if (err) return err;
  }

  /* HID Control Point — 호스트가 suspend/exit-suspend 를 알린다. */
  _chr_control.setProperties(CHR_PROPS_WRITE_WO_RESP);
  _chr_control.setPermission(SECMODE_NO_ACCESS, SECMODE_ENC_NO_MITM);
  _chr_control.setFixedLen(1);
  err = _chr_control.begin();
  if (err) return err;

  /* HID Information */
  _chr_hid_info.setProperties(CHR_PROPS_READ);
  _chr_hid_info.setPermission(SECMODE_ENC_NO_MITM, SECMODE_NO_ACCESS);
  _chr_hid_info.setFixedLen(sizeof(_hid_info));
  err = _chr_hid_info.begin();
  if (err) return err;
  _chr_hid_info.write(_hid_info, sizeof(_hid_info));

  return NRF_SUCCESS;
}

bool BLEHidGeneric::inputReport(uint16_t conn_hdl, uint8_t report_id,
                                const void *data, int len)
{
  if (report_id == 0 || report_id > _num_input) return false;
  return _chr_input[report_id - 1].notify(conn_hdl, data, (uint16_t) len);
}

bool BLEHidGeneric::inputReport(uint8_t report_id, const void *data, int len)
{
  return inputReport(Bluefruit.connHandle(), report_id, data, len);
}
