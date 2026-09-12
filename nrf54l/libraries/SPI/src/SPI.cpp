/*
 * SPI — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 */
#include "SPI.h"
#include "variant.h"
#include <string.h>

#include "FreeRTOS.h"
#include "semphr.h"

/** 한 번의 전송을 기다리는 한계. 걸리면 조용히 멈추는 대신 돌아온다. */
#define SPI_XFER_TIMEOUT_MS   (1000)

/**
 * 한 번에 넘길 수 있는 최대 바이트.
 * 더 길면 나눠 보낸다 — EasyDMA 의 한 번 길이 제한과 무관하게 스택을 쓰지 않는다.
 */
#define SPI_CHUNK_MAX         (255)

static void spim_event_handler(const nrfx_spim_event_t *evt, void *ctx)
{
  (void) evt;
  ((SPIClass *) ctx)->_onEvent();
}

SPIClass::SPIClass(NRF_SPIM_Type *spim, uint32_t pin_sck, uint32_t pin_mosi, uint32_t pin_miso)
  : _spim(NRFX_SPIM_INSTANCE(spim))
{
  _pin_sck  = pin_sck;
  _pin_mosi = pin_mosi;
  _pin_miso = pin_miso;

  _clock = 4000000;
  _order = MSBFIRST;
  _mode  = SPI_MODE0;
  _begun = false;

  _done_sem = NULL;
}

void SPIClass::apply(void)
{
  nrfx_spim_config_t cfg = NRFX_SPIM_DEFAULT_CONFIG(_pin_sck, _pin_mosi, _pin_miso,
                                                    NRF_SPIM_PIN_NOT_CONNECTED);
  cfg.frequency = _clock;
  cfg.bit_order = (_order == LSBFIRST) ? NRF_SPIM_BIT_ORDER_LSB_FIRST
                                       : NRF_SPIM_BIT_ORDER_MSB_FIRST;
  switch (_mode) {
    case SPI_MODE1: cfg.mode = NRF_SPIM_MODE_1; break;
    case SPI_MODE2: cfg.mode = NRF_SPIM_MODE_2; break;
    case SPI_MODE3: cfg.mode = NRF_SPIM_MODE_3; break;
    default:        cfg.mode = NRF_SPIM_MODE_0; break;
  }

  if (_begun) {
    nrfx_spim_reconfigure(&_spim, &cfg);
    return;
  }

  /* nrfx 4.x 는 0 이 성공, 음수가 오류다 (CLAUDE.md §7 F10 ①). */
  int err = nrfx_spim_init(&_spim, &cfg, spim_event_handler, this);
  if (err != 0 && err != -EALREADY) return;
  _begun = true;
}

void SPIClass::begin(void)
{
  if (_done_sem == NULL) {
    _done_sem = (void *) xSemaphoreCreateBinary();
    if (_done_sem == NULL) return;
  }
  apply();
}

void SPIClass::setPins(uint32_t pin_sck, uint32_t pin_mosi, uint32_t pin_miso)
{
  _pin_sck  = pin_sck;
  _pin_mosi = pin_mosi;
  _pin_miso = pin_miso;

  /*
   * 이미 돌고 있으면 핀만 바꿀 수 없다 — nrfx 의 reconfigure 는 핀을 다시
   * 잡아 주지 않는다. 내렸다가 다시 올린다.
   */
  if (_begun) { nrfx_spim_uninit(&_spim); _begun = false; apply(); }
}

void SPIClass::end(void)
{
  if (!_begun) return;
  nrfx_spim_uninit(&_spim);
  _begun = false;
}

void SPIClass::beginTransaction(SPISettings settings)
{
  _clock = settings.clock();
  _order = settings.bitOrder();
  _mode  = settings.dataMode();
  apply();
}

void SPIClass::endTransaction(void) { }

void SPIClass::setBitOrder(uint8_t order)     { _order = order; apply(); }
void SPIClass::setDataMode(uint8_t mode)      { _mode  = mode;  apply(); }
void SPIClass::setClockDivider(uint32_t clock){ _clock = clock; apply(); }

/*------------------------------------------------------------------*/

void SPIClass::_onEvent(void)
{
  BaseType_t woken = pdFALSE;
  xSemaphoreGiveFromISR((SemaphoreHandle_t) _done_sem, &woken);
  portYIELD_FROM_ISR(woken);
}

