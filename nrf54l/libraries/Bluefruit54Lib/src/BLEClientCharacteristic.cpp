/*
 * BLEClientCharacteristic — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 */
#include "bluefruit.h"
#include <string.h>

static void chr_init(ble_gattc_char_t *c)
{
  memset(c, 0, sizeof(*c));
  c->handle_value = BLE_GATT_HANDLE_INVALID;
  c->handle_decl  = BLE_GATT_HANDLE_INVALID;
}

BLEClientCharacteristic::BLEClientCharacteristic(void) : uuid()
{
  chr_init(&_chr);
  _cccd_handle = 0;
  _service     = NULL;
  _notify_cb   = NULL;
}

BLEClientCharacteristic::BLEClientCharacteristic(BLEUuid bleuuid) : uuid(bleuuid)
{
  chr_init(&_chr);
  _cccd_handle = 0;
  _service     = NULL;
  _notify_cb   = NULL;
}

void BLEClientCharacteristic::begin(BLEClientService *parent_svc)
{
  _service = parent_svc ? parent_svc : BLEClientService::lastService;

  uuid.begin();
  if (_service) _service->_registerChar(this);
}

uint16_t BLEClientCharacteristic::connHandle(void) const
{
  return _service ? _service->connHandle() : BLE_CONN_HANDLE_INVALID;
}

uint8_t BLEClientCharacteristic::properties(void) const
{
  /* BLECharacteristic 의 CHR_PROPS_* 와 같은 비트 배치로 맞춘다. */
  uint8_t p = 0;
  if (_chr.char_props.broadcast)      p |= CHR_PROPS_BROADCAST;
  if (_chr.char_props.read)           p |= CHR_PROPS_READ;
  if (_chr.char_props.write_wo_resp)  p |= CHR_PROPS_WRITE_WO_RESP;
  if (_chr.char_props.write)          p |= CHR_PROPS_WRITE;
  if (_chr.char_props.notify)         p |= CHR_PROPS_NOTIFY;
  if (_chr.char_props.indicate)       p |= CHR_PROPS_INDICATE;
  return p;
}

bool BLEClientCharacteristic::discover(void)
{
  if (_service == NULL || !_service->discovered()) return false;

  /* 서비스가 한 번에 훑어서 UUID 로 나눠 준다 (BLEClientService 주석 참조). */
  _service->discoverCharacteristics();
  return discovered();
}

/* ── 읽기 ─────────────────────────────────────────────────────────── */

uint16_t BLEClientCharacteristic::read(void *buffer, uint16_t bufsize)
{
  if (!discovered()) return 0;
  return Bluefruit.Gatt.readChar(connHandle(), _chr.handle_value, buffer, bufsize);
}

uint8_t BLEClientCharacteristic::read8(void)
{
  uint8_t v = 0;
  return (read(&v, 1) == 1) ? v : 0;
}

uint16_t BLEClientCharacteristic::read16(void)
{
  uint16_t v = 0;
  return (read(&v, 2) == 2) ? v : 0;
}

uint32_t BLEClientCharacteristic::read32(void)
{
  uint32_t v = 0;
  return (read(&v, 4) == 4) ? v : 0;
}

/* ── 쓰기 ─────────────────────────────────────────────────────────── */

uint16_t BLEClientCharacteristic::write(const void *data, uint16_t len)
{
  if (!discovered()) return 0;
  return Bluefruit.Gatt.writeChar(connHandle(), _chr.handle_value, data, len, false) ? len : 0;
}

uint16_t BLEClientCharacteristic::write_resp(const void *data, uint16_t len)
{
  if (!discovered()) return 0;
  return Bluefruit.Gatt.writeChar(connHandle(), _chr.handle_value, data, len, true) ? len : 0;
}

uint16_t BLEClientCharacteristic::write8(uint8_t v)        { return write(&v, 1); }
uint16_t BLEClientCharacteristic::write16(uint16_t v)      { return write(&v, 2); }
uint16_t BLEClientCharacteristic::write32(uint32_t v)      { return write(&v, 4); }
uint16_t BLEClientCharacteristic::write8_resp(uint8_t v)   { return write_resp(&v, 1); }
uint16_t BLEClientCharacteristic::write16_resp(uint16_t v) { return write_resp(&v, 2); }
uint16_t BLEClientCharacteristic::write32_resp(uint32_t v) { return write_resp(&v, 4); }

/* ── 알림 ─────────────────────────────────────────────────────────── */

bool BLEClientCharacteristic::writeCCCD(uint16_t value)
{
  /*
   * ⚠ CCCD 가 없으면 알림을 못 켠다. 탐색 때 CCCD 를 못 찾았다는 뜻이고,
   *   보통 그 characteristic 이 notify/indicate 속성을 갖고 있지 않다.
   */
  if (!discovered() || _cccd_handle == 0) return false;

  /* 응답을 확인해야 실제로 켜졌는지 알 수 있다. */
  return Bluefruit.Gatt.writeChar(connHandle(), _cccd_handle, &value, 2, true);
}

bool BLEClientCharacteristic::enableNotify(void)    { return writeCCCD(BLE_GATT_HVX_NOTIFICATION); }
bool BLEClientCharacteristic::disableNotify(void)   { return writeCCCD(0); }
bool BLEClientCharacteristic::enableIndicate(void)  { return writeCCCD(BLE_GATT_HVX_INDICATION); }
bool BLEClientCharacteristic::disableIndicate(void) { return writeCCCD(0); }

void BLEClientCharacteristic::setNotifyCallback(notify_cb_t fp, bool use_ada_callback)
{
  /* Adafruit 은 콜백을 별도 태스크로 미룰지 고르게 한다. 우리는 항상 직접
   * 부르므로 값만 받아 둔다 (헤더 주석 참조). */
  (void) use_ada_callback;
  _notify_cb = fp;
}

void BLEClientCharacteristic::_disconnected(void)
{
  chr_init(&_chr);
  _cccd_handle = 0;
}

void BLEClientCharacteristic::_eventHandler(const ble_evt_t *evt)
{
  if (evt->header.evt_id != BLE_GATTC_EVT_HVX) return;
  if (!discovered()) return;

  const ble_gattc_evt_hvx_t *hvx = &evt->evt.gattc_evt.params.hvx;
  if (hvx->handle != _chr.handle_value) return;

  /*
   * ⚠ indication 은 **확인 응답을 보내야 한다.** 안 보내면 상대가 다음 것을
   *   안 보내고 결국 GATT 절차 타임아웃으로 링크가 끊긴다.
   */
  if (hvx->type == BLE_GATT_HVX_INDICATION) {
    sd_ble_gattc_hv_confirm(evt->evt.gattc_evt.conn_handle, hvx->handle);
  }

  if (_notify_cb) _notify_cb(this, (uint8_t *) hvx->data, hvx->len);
}
