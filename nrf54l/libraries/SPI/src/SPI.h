/*
 * SPI — SPIM
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * Arduino `SPI` API. 인스턴스와 핀은 **variant 가 정한다**:
 *
 *   SPI_SPIM_INSTANCE / SPI_SPIM_IRQ_HANDLER / PIN_SPI_SCK / PIN_SPI_MOSI / PIN_SPI_MISO
 *
 * ⚠ **SPIM00 은 P2 전용 고속 도메인이다** (`nrf54l_domains.h`). P1 에 두려면
 *   SPIM20~22 를 써야 하는데, 그 번호는 같은 번호의 UARTE/TWIM 과 **같은
 *   하드웨어 블록**이라 동시에 못 쓴다 (docs/PERIPHERAL-PINMAP.md §0).
 *
 * ⚠ **CS(SS) 는 이 라이브러리가 건드리지 않는다.** Arduino 관례대로 스케치가
 *   직접 내리고 올린다 — 장치마다 CS 가 여러 개인 경우가 흔하기 때문이다.
 */
#ifndef _SPI_H_
#define _SPI_H_

#include <Arduino.h>

extern "C" {
#include "nrfx_spim.h"
}

#define SPI_HAS_TRANSACTION   1

#define SPI_MODE0   0
#define SPI_MODE1   1
#define SPI_MODE2   2
#define SPI_MODE3   3

#define SPI_CLOCK_DIV2     (8000000)
#define SPI_CLOCK_DIV4     (4000000)
#define SPI_CLOCK_DIV8     (2000000)
#define SPI_CLOCK_DIV16    (1000000)
#define SPI_CLOCK_DIV32     (500000)
#define SPI_CLOCK_DIV64     (250000)
#define SPI_CLOCK_DIV128    (125000)

/* LSBFIRST / MSBFIRST 는 코어(wiring_constants.h)가 이미 정의한다. */

class SPISettings
{
  public:
    SPISettings(uint32_t clock, uint8_t bitOrder, uint8_t dataMode)
      : _clock(clock), _order(bitOrder), _mode(dataMode) { }

    /** 인자 없는 기본값은 Arduino 관례대로 4 MHz / MSB / 모드 0 이다. */
    SPISettings(void) : _clock(4000000), _order(MSBFIRST), _mode(SPI_MODE0) { }

    uint32_t clock(void) const    { return _clock; }
    uint8_t  bitOrder(void) const { return _order; }
    uint8_t  dataMode(void) const { return _mode;  }

  private:
    uint32_t _clock;
    uint8_t  _order, _mode;
};

class SPIClass
{
  public:
    SPIClass(NRF_SPIM_Type *spim, uint32_t pin_sck, uint32_t pin_mosi, uint32_t pin_miso);

    void begin(void);
    void end(void);

    /**
     * 핀을 바꾼다. **`begin()` 보다 먼저** 부른다.
     *
     * 기본값은 variant 의 `PIN_SPI_*` 다. 배선이 다른 보드나, 예제에서 다른
     * 핀으로 시험할 때 쓴다. 이미 `begin()` 한 뒤에 부르면 그 자리에서 다시 세운다.
     *
     * ⚠ **아무 핀이나 되지 않는다.** SPIM00 은 P2 만, SPIM20~22 는 P1 만 쓸 수
     *   있다 (`nrf54l_domains.h`). 도메인을 어기면 런타임에 조용히 동작하지 않는다.
     */
    void setPins(uint32_t pin_sck, uint32_t pin_mosi, uint32_t pin_miso);

    void beginTransaction(SPISettings settings);
    void endTransaction(void);

    void setBitOrder(uint8_t order);
    void setDataMode(uint8_t mode);
    void setClockDivider(uint32_t clock);      /* 값을 Hz 로 받는다 (위 매크로) */

    uint8_t  transfer(uint8_t data);
    uint16_t transfer16(uint16_t data);

    /** 제자리에서 주고받는다 (Arduino 규약). 보낸 버퍼가 받은 값으로 덮인다. */
    void transfer(void *buf, size_t count);

    /** 보내기만 / 받기만. 큰 블록을 다룰 때 제자리 버퍼가 필요 없다. */
    void transfer(const void *tx, void *rx, size_t count);

    /* 인터럽트 연동은 지원하지 않는다 — 상류도 빈 함수다. */
    void usingInterrupt(int) { }
    void notUsingInterrupt(int) { }

    /* 내부용 — 벡터가 부른다. */
    void _irqHandler(void);
    void _onEvent(void);

  private:
    nrfx_spim_t _spim;
    uint32_t    _pin_sck, _pin_mosi, _pin_miso;

    uint32_t    _clock;
    uint8_t     _order, _mode;
    bool        _begun;

    void       *_done_sem;

    void apply(void);
    bool run(const uint8_t *tx, size_t tx_len, uint8_t *rx, size_t rx_len);
};

extern SPIClass SPI;
#ifdef SPI1_SPIM_INSTANCE
extern SPIClass SPI1;
#endif

#endif
