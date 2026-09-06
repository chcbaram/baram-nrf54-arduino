/*
 * BLEUart — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 */
#include "BLEUart.h"
#include "bluefruit.h"
#include <string.h>

/* little-endian 배열. 위 헤더의 UUID 를 바이트 역순으로 적은 것이다. */
static const uint8_t NUS_SVC[16] = {
  0x9E,0xCA,0xDC,0x24,0x0E,0xE5,0xA9,0xE0, 0x93,0xF3,0xA3,0xB5,0x01,0x00,0x40,0x6E
};
static const uint8_t NUS_RX[16] = {
  0x9E,0xCA,0xDC,0x24,0x0E,0xE5,0xA9,0xE0, 0x93,0xF3,0xA3,0xB5,0x02,0x00,0x40,0x6E
};
static const uint8_t NUS_TX[16] = {
  0x9E,0xCA,0xDC,0x24,0x0E,0xE5,0xA9,0xE0, 0x93,0xF3,0xA3,0xB5,0x03,0x00,0x40,0x6E
};

static BLEUart *_uart_instance = NULL;

static void bleuart_write_cb(uint16_t conn_hdl, BLECharacteristic *chr, uint8_t *data, uint16_t len)
{
  (void) chr;
  if (_uart_instance) _uart_instance->_rxHandler(conn_hdl, data, len);
}

BLEUart::BLEUart(void)
  : BLEService(NUS_SVC), _txchr(NUS_TX), _rxchr(NUS_RX)
{
  _rx_cb = NULL;
}

err_t BLEUart::begin(void)
{
  _uart_instance = this;

  err_t err = BLEService::begin();
  if (err) return err;

  /* TX: 우리가 보내는 쪽. 상대는 notify 로 받는다. */
  _txchr.setProperties(CHR_PROPS_NOTIFY);
  _txchr.setMaxLen(BLE_GATT_ATT_MTU_DEFAULT - 3);   /* ATT 헤더 3바이트를 뺀다 */
  err = _txchr.begin();
  if (err) return err;

  /* RX: 상대가 쓰는 쪽. 응답 있는/없는 쓰기 둘 다 받는다. */
  _rxchr.setProperties(CHR_PROPS_WRITE | CHR_PROPS_WRITE_WO_RESP);
  _rxchr.setMaxLen(BLE_GATT_ATT_MTU_DEFAULT - 3);
  _rxchr.setWriteCallback(bleuart_write_cb);
  err = _rxchr.begin();

  return err;
}

bool BLEUart::notifyEnabled(void)
{
  uint16_t conn = Bluefruit.connHandle();
  if (conn == BLE_CONN_HANDLE_INVALID) return false;
  return _txchr.notifyEnabled(conn);
}

void BLEUart::_rxHandler(uint16_t conn_hdl, uint8_t *data, uint16_t len)
{
  for (uint16_t i = 0; i < len; i++) {
    /*
     * 링버퍼가 가득 차면 **새 데이터를 버린다.** 오래된 것을 밀어내면
     * 이미 읽고 있던 프레임 중간이 잘려 더 나쁘다.
     * 버퍼는 SERIAL_BUFFER_SIZE(64) 다 — 큰 덩어리를 받으려면 늘려야 한다.
     */
    if (_rxfifo.isFull()) break;
    _rxfifo.store_char(data[i]);
  }

  if (_rx_cb) _rx_cb(conn_hdl);
}

int  BLEUart::available(void) { return _rxfifo.available(); }
int  BLEUart::peek(void)      { return _rxfifo.peek(); }
int  BLEUart::read(void)      { return _rxfifo.read_char(); }
void BLEUart::flush(void)     { /* 송신은 notify 라 버퍼링이 없다 */ }

size_t BLEUart::read(uint8_t *buf, size_t size)
{
  size_t n = 0;
  while (n < size) {
    int c = _rxfifo.read_char();
    if (c < 0) break;
    buf[n++] = (uint8_t) c;
  }
  return n;
}

size_t BLEUart::write(uint8_t b)
{
  return write(&b, 1);
}

size_t BLEUart::write(const uint8_t *content, size_t len)
{
  if (!notifyEnabled()) return 0;

  /*
   * 한 번의 notify 로 보낼 수 있는 크기를 넘으면 잘라서 여러 번 보낸다.
   * MTU 협상을 아직 안 하므로 기본값(23) - ATT 헤더(3) = 20 바이트다.
   */
  const size_t chunk = BLE_GATT_ATT_MTU_DEFAULT - 3;
  size_t sent = 0;

  while (sent < len) {
    size_t n = len - sent;
    if (n > chunk) n = chunk;

    if (!_txchr.notify(content + sent, (uint16_t) n)) {
      break;                 /* 큐가 찼거나 연결이 끊겼다 */
    }
    sent += n;
  }
  return sent;
}