void SPIClass::_irqHandler(void)
{
  nrfx_spim_irq_handler(&_spim);
}

bool SPIClass::run(const uint8_t *tx, size_t tx_len, uint8_t *rx, size_t rx_len)
{
  if (!_begun) return false;

  while (xSemaphoreTake((SemaphoreHandle_t) _done_sem, 0) == pdTRUE) { }

  nrfx_spim_xfer_desc_t desc;
  desc.p_tx_buffer = (uint8_t *) tx;
  desc.tx_length   = tx_len;
  desc.p_rx_buffer = rx;
  desc.rx_length   = rx_len;

  if (nrfx_spim_xfer(&_spim, &desc, 0) != 0) return false;

  /*
   * 인터럽트가 깨워 줄 때까지 기다린다. 폴링으로 돌면 tickless 가 잠들지 못하고
   * 그동안 다른 태스크도 못 돈다 (Wire 와 같은 판단).
   */
  return xSemaphoreTake((SemaphoreHandle_t) _done_sem,
                        pdMS_TO_TICKS(SPI_XFER_TIMEOUT_MS)) == pdTRUE;
}

uint8_t SPIClass::transfer(uint8_t data)
{
  uint8_t rx = 0;
  if (!run(&data, 1, &rx, 1)) return 0;
  return rx;
}

uint16_t SPIClass::transfer16(uint16_t data)
{
  uint8_t tx[2], rx[2] = { 0, 0 };

  /* 비트 순서에 맞춰 바이트도 뒤집는다 — Arduino 규약이다. */
  if (_order == MSBFIRST) { tx[0] = data >> 8;  tx[1] = data & 0xFF; }
  else                    { tx[0] = data & 0xFF; tx[1] = data >> 8;  }

  if (!run(tx, 2, rx, 2)) return 0;

  return (_order == MSBFIRST) ? (uint16_t) ((rx[0] << 8) | rx[1])
                              : (uint16_t) ((rx[1] << 8) | rx[0]);
}

void SPIClass::transfer(const void *tx, void *rx, size_t count)
{
  const uint8_t *t = (const uint8_t *) tx;
  uint8_t       *r = (uint8_t *) rx;

  while (count) {
    size_t n = (count > SPI_CHUNK_MAX) ? SPI_CHUNK_MAX : count;
    if (!run(t, t ? n : 0, r, r ? n : 0)) return;
    if (t) t += n;
    if (r) r += n;
    count -= n;
  }
}

void SPIClass::transfer(void *buf, size_t count)
{
  /* 제자리 교환. nrfx 는 같은 버퍼를 TX·RX 로 함께 받아도 된다. */
  transfer(buf, buf, count);
}

/*------------------------------------------------------------------*/
/* 인스턴스 — variant 가 고른다                                       */
/*------------------------------------------------------------------*/

#ifdef SPI_SPIM_INSTANCE
  #ifndef SPI_SPIM_IRQ_HANDLER
    #error "variant.h 에서 SPI_SPIM_IRQ_HANDLER 도 정의해야 한다 (예: SERIAL00_IRQHandler)"
  #endif

SPIClass SPI(SPI_SPIM_INSTANCE, PIN_SPI_SCK, PIN_SPI_MOSI, PIN_SPI_MISO);

/*
 * ⚠ 다중 인스턴스 드라이버라 벡터를 직접 이어야 한다 (CLAUDE.md §7 F10 ③).
 *   링크 후 `nm <elf> | grep <핸들러>` 가 `T` 인지 확인하라. `W` 면 미연결이다.
 */
extern "C" void SPI_SPIM_IRQ_HANDLER(void) { SPI._irqHandler(); }
#endif

#ifdef SPI1_SPIM_INSTANCE
  #ifndef SPI1_SPIM_IRQ_HANDLER
    #error "variant.h 에서 SPI1_SPIM_IRQ_HANDLER 도 정의해야 한다"
  #endif

SPIClass SPI1(SPI1_SPIM_INSTANCE, PIN_SPI1_SCK, PIN_SPI1_MOSI, PIN_SPI1_MISO);

extern "C" void SPI1_SPIM_IRQ_HANDLER(void) { SPI1._irqHandler(); }
#endif
