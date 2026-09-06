/*
 * BLECharacteristic — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 */
#include "BLECharacteristic.h"
#include "bluefruit.h"
#include <string.h>

static void sec_mode_set(ble_gap_conn_sec_mode_t *m, uint8_t mode)
{
  m->sm = (mode >> 4) & 0x0F;
  m->lv = (mode     ) & 0x0F;
}

#define CHR_INIT()                                        \
  do {                                                    \
    _handles.value_handle = BLE_GATT_HANDLE_INVALID;      \
    _handles.cccd_handle  = BLE_GATT_HANDLE_INVALID;      \
    _properties = 0;                                      \
    _max_len    = 20;                                     \
    _fixed_len  = 0;                                      \
    _rd_sec     = SECMODE_OPEN;                           \
    _wr_sec     = SECMODE_OPEN;                           \
    _wr_cb      = NULL;                                   \
  } while (0)

BLECharacteristic::BLECharacteristic(void)                      : uuid()        { CHR_INIT(); }
BLECharacteristic::BLECharacteristic(uint16_t uuid16)           : uuid(uuid16)  { CHR_INIT(); }
BLECharacteristic::BLECharacteristic(uint8_t const uuid128[16]) : uuid(uuid128) { CHR_INIT(); }

void BLECharacteristic::setUuid(BLEUuid bleuuid)        { uuid = bleuuid; }
void BLECharacteristic::setProperties(uint8_t prop)     { _properties = prop; }
void BLECharacteristic::setFixedLen(uint16_t len)
{
  /*
   * ⚠ 고정 길이는 max_len 도 **정확히 같아야 한다.**
   *   "더 클 때만 늘린다" 로 두면 기본값(20)이 남아, 짧은 문자열을 넣어도
   *   속성이 20바이트로 잡히고 남는 부분이 초기화되지 않은 0xFF 로 노출된다.
   *   실제로 DIS 문자열 뒤에 0xFF 가 붙어 나왔다.
   */
  _fixed_len = len;
  _max_len   = len;
}
void BLECharacteristic::setMaxLen(uint16_t len)         { _max_len = len; }
void BLECharacteristic::setWriteCallback(write_cb_t fp) { _wr_cb = fp; }

void BLECharacteristic::setPermission(BleSecurityMode read, BleSecurityMode write)
{
  _rd_sec = (uint8_t) read;
  _wr_sec = (uint8_t) write;
}

err_t BLECharacteristic::begin(void)
{
  if (!uuid.begin()) {
    return NRF_ERROR_INVALID_PARAM;
  }

  ble_gatts_char_md_t char_md;
  ble_gatts_attr_md_t cccd_md;
  ble_gatts_attr_t    attr;
  ble_gatts_attr_md_t attr_md;

  memset(&char_md, 0, sizeof(char_md));
  memset(&cccd_md, 0, sizeof(cccd_md));
  memset(&attr,    0, sizeof(attr));
  memset(&attr_md, 0, sizeof(attr_md));

  char_md.char_props.broadcast      = (_properties & CHR_PROPS_BROADCAST)     ? 1 : 0;
  char_md.char_props.read           = (_properties & CHR_PROPS_READ)          ? 1 : 0;
  char_md.char_props.write_wo_resp  = (_properties & CHR_PROPS_WRITE_WO_RESP) ? 1 : 0;
  char_md.char_props.write          = (_properties & CHR_PROPS_WRITE)         ? 1 : 0;
  char_md.char_props.notify         = (_properties & CHR_PROPS_NOTIFY)        ? 1 : 0;
  char_md.char_props.indicate       = (_properties & CHR_PROPS_INDICATE)      ? 1 : 0;

  /*
   * notify / indicate 를 쓰면 CCCD 가 필요하다. CCCD 는 **상대가 쓰는** 디스크립터라
   * 쓰기 권한이 열려 있어야 한다. 빠뜨리면 상대가 알림을 켤 수 없고,
   * 증상은 "연결은 되는데 데이터가 안 온다" 로 나타난다.
   */
  if (_properties & (CHR_PROPS_NOTIFY | CHR_PROPS_INDICATE)) {
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.write_perm);
    cccd_md.vloc = BLE_GATTS_VLOC_STACK;
    char_md.p_cccd_md = &cccd_md;
  }

  sec_mode_set(&attr_md.read_perm,  _rd_sec);
  sec_mode_set(&attr_md.write_perm, _wr_sec);
  attr_md.vloc    = BLE_GATTS_VLOC_STACK;   /* 값은 SoftDevice 가 보관한다 */
  attr_md.vlen    = (_fixed_len == 0) ? 1 : 0;

  attr.p_uuid    = &uuid._uuid;
  attr.p_attr_md = &attr_md;
  attr.init_len  = (_fixed_len == 0) ? 0 : _fixed_len;
  attr.max_len   = _max_len;

  err_t err = sd_ble_gatts_characteristic_add(Bluefruit._currentServiceHandle(),
                                              &char_md, &attr, &_handles);
  if (err == NRF_SUCCESS) {
    /* 쓰기 이벤트를 받으려면 등록해야 한다. */
    Bluefruit._registerChar(this);
  }
  return err;
}

