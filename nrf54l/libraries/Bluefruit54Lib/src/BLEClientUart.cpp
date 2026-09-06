/*
 * BLEClientUart — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 */
#include "bluefruit.h"
#include <string.h>

static void clientuart_notify_cb(BLEClientCharacteristic *chr, uint8_t *data, uint16_t len)
{
  BLEClientUart *uart = (BLEClientUart *) chr->parentService();
  if (uart) uart->_rxNotify(data, len);
}

BLEClientUart::BLEClientUart(void)
  : BLEClientService(BLEUuid(BLEUART_UUID_SERVICE)),
    _txd(BLEUuid(BLEUART_UUID_CHR_TXD)),
    _rxd(BLEUuid(BLEUART_UUID_CHR_RXD))
{
  _rxhead = _rxtail = 0;
  _rx_dropped = 0;
  _rx_cb = NULL;
}

bool BLEClientUart::begin(void)
{
  _rxhead = _rxtail = 0;

  if (!BLEClientService::begin()) return false;

  _txd.begin(this);
  _rxd.begin(this);
  _txd.setNotifyCallback(clientuart_notify_cb);
  return true;
}

bool BLEClientUart::discover(uint16_t conn_hdl)
{
  if (!BLEClientService::discover(conn_hdl)) return false;

  /* 서비스 범위를 한 번 훑어 두 characteristic 을 한꺼번에 채운다. */
  discoverCharacteristics();

  if (!_txd.discovered() || !_rxd.discovered()) {
    _disconnected();          /* 반쪽만 찾은 상태로 두지 않는다 */
    return false;
  }
  _rxhead = _rxtail = 0;
  return true;
}

bool BLEClientUart::enableTXD(void)  { return _txd.enableNotify(); }
bool BLEClientUart::disableTXD(void) { return _txd.disableNotify(); }

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

void BLEClientUart::_rxNotify(uint8_t *data, uint16_t len)
{
  for (uint16_t i = 0; i < len; i++) {
    /* 가득 차면 새 데이터를 버리고 센다 (BLEUart 와 같은 정책). */
    if (!rxPush(data[i])) {
      _rx_dropped += (uint32_t) (len - i);
      break;
    }
  }
  if (_rx_cb) _rx_cb(*this);
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

size_t BLEClientUart::write(uint8_t b) { return write(&b, 1); }

size_t BLEClientUart::write(const uint8_t *content, size_t len)
{
  if (!_rxd.discovered()) return 0;

  /*
   * 한 번에 보낼 수 있는 크기를 넘으면 잘라서 여러 번 보낸다.
   * 응답 없는 쓰기를 지원하면 그쪽을 쓴다 — 왕복이 없어 훨씬 빠르다.
   */
  const bool   no_resp = (_rxd.properties() & CHR_PROPS_WRITE_WO_RESP) != 0;
  const size_t chunk   = Bluefruit.maxPayload(connHandle());
  size_t sent = 0;

  while (sent < len) {
    size_t n = len - sent;
    if (n > chunk) n = chunk;

    uint16_t done = no_resp ? _rxd.write(content + sent, (uint16_t) n)
                            : _rxd.write_resp(content + sent, (uint16_t) n);
    if (done == 0) break;
    sent += done;
  }
  return sent;
}
