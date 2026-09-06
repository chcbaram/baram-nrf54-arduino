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

/*
 * 수신 FIFO 크기.
 *
 * ⚠ FIFO 는 **연결마다가 아니라 하나를 공유한다.** 여러 상대가 동시에 보내면
 *   바이트가 섞이고, available()/read() 로는 어느 쪽이 보낸 것인지 알 수 없다.
 *   Adafruit 도 같은 구조이고 bleuart_multi 예제도 그 전제로 동작한다 (R12).
 *   보낸 쪽을 알아야 하면 setRxCallback() 의 conn_hdl 을 쓴다.
 *
 * ⚠ 코어의 RingBuffer(SERIAL_BUFFER_SIZE = 64)를 쓰면 안 된다. MTU 를 키우면
 *   한 번에 그보다 많이 들어오고, 넘치는 만큼 **조용히 사라진다.**
 *   실제로 MTU 247 에서 75바이트를 보냈더니 62바이트만 에코됐다.
 *   최소한 한 번에 받을 수 있는 최대치(MTU − 3)보다 커야 한다.
 */
#ifndef BLE_UART_RX_FIFO_SIZE
#define BLE_UART_RX_FIFO_SIZE   (256)
#endif

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
    bool notifyEnabled(uint16_t conn_hdl);

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

    /**
     * 특정 연결로 보낸다. 그 핸들이 연결돼 있지 않으면 0 을 돌려준다.
     *
     * ⚠ 핸들 없는 write() 는 **모든 연결이 아니라 `Bluefruit.connHandle()`
     *   한 곳으로만** 간다 (Adafruit 과 같은 동작). 붙어 있는 전부에 보내려면
     *   스케치가 핸들을 돌며 이 판을 부른다 — examples/Peripheral/bleuart_multi.
     */
    size_t write(uint16_t conn_hdl, const uint8_t *content, size_t len);
    size_t write(uint16_t conn_hdl, uint8_t b);

    /* 코어 내부용 */
    void _rxHandler(uint16_t conn_hdl, uint8_t *data, uint16_t len);

    /** FIFO 가 넘쳐 버린 바이트 수. 0 이 아니면 BLE_UART_RX_FIFO_SIZE 를 키워라. */
    uint32_t dropped(void) const { return _rx_dropped; }

  protected:
    BLECharacteristic _txchr;
    BLECharacteristic _rxchr;
    ble_uart_rx_callback_t _rx_cb;

    /* 단순 링버퍼. head == tail 이면 비어 있다. */
    uint8_t           _rxbuf[BLE_UART_RX_FIFO_SIZE];
    volatile uint16_t _rxhead;
    volatile uint16_t _rxtail;
    volatile uint32_t _rx_dropped;

    uint16_t rxCount(void) const;
    bool     rxPush(uint8_t b);
    int      rxPop(void);
};

#endif
