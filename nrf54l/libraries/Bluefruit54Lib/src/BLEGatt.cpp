/*
 * BLEGatt — 상대(peer) 의 GATT 서버를 읽는다 (GATT 클라이언트)
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 */
#include "bluefruit.h"
#include <string.h>

#include "FreeRTOS.h"
#include "semphr.h"

BLEGatt::BLEGatt(void)
{
  _sem      = NULL;
  _mutex    = NULL;
  _buf      = NULL;
  _bufsize  = 0;
  _len      = 0;
  _conn_hdl = BLE_CONN_HANDLE_INVALID;
  _waiting  = false;
}

bool BLEGatt::_begin(void)
{
  if (_sem == NULL) {
    _sem = (void *) xSemaphoreCreateBinary();
    if (_sem == NULL) return false;
  }
  if (_mutex == NULL) {
    _mutex = (void *) xSemaphoreCreateMutex();
    if (_mutex == NULL) return false;
  }
  return true;
}

uint16_t BLEGatt::readCharByUuid(uint16_t conn_hdl, BLEUuid bleuuid,
                                 void *buffer, uint16_t bufsize,
                                 uint16_t start_hdl, uint16_t end_hdl)
{
  if (_sem == NULL || _mutex == NULL) return 0;
  if (buffer == NULL || bufsize == 0)  return 0;
  if (!Bluefruit.connected(conn_hdl))  return 0;

  /* UUID 를 SoftDevice 에 등록해야 비교가 된다 (128비트 vendor UUID 의 경우). */
  if (!bleuuid.begin()) return 0;

  /*
   * ⚠ GATT 절차는 링크당 한 번에 하나다. 두 스레드가 겹치면 뒤엣것이
   *   NRF_ERROR_BUSY 를 받는데, 더 나쁜 건 **응답이 엉뚱한 대기자에게 간다**는
   *   것이다. 여기서 통째로 직렬화한다.
   */
  if (xSemaphoreTake((SemaphoreHandle_t) _mutex, pdMS_TO_TICKS(BLE_GATT_TIMEOUT_MS)) != pdTRUE) {
    return 0;
  }

  /* 이전 절차가 남긴 신호가 있으면 버린다 (타임아웃 뒤 늦게 온 응답 등). */
  xSemaphoreTake((SemaphoreHandle_t) _sem, 0);

  _buf      = (uint8_t *) buffer;
  _bufsize  = bufsize;
  _len      = 0;
  _conn_hdl = conn_hdl;
  _waiting  = true;

  ble_gattc_handle_range_t range;
  range.start_handle = start_hdl;
  range.end_handle   = end_hdl;

  uint16_t len = 0;
  uint32_t err = sd_ble_gattc_char_value_by_uuid_read(conn_hdl, &bleuuid._uuid, &range);

  if (err == NRF_SUCCESS) {
    /* UUID 가 핸들 범위 끝에 있으면 오래 걸린다. 넉넉히 기다린다. */
    if (xSemaphoreTake((SemaphoreHandle_t) _sem, pdMS_TO_TICKS(BLE_GATT_TIMEOUT_MS)) == pdTRUE) {
      len = _len;
    }
  }

  _waiting  = false;
  _buf      = NULL;
  _bufsize  = 0;
  _conn_hdl = BLE_CONN_HANDLE_INVALID;

  xSemaphoreGive((SemaphoreHandle_t) _mutex);
  return len;
}

void BLEGatt::_eventHandler(const ble_evt_t *evt)
{
  if (!_waiting) return;
  if (evt->evt.gattc_evt.conn_handle != _conn_hdl) return;

  switch (evt->header.evt_id) {
    case BLE_GATTC_EVT_CHAR_VAL_BY_UUID_READ_RSP: {
      const ble_gattc_evt_t *g = &evt->evt.gattc_evt;

      if (g->gatt_status == BLE_GATT_STATUS_SUCCESS) {
        /*
         * 응답은 [핸들][값] 쌍의 목록이고 값 길이가 가변이라 구조체로 바로
         * 못 읽는다. SoftDevice 가 준 반복자를 써야 한다.
         * 첫 쌍만 쓴다 — 이름은 하나뿐이다.
         */
        ble_gattc_handle_value_t iter;
        memset(&iter, 0, sizeof(iter));

        if (sd_ble_gattc_evt_char_val_by_uuid_read_rsp_iter(
                (ble_gattc_evt_t *) g, &iter) == NRF_SUCCESS) {
          uint16_t n = g->params.char_val_by_uuid_read_rsp.value_len;
          if (n > _bufsize) n = _bufsize;      /* 넘치면 자른다 */
          memcpy(_buf, iter.p_value, n);
          _len = n;
        }
      }
      /* 실패든 성공이든 깨운다. 실패면 _len 이 0 이다. */
      xSemaphoreGive((SemaphoreHandle_t) _sem);
      break;
    }

    /*
     * ⚠ 이걸 놓치면 호출한 쪽이 타임아웃까지 멈춘다. 상대가 절차를 거부하거나
     *   (권한 없음 등) 링크가 끊기면 이쪽으로 온다.
     */
    case BLE_GATTC_EVT_TIMEOUT:
      xSemaphoreGive((SemaphoreHandle_t) _sem);
      break;

    case BLE_GAP_EVT_DISCONNECTED:
      xSemaphoreGive((SemaphoreHandle_t) _sem);
      break;

    default:
      break;
  }
}
