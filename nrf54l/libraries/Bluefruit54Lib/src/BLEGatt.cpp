/*
 * BLEGatt — 상대(peer) 의 GATT 서버를 읽고 훑는다 (GATT 클라이언트)
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * ⚠ 여기 공개 함수는 전부 **블로킹**이다. 응답 이벤트를 기다리므로
 *   BLE 이벤트 태스크에서 부르면 안 된다. 연결 콜백은 별도 태스크에서
 *   돌기 때문에 거기서는 안전하다 (bluefruit.cpp 의 콜백 태스크 주석 참조).
 */
#include "bluefruit.h"
#include <string.h>

#include "FreeRTOS.h"
#include "semphr.h"

BLEGatt::BLEGatt(void)
{
  _sem      = NULL;
  _mutex    = NULL;
  _proc     = PROC_NONE;
  _conn_hdl = BLE_CONN_HANDLE_INVALID;
  _buf      = NULL;
  _bufsize  = 0;
  _len      = 0;
  _srvc_start = 0;
  _srvc_end   = 0;
  _chars      = NULL;
  _char_max   = 0;
  _char_count = 0;
  _cccd_hdl   = 0;
  _mtu        = 0;
  _ok         = false;
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

/*
 * ⚠ GATT 절차는 링크당 한 번에 하나다. 두 스레드가 겹치면 뒤엣것이
 *   NRF_ERROR_BUSY 를 받는데, 더 나쁜 건 **응답이 엉뚱한 대기자에게 간다**는
 *   것이다. 그래서 시작~완료를 통째로 뮤텍스로 감싼다.
 */
bool BLEGatt::beginProc(uint8_t proc, uint16_t conn_hdl)
{
  if (_sem == NULL || _mutex == NULL)  return false;
  if (!Bluefruit.connected(conn_hdl))  return false;

  if (xSemaphoreTake((SemaphoreHandle_t) _mutex, pdMS_TO_TICKS(BLE_GATT_TIMEOUT_MS)) != pdTRUE) {
    return false;
  }

  /* 이전 절차가 타임아웃 뒤에 남긴 신호가 있으면 버린다. */
  xSemaphoreTake((SemaphoreHandle_t) _sem, 0);

  _proc     = proc;
  _conn_hdl = conn_hdl;
  _ok       = false;
  _len      = 0;
  _char_count = 0;
  _cccd_hdl = 0;
  return true;
}

bool BLEGatt::waitProc(void)
{
  return xSemaphoreTake((SemaphoreHandle_t) _sem, pdMS_TO_TICKS(BLE_GATT_TIMEOUT_MS)) == pdTRUE;
}

void BLEGatt::endProc(void)
{
  _proc     = PROC_NONE;
  _conn_hdl = BLE_CONN_HANDLE_INVALID;
  _buf      = NULL;
  _bufsize  = 0;
  _chars    = NULL;
  _char_max = 0;
  xSemaphoreGive((SemaphoreHandle_t) _mutex);
}

uint16_t BLEGatt::readCharByUuid(uint16_t conn_hdl, BLEUuid bleuuid,
                                 void *buffer, uint16_t bufsize,
                                 uint16_t start_hdl, uint16_t end_hdl)
{
  if (buffer == NULL || bufsize == 0) return 0;
  /* 128비트 vendor UUID 는 SoftDevice 에 등록돼야 비교가 된다. */
  if (!bleuuid.begin())               return 0;
  if (!beginProc(PROC_READ_UUID, conn_hdl)) return 0;

  _buf     = (uint8_t *) buffer;
  _bufsize = bufsize;

  ble_gattc_handle_range_t range = { start_hdl, end_hdl };
  uint16_t len = 0;

  if (sd_ble_gattc_char_value_by_uuid_read(conn_hdl, &bleuuid._uuid, &range) == NRF_SUCCESS) {
    if (waitProc()) len = _len;
  }
  endProc();
  return len;
}

bool BLEGatt::discoverService(uint16_t conn_hdl, BLEUuid bleuuid,
                              uint16_t *start_hdl, uint16_t *end_hdl)
{
  if (!bleuuid.begin())                 return false;
  if (!beginProc(PROC_SRVC, conn_hdl))  return false;

  bool ok = false;

  if (sd_ble_gattc_primary_services_discover(conn_hdl, 1, &bleuuid._uuid) == NRF_SUCCESS) {
    if (waitProc() && _ok) {
      if (start_hdl) *start_hdl = _srvc_start;
      if (end_hdl)   *end_hdl   = _srvc_end;
      ok = true;
    }
  }
  endProc();
  return ok;
}

uint8_t BLEGatt::discoverChars(uint16_t conn_hdl, uint16_t start_hdl, uint16_t end_hdl,
                               ble_gattc_char_t *out, uint8_t max_count)
{
  if (out == NULL || max_count == 0) return 0;

  uint8_t  total = 0;
  uint16_t next  = start_hdl;

  /*
   * ⚠ 한 응답에 다 안 온다. 마지막으로 받은 핸들 다음부터 다시 물어야 하고,
   *   범위가 끝나거나 NOT_FOUND 가 올 때까지 반복한다.
   *   한 번만 부르고 끝내면 뒤쪽 characteristic 이 조용히 빠진다.
   */
  while (next <= end_hdl && total < max_count) {
    if (!beginProc(PROC_CHAR, conn_hdl)) break;

    _chars    = out + total;
    _char_max = (uint8_t) (max_count - total);

    ble_gattc_handle_range_t range = { next, end_hdl };
    uint8_t got = 0;

    if (sd_ble_gattc_characteristics_discover(conn_hdl, &range) == NRF_SUCCESS) {
      if (waitProc() && _ok) got = _char_count;
    }
    endProc();

    if (got == 0) break;

    total += got;
    uint16_t last = out[total - 1].handle_value;
    if (last >= end_hdl) break;
    next = (uint16_t) (last + 1);
  }
  return total;
}

uint16_t BLEGatt::discoverCccd(uint16_t conn_hdl, uint16_t value_hdl, uint16_t end_hdl)
{
  /* CCCD 는 value 핸들 바로 뒤에 온다. 범위를 좁혀 한 번만 묻는다. */
  if (value_hdl >= end_hdl) return 0;
  if (!beginProc(PROC_DESC, conn_hdl)) return 0;

  uint16_t hdl = 0;
  ble_gattc_handle_range_t range = { (uint16_t) (value_hdl + 1), end_hdl };

  if (sd_ble_gattc_descriptors_discover(conn_hdl, &range) == NRF_SUCCESS) {
    if (waitProc() && _ok) hdl = _cccd_hdl;
  }
  endProc();
  return hdl;
}

bool BLEGatt::writeChar(uint16_t conn_hdl, uint16_t value_hdl,
                        const void *data, uint16_t len, bool resp)
{
  if (!Bluefruit.connected(conn_hdl)) return false;

  ble_gattc_write_params_t w;
  memset(&w, 0, sizeof(w));
  w.write_op = resp ? BLE_GATT_OP_WRITE_REQ : BLE_GATT_OP_WRITE_CMD;
  w.handle   = value_hdl;
  w.offset   = 0;
  w.len      = len;
  w.p_value  = (const uint8_t *) data;

  if (!resp) {
    /* 응답 없는 쓰기는 큐에 넣기만 한다. 기다릴 것이 없다. */
    return sd_ble_gattc_write(conn_hdl, &w) == NRF_SUCCESS;
  }

  if (!beginProc(PROC_WRITE, conn_hdl)) return false;

  bool ok = false;
  if (sd_ble_gattc_write(conn_hdl, &w) == NRF_SUCCESS) {
    ok = waitProc() && _ok;
  }
  endProc();
  return ok;
}

uint16_t BLEGatt::exchangeMtu(uint16_t conn_hdl)
{
  uint16_t ours = sdAttMtu();

  if (!beginProc(PROC_MTU, conn_hdl)) return 0;

  uint16_t mtu = 0;
  if (sd_ble_gattc_exchange_mtu_request(conn_hdl, ours) == NRF_SUCCESS) {
    if (waitProc() && _ok) {
      /* 실효 MTU 는 양쪽 제시값 중 작은 쪽이다. */
      mtu = (_mtu < ours) ? _mtu : ours;
      if (mtu < BLE_GATT_ATT_MTU_DEFAULT) mtu = BLE_GATT_ATT_MTU_DEFAULT;
    }
  }
  endProc();
  return mtu;
}

void BLEGatt::_eventHandler(const ble_evt_t *evt)
{
  if (_proc == PROC_NONE) return;
  if (evt->evt.gattc_evt.conn_handle != _conn_hdl) return;

  const ble_gattc_evt_t *g = &evt->evt.gattc_evt;

  switch (evt->header.evt_id) {
    case BLE_GATTC_EVT_CHAR_VAL_BY_UUID_READ_RSP: {
      if (_proc != PROC_READ_UUID) return;

      if (g->gatt_status == BLE_GATT_STATUS_SUCCESS) {
        /*
         * 응답은 [핸들][값] 쌍의 목록이고 값 길이가 가변이라 구조체로 바로
         * 못 읽는다. SoftDevice 가 준 반복자를 써야 한다. 첫 쌍만 쓴다.
         */
        ble_gattc_handle_value_t iter;
        memset(&iter, 0, sizeof(iter));

        if (sd_ble_gattc_evt_char_val_by_uuid_read_rsp_iter(
                (ble_gattc_evt_t *) g, &iter) == NRF_SUCCESS) {
          uint16_t n = g->params.char_val_by_uuid_read_rsp.value_len;
          if (n > _bufsize) n = _bufsize;      /* 넘치면 자른다 */
          memcpy(_buf, iter.p_value, n);
          _len = n;
          _ok  = true;
        }
      }
      xSemaphoreGive((SemaphoreHandle_t) _sem);
      break;
    }

    case BLE_GATTC_EVT_PRIM_SRVC_DISC_RSP:
      if (_proc != PROC_SRVC) return;

      if (g->gatt_status == BLE_GATT_STATUS_SUCCESS &&
          g->params.prim_srvc_disc_rsp.count > 0) {
        /* UUID 로 걸러 물었으므로 첫 건이 우리가 찾던 것이다. */
        _srvc_start = g->params.prim_srvc_disc_rsp.services[0].handle_range.start_handle;
        _srvc_end   = g->params.prim_srvc_disc_rsp.services[0].handle_range.end_handle;
        _ok = true;
      }
      xSemaphoreGive((SemaphoreHandle_t) _sem);
      break;

    case BLE_GATTC_EVT_CHAR_DISC_RSP:
      if (_proc != PROC_CHAR) return;

      if (g->gatt_status == BLE_GATT_STATUS_SUCCESS) {
        uint16_t n = g->params.char_disc_rsp.count;
        if (n > _char_max) n = _char_max;
        for (uint16_t i = 0; i < n; i++) _chars[i] = g->params.char_disc_rsp.chars[i];
        _char_count = (uint8_t) n;
        _ok = (n > 0);
      }
      /* 더 없으면 ATTERR_ATTRIBUTE_NOT_FOUND 가 온다 — 오류가 아니라 끝이다. */
      xSemaphoreGive((SemaphoreHandle_t) _sem);
      break;

    case BLE_GATTC_EVT_DESC_DISC_RSP:
      if (_proc != PROC_DESC) return;

      if (g->gatt_status == BLE_GATT_STATUS_SUCCESS) {
        for (uint16_t i = 0; i < g->params.desc_disc_rsp.count; i++) {
          const ble_gattc_desc_t *d = &g->params.desc_disc_rsp.descs[i];
          if (d->uuid.type == BLE_UUID_TYPE_BLE &&
              d->uuid.uuid == BLE_UUID_DESCRIPTOR_CLIENT_CHAR_CONFIG) {
            _cccd_hdl = d->handle;
            _ok = true;
            break;
          }
        }
      }
      xSemaphoreGive((SemaphoreHandle_t) _sem);
      break;

    case BLE_GATTC_EVT_EXCHANGE_MTU_RSP:
      if (_proc != PROC_MTU) return;
      if (g->gatt_status == BLE_GATT_STATUS_SUCCESS) {
        _mtu = g->params.exchange_mtu_rsp.server_rx_mtu;
        _ok  = true;
      }
      xSemaphoreGive((SemaphoreHandle_t) _sem);
      break;

    case BLE_GATTC_EVT_WRITE_RSP:
      if (_proc != PROC_WRITE) return;
      _ok = (g->gatt_status == BLE_GATT_STATUS_SUCCESS);
      xSemaphoreGive((SemaphoreHandle_t) _sem);
      break;

    /*
     * ⚠ 이 둘을 놓치면 호출한 쪽이 타임아웃까지 멈춘다.
     *   상대가 절차를 거부하거나 링크가 끊기면 이쪽으로 온다.
     */
    case BLE_GATTC_EVT_TIMEOUT:
    case BLE_GAP_EVT_DISCONNECTED:
      xSemaphoreGive((SemaphoreHandle_t) _sem);
      break;

    default:
      break;
  }
}