uint16_t BLECharacteristic::write(const void *data, uint16_t len)
{
  if (_handles.value_handle == BLE_GATT_HANDLE_INVALID) return 0;
  if (len > _max_len) len = _max_len;

  ble_gatts_value_t v;
  memset(&v, 0, sizeof(v));
  v.len     = len;
  v.offset  = 0;
  v.p_value = (uint8_t *) data;

  /* BLE_CONN_HANDLE_INVALID = 연결과 무관하게 서버 값 자체를 갱신한다. */
  return (sd_ble_gatts_value_set(BLE_CONN_HANDLE_INVALID, _handles.value_handle, &v) == NRF_SUCCESS)
         ? len : 0;
}

uint16_t BLECharacteristic::write(const char *str)
{
  return write((const void *) str, (uint16_t) strlen(str));
}

bool BLECharacteristic::notifyEnabled(uint16_t conn_hdl) const
{
  if (_handles.cccd_handle == BLE_GATT_HANDLE_INVALID) return false;

  uint8_t buf[2] = { 0, 0 };
  ble_gatts_value_t v;
  memset(&v, 0, sizeof(v));
  v.len     = 2;
  v.p_value = buf;

  if (sd_ble_gatts_value_get(conn_hdl, _handles.cccd_handle, &v) != NRF_SUCCESS) return false;
  return (buf[0] & BLE_GATT_HVX_NOTIFICATION) != 0;
}

bool BLECharacteristic::notify(const void *data, uint16_t len)
{
  uint16_t conn = Bluefruit.connHandle();
  if (conn == BLE_CONN_HANDLE_INVALID) return false;
  if (!notifyEnabled(conn)) {
    /* 상대가 CCCD 를 켜지 않았다. 값만 갱신해 둔다. */
    write(data, len);
    return false;
  }

  /*
   * ⚠ SoftDevice 의 notify 큐는 얕다 (기본 1건). 연속으로 보내면 금방
   *   NRF_ERROR_RESOURCES 가 난다. 그때 포기하면 **데이터가 조용히 사라진다** —
   *   실제로 10바이트를 에코했는데 3바이트만 나가는 증상을 겪었다.
   *   HVN_TX_COMPLETE 를 기다렸다가 재시도한다.
   */
  for (uint8_t retry = 0; retry < BLE_HVX_MAX_RETRY; retry++) {
    uint16_t n = len;
    ble_gatts_hvx_params_t hvx;
    memset(&hvx, 0, sizeof(hvx));
    hvx.handle = _handles.value_handle;
    hvx.type   = BLE_GATT_HVX_NOTIFICATION;
    hvx.offset = 0;
    hvx.p_len  = &n;
    hvx.p_data = (uint8_t *) data;

    uint32_t err = sd_ble_gatts_hvx(conn, &hvx);
    if (err == NRF_SUCCESS) return true;
    if (err != NRF_ERROR_RESOURCES) return false;

    if (!Bluefruit._waitTxComplete(BLE_HVX_TX_TIMEOUT_MS)) return false;
  }
  return false;
}

bool BLECharacteristic::notify(const char *str)
{
  return notify((const void *) str, (uint16_t) strlen(str));
}

void BLECharacteristic::_eventHandler(const ble_evt_t *evt)
{
  if (evt->header.evt_id != BLE_GATTS_EVT_WRITE) return;

  const ble_gatts_evt_write_t *wr = &evt->evt.gatts_evt.params.write;
  if (wr->handle != _handles.value_handle) return;

  if (_wr_cb) {
    _wr_cb(evt->evt.gatts_evt.conn_handle, this, (uint8_t *) wr->data, wr->len);
  }
}
