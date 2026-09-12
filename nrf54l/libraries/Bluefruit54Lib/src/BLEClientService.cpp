/*
 * BLEClientService — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 */
#include "bluefruit.h"
#include <string.h>

BLEClientService *BLEClientService::lastService = NULL;

BLEClientService::BLEClientService(void) : uuid()
{
  _conn_hdl = BLE_CONN_HANDLE_INVALID;
  _hdl_range.start_handle = 1;
  _hdl_range.end_handle   = 0xFFFF;
  _char_count = 0;
  memset(_chars, 0, sizeof(_chars));
}

BLEClientService::BLEClientService(BLEUuid bleuuid) : uuid(bleuuid)
{
  _conn_hdl = BLE_CONN_HANDLE_INVALID;
  _hdl_range.start_handle = 1;
  _hdl_range.end_handle   = 0xFFFF;
  _char_count = 0;
  memset(_chars, 0, sizeof(_chars));
}

bool BLEClientService::begin(void)
{
  /* 128비트 UUID 는 SoftDevice 에 등록돼야 탐색 때 비교가 된다. */
  if (!uuid.begin()) return false;

  lastService = this;
  return Bluefruit._registerClientService(this);
}

bool BLEClientService::_registerChar(BLEClientCharacteristic *chr)
{
  if (_char_count >= BLE_CLIENT_CHAR_MAX) return false;

  for (uint8_t i = 0; i < _char_count; i++) {
    if (_chars[i] == chr) return true;
  }
  _chars[_char_count++] = chr;
  return true;
}

bool BLEClientService::discover(uint16_t conn_hdl)
{
  _conn_hdl = BLE_CONN_HANDLE_INVALID;

  uint16_t start = 0, end = 0;
  if (!Bluefruit.Gatt.discoverService(conn_hdl, uuid, &start, &end)) return false;

  _hdl_range.start_handle = start;
  _hdl_range.end_handle   = end;
  _conn_hdl = conn_hdl;
  return true;
}

uint8_t BLEClientService::discoverCharacteristics(void)
{
  if (!discovered() || _char_count == 0) return 0;

  /*
   * 서비스 범위를 **한 번만** 훑고 UUID 로 나눠 준다.
   * characteristic 마다 따로 훑으면 DIS 처럼 특성이 많을 때 그 수만큼 왕복한다.
   */
  ble_gattc_char_t found[BLE_CLIENT_CHAR_MAX];
  uint8_t  matched = 0;
  uint16_t next    = _hdl_range.start_handle;

  /*
   * ⚠ 한 번에 BLE_CLIENT_CHAR_MAX 개까지만 담을 수 있으므로 **범위를 나눠 여러 번**
   *   훑는다. HID 처럼 characteristic 이 10개가 넘는 서비스가 실제로 있고,
   *   한 번만 부르면 뒤쪽이 조용히 빠진다 — 부트 키보드 리포트가 그래서 안 잡혔다.
   *   배열을 키우는 대신 나눠 도는 쪽을 골랐다: 스택에 들고 있는 크기가 서비스
   *   크기에 따라 늘지 않는다.
   */
  while (next <= _hdl_range.end_handle) {
  uint8_t n = Bluefruit.Gatt.discoverChars(_conn_hdl,
                                           next,
                                           _hdl_range.end_handle,
                                           found, BLE_CLIENT_CHAR_MAX);
  if (n == 0) break;

  for (uint8_t i = 0; i < _char_count; i++) {
    BLEClientCharacteristic *chr = _chars[i];
    if (chr == NULL) continue;

    for (uint8_t j = 0; j < n; j++) {
      /*
       * ⚠ UUID 로 골라야 한다. 탐색 응답의 **순서로 고르면 안 된다** —
       *   규격이 순서를 정해 두지 않아 구현마다 다르다.
       */
      if (found[j].uuid.type != chr->uuid._uuid.type) continue;
      if (found[j].uuid.uuid != chr->uuid._uuid.uuid) continue;

      chr->_assign(&found[j]);

      /* 알림을 쓸 특성이면 CCCD 도 찾아 둔다. 없으면 알림을 못 켠다. */
      if (found[j].char_props.notify || found[j].char_props.indicate) {
        chr->_setCccd(Bluefruit.Gatt.discoverCccd(_conn_hdl,
                                                  found[j].handle_value,
                                                  _hdl_range.end_handle));
      }
      matched++;
      break;
    }
  }

    uint16_t last = found[n - 1].handle_value;
    if (last >= _hdl_range.end_handle) break;
    next = (uint16_t) (last + 1);
  }
  return matched;
}

void BLEClientService::_disconnected(void)
{
  _conn_hdl = BLE_CONN_HANDLE_INVALID;
  for (uint8_t i = 0; i < _char_count; i++) {
    if (_chars[i]) _chars[i]->_disconnected();
  }
}

void BLEClientService::_eventHandler(const ble_evt_t *evt)
{
  if (evt->header.evt_id == BLE_GAP_EVT_DISCONNECTED) {
    if (evt->evt.gap_evt.conn_handle == _conn_hdl) _disconnected();
    return;
  }

  if (!discovered()) return;
  if (evt->evt.gattc_evt.conn_handle != _conn_hdl) return;

  for (uint8_t i = 0; i < _char_count; i++) {
    if (_chars[i]) _chars[i]->_eventHandler(evt);
  }
}
