/*
 * BLEClientUart — 상대의 Nordic UART Service 에 붙는다 (central 쪽)
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * BLEUart 의 반대쪽이다. 우리가 central 로 붙어서 상대 peripheral 의
 * NUS 를 쓴다. Stream 을 상속하므로 Serial 처럼 쓴다.
 *
 * ⚠ 이름이 **상대 기준**이라 방향이 뒤집힌다. 상대의 RX 에 우리가 쓰고,
 *   상대의 TX 를 우리가 notify 로 받는다. BLEUart 와 반대다.
 */
#ifndef _BLE_CLIENT_UART_H_
#define _BLE_CLIENT_UART_H_

#include <Arduino.h>
#include <Stream.h>
#include "BLEUuid.h"
#include "BLEUart.h"      /* BLEUART_UUID_*, BLE_UART_RX_FIFO_SIZE */

class BLEClientUart;
typedef void (*ble_client_uart_rx_callback_t)(BLEClientUart &uart);

class BLEClientUart : public Stream
{
  public:
    BLEClientUart(void);

    BLEUuid uuid;

    /** 등록만 한다. 실제 탐색은 discover() 다. */
    bool begin(void);

    /**
     * 상대에게서 NUS 를 찾는다. 연결 콜백 안에서 부른다.
     *
     * ⚠ **블로킹이다.** 서비스 -> characteristic -> CCCD 를 차례로 묻는다.
     *   연결 콜백은 BLE 이벤트 태스크가 아니라 별도 태스크에서 도니 안전하다.
     *
     * @return 찾았으면 true.
     */
    bool discover(uint16_t conn_hdl);
    bool discovered(void) const { return _conn_hdl != BLE_CONN_HANDLE_INVALID; }
    uint16_t connHandle(void) const { return _conn_hdl; }

    /** 상대의 TX 알림을 켠다. 이걸 해야 데이터가 들어온다. */
    bool enableTXD(void);
    bool disableTXD(void);

    void setRxCallback(ble_client_uart_rx_callback_t fp) { _rx_cb = fp; }

    /* ── Stream / Print ─────────────────────────────────────────────── */
    virtual int    available(void);
    virtual int    peek(void);
    virtual int    read(void);
    virtual void   flush(void);
    virtual size_t write(uint8_t b);
    virtual size_t write(const uint8_t *content, size_t len);
    using Print::write;

    size_t read(uint8_t *buf, size_t size);

    /** FIFO 가 넘쳐 버린 바이트 수. */
    uint32_t dropped(void) const { return _rx_dropped; }

    /* 코어 내부용 */
    void _eventHandler(const ble_evt_t *evt);
    void _disconnected(uint16_t conn_hdl);

  protected:
    uint16_t _conn_hdl;
    uint16_t _tx_value_hdl;   /* 상대 TX (notify) — 우리가 받는 쪽 */
    uint16_t _tx_cccd_hdl;
    uint16_t _rx_value_hdl;   /* 상대 RX (write)  — 우리가 쓰는 쪽 */
    bool     _rx_write_no_resp;

    uint8_t           _rxbuf[BLE_UART_RX_FIFO_SIZE];
    volatile uint16_t _rxhead;
    volatile uint16_t _rxtail;
    volatile uint32_t _rx_dropped;

    ble_client_uart_rx_callback_t _rx_cb;

    uint16_t rxCount(void) const;
    bool     rxPush(uint8_t b);
    int      rxPop(void);
};

#endif
