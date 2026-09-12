/*
 * Wire — I2C (TWIM)
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * Arduino `Wire` API. 인스턴스와 핀은 **variant 가 정한다**:
 *
 *   WIRE_TWIM_INSTANCE  / WIRE_TWIM_IRQ_HANDLER  / PIN_WIRE_SDA  / PIN_WIRE_SCL
 *   WIRE1_TWIM_INSTANCE / WIRE1_TWIM_IRQ_HANDLER / PIN_WIRE1_SDA / PIN_WIRE1_SCL  (선택)
 *
 * ⚠ **인스턴스 번호가 곧 제약이다** (docs/PERIPHERAL-PINMAP.md §0):
 *   - `TWIM30` 은 `UARTE30` 과 **같은 하드웨어 블록**이다. `Serial` 이 UARTE30 이면
 *     그 보드에서 `Wire` 를 TWIM30 에 놓을 수 없다
 *   - **TWIM00 은 없다** = P2 핀에는 I2C 를 놓을 수 없다
 *   - TWIM20/21/22 는 P1, TWIM30 은 P0 만 쓴다 (도메인 규칙)
 *
 * ⚠ **master 전용이다.** target(slave) 은 지원하지 않는다 — `onReceive()` /
 *   `onRequest()` 가 없다. TWIS 로 별도 구현해야 하고 아직 하지 않았다.
 */
#ifndef _WIRE_H_
#define _WIRE_H_

#include <Arduino.h>
#include "Stream.h"

extern "C" {
#include "nrfx_twim.h"
}

/**
 * 한 번의 전송에 담을 수 있는 최대 바이트.
 * Arduino 의 관례값(32)보다 크게 잡았다 — 센서 라이브러리가 한 번에 긴 블록을
 * 읽는 경우가 흔하고, 넘치면 조용히 잘리는 대신 `endTransmission()` 이 실패한다.
 */
#ifndef WIRE_BUFFER_SIZE
#define WIRE_BUFFER_SIZE   (64)
#endif

class TwoWire : public Stream
{
  public:
    TwoWire(NRF_TWIM_Type *twim, uint32_t pin_sda, uint32_t pin_scl);

    void begin(void);
    void end(void);

    /** 100000 / 250000 / 400000 만 의미가 있다. 그 밖의 값은 가까운 쪽으로 내린다. */
    void setClock(uint32_t freq);

    void    beginTransmission(uint8_t address);

    /**
     * @return 0 성공 / 1 버퍼 초과 / 2 주소 NACK / 3 데이터 NACK / 4 그 밖
     *         (Arduino 규약 그대로다. 라이브러리들이 이 숫자를 본다.)
     */
    uint8_t endTransmission(bool stopBit);
    uint8_t endTransmission(void) { return endTransmission(true); }

    uint8_t requestFrom(uint8_t address, size_t quantity, bool stopBit);
    uint8_t requestFrom(uint8_t address, size_t quantity) { return requestFrom(address, quantity, true); }

    virtual size_t write(uint8_t data);
    virtual size_t write(const uint8_t *data, size_t len);

    virtual int  available(void);
    virtual int  read(void);
    virtual int  peek(void);
    virtual void flush(void);

    using Print::write;

    /* 내부용 — 벡터가 부른다. */
    void _irqHandler(void);
    void _onEvent(const nrfx_twim_event_t *evt);

  private:
    nrfx_twim_t _twim;
    uint32_t    _pin_sda, _pin_scl;
    uint32_t    _freq;
    bool        _begun;

    uint8_t     _addr;
    uint8_t     _tx[WIRE_BUFFER_SIZE];
    size_t      _tx_len;
    bool        _tx_overflow;

    uint8_t     _rx[WIRE_BUFFER_SIZE];
    size_t      _rx_len, _rx_pos;

    void       *_done_sem;     /* SemaphoreHandle_t */
    volatile uint8_t _last_evt;

    bool transfer(const nrfx_twim_xfer_desc_t *desc, uint32_t flags);
};

extern TwoWire Wire;
#ifdef WIRE1_TWIM_INSTANCE
extern TwoWire Wire1;
#endif

#endif
