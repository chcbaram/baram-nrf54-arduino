/*
 * BLEUart — Nordic UART Service (NUS)
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * API 는 Adafruit Bluefruit52Lib 의 BLEUart 를 따른다. Stream 을 상속하므로
 * Serial 처럼 쓴다 (CLAUDE.md R12 — 호환 우선).
 *
 * NUS UUID (Nordic 규정):
 *   서비스 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
 *   RX     6E400002-...  상대 -> 우리 (상대가 write)
 *   TX     6E400003-...  우리 -> 상대 (notify)
 *
 * ⚠ 이름이 **상대 기준**이다. 우리가 보내는 쪽이 TX 이고, 그건 상대가
 *   notify 로 받는다. 반대로 읽는 것은 RX 다. 헷갈리기 쉬운 지점이다.
 */
#ifndef _BLE_UART_H_
#define _BLE_UART_H_

#include <Arduino.h>
#include <Stream.h>
#include <RingBuffer.h>

#include "BLEService.h"
#include "BLECharacteristic.h"

class BLEUart;
typedef void (*ble_uart_rx_callback_t)(uint16_t conn_hdl);

class BLEUart : public BLEService, public Stream
{
  public:
    BLEUart(void);

    virtual err_t begin(void);

    /** 상대가 알림을 켰는가. 켜기 전에 write() 해도 나가지 않는다. */
    bool notifyEnabled(void);

    void setRxCallback(ble_uart_rx_callback_t fp) { _rx_cb = fp; }

    /* ── Stream / Print ─────────────────────────────────────────────── */
    virtual int    available(void);
    virtual int    peek(void);
    virtual int    read(void);
    virtual void   flush(void);
    virtual size_t write(uint8_t b);
    virtual size_t write(const uint8_t *content, size_t len);
    using Print::write;

    size_t read(uint8_t *buf, size_t size);

    /* 코어 내부용 */
    void _rxHandler(uint16_t conn_hdl, uint8_t *data, uint16_t len);

  protected:
    BLECharacteristic _txchr;
    BLECharacteristic _rxchr;
    RingBuffer        _rxfifo;
    ble_uart_rx_callback_t _rx_cb;
};

#endif
