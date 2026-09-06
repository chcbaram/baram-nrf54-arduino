/*
 * BLEClientUart — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 */
#include "bluefruit.h"
#include <string.h>

/* NUS 안의 characteristic 은 둘뿐이다. 넉넉히 4 개까지 받아 둔다. */
#define NUS_CHAR_MAX   (4)

BLEClientUart::BLEClientUart(void)
  : uuid(BLEUART_UUID_SERVICE)
{
  _conn_hdl     = BLE_CONN_HANDLE_INVALID;
  _tx_value_hdl = 0;
  _tx_cccd_hdl  = 0;
  _rx_value_hdl = 0;
  _rx_write_no_resp = true;
  _rxhead = _rxtail = 0;
  _rx_dropped = 0;
  _rx_cb = NULL;
}

bool BLEClientUart::begin(void)
{
  /* 128비트 UUID 를 SoftDevice 에 등록해 둔다. 탐색 때 비교에 쓰인다. */
  if (!uuid.begin()) return false;
  return Bluefruit._registerClientUart(this);
}

bool BLEClientUart::discover(uint16_t conn_hdl)
{
  _conn_hdl     = BLE_CONN_HANDLE_INVALID;
  _tx_value_hdl = _tx_cccd_hdl = _rx_value_hdl = 0;

  uint16_t start = 0, end = 0;
  if (!Bluefruit.Gatt.discoverService(conn_hdl, uuid, &start, &end)) return false;

  ble_gattc_char_t chars[NUS_CHAR_MAX];
  uint8_t n = Bluefruit.Gatt.discoverChars(conn_hdl, start, end, chars, NUS_CHAR_MAX);
  if (n == 0) return false;

  /*
   * ⚠ characteristic 을 **UUID 로 가려야 한다.** 순서로 고르면 안 된다 —
   *   규격이 순서를 정해 두지 않아서 구현마다 다르다.
   */
  for (uint8_t i = 0; i < n; i++) {
    if (chars[i].uuid.type != uuid._uuid.type) continue;

    if (chars[i].uuid.uuid == BLEUuid(BLEUART_UUID_CHR_TXD)._uuid.uuid) {
      _tx_value_hdl = chars[i].handle_value;
    } else if (chars[i].uuid.uuid == BLEUuid(BLEUART_UUID_CHR_RXD)._uuid.uuid) {
      _rx_value_hdl = chars[i].handle_value;
      /* 응답 없는 쓰기를 지원하면 그쪽을 쓴다 — 왕복이 없어 훨씬 빠르다. */
      _rx_write_no_resp = chars[i].char_props.write_wo_resp ? true : false;
    }
  }
  if (_tx_value_hdl == 0 || _rx_value_hdl == 0) return false;

  _tx_cccd_hdl = Bluefruit.Gatt.discoverCccd(conn_hdl, _tx_value_hdl, end);
  if (_tx_cccd_hdl == 0) return false;

  _conn_hdl = conn_hdl;
  _rxhead = _rxtail = 0;
  return true;
}

bool BLEClientUart::enableTXD(void)
{
  if (!discovered()) return false;

  /* CCCD 에 0x0001 을 쓰면 알림이 켜진다. 응답을 확인해야 확실하다. */
  uint16_t value = BLE_GATT_HVX_NOTIFICATION;
  return Bluefruit.Gatt.writeChar(_conn_hdl, _tx_cccd_hdl, &value, 2, true);
}

bool BLEClientUart::disableTXD(void)
{
  if (!discovered()) return false;

  uint16_t value = 0;
  return Bluefruit.Gatt.writeChar(_conn_hdl, _tx_cccd_hdl, &value, 2, true);
}

uint16_t BLEClientUart::rxCount(void) const
{
  uint16_t h = _rxhead, t = _rxtail;
  return (uint16_t) ((h >= t) ? (h - t) : (BLE_UART_RX_FIFO_SIZE - t + h));
}

bool BLEClientUart::rxPush(uint8_t b)
{
  uint16_t next = (uint16_t) ((_rxhead + 1) % BLE_UART_RX_FIFO_SIZE);
  if (next == _rxtail) return false;
  _rxbuf[_rxhead] = b;
  _rxhead = next;
  return true;
}

int BLEClientUart::rxPop(void)
{
  if (_rxhead == _rxtail) return -1;
  uint8_t b = _rxbuf[_rxtail];
  _rxtail = (uint16_t) ((_rxtail + 1) % BLE_UART_RX_FIFO_SIZE);
  return b;
}

int  BLEClientUart::available(void) { return (int) rxCount(); }
int  BLEClientUart::peek(void)      { return (_rxhead == _rxtail) ? -1 : _rxbuf[_rxtail]; }
int  BLEClientUart::read(void)      { return rxPop(); }
void BLEClientUart::flush(void)     { /* 송신은 바로 나간다 */ }

size_t BLEClientUart::read(uint8_t *buf, size_t size)
{
  size_t n = 0;
  while (n < size) {
    int c = rxPop();
    if (c < 0) break;
    buf[n++] = (uint8_t) c;
  }
  return n;
}

size_t BLEClientUart::write(uint8_t b)
{
  return write(&b, 1);
}

size_t BLEClientUart::write(const uint8_t *content, size_t len)
{
  if (!discovered()) return 0;

  /* 한 번에 보낼 수 있는 크기를 넘으면 잘라서 여러 번 보낸다. */
  const size_t chunk = Bluefruit.maxPayload(_conn_hdl);
  size_t sent = 0;

  while (sent < len) {
    size_t n = len - sent;
    if (n > chunk) n = chunk;

    if (!Bluefruit.Gatt.writeChar(_conn_hdl, _rx_value_hdl,
                                  content + sent, (uint16_t) n, !_rx_write_no_resp)) {
      break;
    }
    sent += n;
  }
  return sent;
}

void BLEClientUart::_disconnected(uint16_t conn_hdl)
{
  if (_conn_hdl != conn_hdl) return;
  _conn_hdl     = BLE_CONN_HANDLE_INVALID;
  _tx_value_hdl = _tx_cccd_hdl = _rx_value_hdl = 0;
}

void BLEClientUart::_eventHandler(const ble_evt_t *evt)
{
  if (evt->header.evt_id == BLE_GAP_EVT_DISCONNECTED) {
    _disconnected(evt->evt.gap_evt.conn_handle);
    return;
  }

  if (evt->header.evt_id != BLE_GATTC_EVT_HVX) return;
  if (!discovered()) return;
  if (evt->evt.gattc_evt.conn_handle != _conn_hdl) return;

  const ble_gattc_evt_hvx_t *hvx = &evt->evt.gattc_evt.params.hvx;
  if (hvx->handle != _tx_value_hdl) return;

  for (uint16_t i = 0; i < hvx->len; i++) {
    /* 가득 차면 새 데이터를 버리고 센다 (BLEUart 와 같은 정책). */
    if (!rxPush(hvx->data[i])) {
      _rx_dropped += (uint32_t) (hvx->len - i);
      break;
    }
  }

  if (_rx_cb) _rx_cb(*this);
}
