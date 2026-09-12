/*
 * Wire — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 */
#include "Wire.h"
#include "variant.h"
#include <string.h>

#include "FreeRTOS.h"
#include "semphr.h"


/** 한 번의 전송을 기다리는 한계. 상대가 클럭을 잡고 놓지 않는 경우를 끊는다. */
#define WIRE_XFER_TIMEOUT_MS   (1000)

/*------------------------------------------------------------------*/

static void twim_event_handler(const nrfx_twim_event_t *evt, void *ctx)
{
  ((TwoWire *) ctx)->_onEvent(evt);
}

TwoWire::TwoWire(NRF_TWIM_Type *twim, uint32_t pin_sda, uint32_t pin_scl)
  : _twim(NRFX_TWIM_INSTANCE(twim))
{
  _pin_sda = pin_sda;
  _pin_scl = pin_scl;
  _freq    = 100000;
  _begun   = false;

  _addr        = 0;
  _tx_len      = 0;
  _tx_overflow = false;
  _rx_len      = _rx_pos = 0;

  _done_sem = NULL;
  _last_evt = NRFX_TWIM_EVT_DONE;
}

void TwoWire::begin(void)
{
  if (_begun) return;

  if (_done_sem == NULL) {
    _done_sem = (void *) xSemaphoreCreateBinary();
    if (_done_sem == NULL) return;
  }

  nrfx_twim_config_t cfg = NRFX_TWIM_DEFAULT_CONFIG(_pin_scl, _pin_sda);
  cfg.frequency = (_freq >= 400000) ? NRF_TWIM_FREQ_400K :
                  (_freq >= 250000) ? NRF_TWIM_FREQ_250K : NRF_TWIM_FREQ_100K;

  /*
   * ⚠ nrfx 4.x 는 0 이 성공이고 음수가 오류다 (CLAUDE.md §7 F10 ①).
   *   -EALREADY 는 이미 초기화된 것이라 그대로 쓴다.
   */
  int err = nrfx_twim_init(&_twim, &cfg, twim_event_handler, this);
  if (err != 0 && err != -EALREADY) return;

  nrfx_twim_enable(&_twim);
  _begun = true;
}

void TwoWire::end(void)
{
  if (!_begun) return;
  nrfx_twim_disable(&_twim);
  nrfx_twim_uninit(&_twim);
  _begun = false;
}

void TwoWire::setClock(uint32_t freq)
{
  _freq = freq;
  if (!_begun) return;

  /* 이미 돌고 있으면 다시 세운다. 핀은 그대로다. */
  nrfx_twim_config_t cfg = NRFX_TWIM_DEFAULT_CONFIG(_pin_scl, _pin_sda);
  cfg.frequency = (_freq >= 400000) ? NRF_TWIM_FREQ_400K :
                  (_freq >= 250000) ? NRF_TWIM_FREQ_250K : NRF_TWIM_FREQ_100K;
  nrfx_twim_reconfigure(&_twim, &cfg);
}

/*------------------------------------------------------------------*/
/* 전송                                                              */
/*------------------------------------------------------------------*/

void TwoWire::_onEvent(const nrfx_twim_event_t *evt)
{
  _last_evt = evt->type;

  BaseType_t woken = pdFALSE;
  xSemaphoreGiveFromISR((SemaphoreHandle_t) _done_sem, &woken);
  portYIELD_FROM_ISR(woken);
}

void TwoWire::_irqHandler(void)
{
  nrfx_twim_irq_handler(&_twim);
}

bool TwoWire::transfer(const nrfx_twim_xfer_desc_t *desc, uint32_t flags)
{
  if (!_begun) return false;

  /* 앞선 전송이 남긴 신호를 비운다. */
  while (xSemaphoreTake((SemaphoreHandle_t) _done_sem, 0) == pdTRUE) { }

  if (nrfx_twim_xfer(&_twim, desc, flags) != 0) return false;

  /*
   * 완료를 **기다린다.** 인터럽트가 깨워 주므로 그동안 다른 태스크가 돈다 —
   * 폴링으로 돌면 tickless 가 잠들지 못하고 CPU 도 그만큼 태운다.
   */
  if (xSemaphoreTake((SemaphoreHandle_t) _done_sem,
                     pdMS_TO_TICKS(WIRE_XFER_TIMEOUT_MS)) != pdTRUE) {
    return false;
  }
  return _last_evt == NRFX_TWIM_EVT_DONE;
}

void TwoWire::beginTransmission(uint8_t address)
{
  _addr        = address;
  _tx_len      = 0;
  _tx_overflow = false;
}

uint8_t TwoWire::endTransmission(bool stopBit)
{
  if (_tx_overflow) { _tx_len = 0; return 1; }      /* Arduino: 1 = 버퍼 초과 */

  nrfx_twim_xfer_desc_t desc = NRFX_TWIM_XFER_DESC_TX(_addr, _tx, _tx_len);
  uint32_t flags = stopBit ? 0 : NRFX_TWIM_FLAG_TX_NO_STOP;

  bool ok = transfer(&desc, flags);
  _tx_len = 0;

  if (ok) return 0;

  /* 라이브러리들이 이 숫자로 분기하므로 원인을 구분해 준다. */
  switch (_last_evt) {
    case NRFX_TWIM_EVT_ADDRESS_NACK: return 2;
    case NRFX_TWIM_EVT_DATA_NACK:    return 3;
    default:                         return 4;
  }
}

uint8_t TwoWire::requestFrom(uint8_t address, size_t quantity, bool stopBit)
{
  (void) stopBit;                                   /* RX 는 항상 STOP 으로 끝낸다 */

  if (quantity == 0) return 0;
  if (quantity > WIRE_BUFFER_SIZE) quantity = WIRE_BUFFER_SIZE;

  _rx_len = _rx_pos = 0;

  nrfx_twim_xfer_desc_t desc = NRFX_TWIM_XFER_DESC_RX(address, _rx, quantity);
  if (!transfer(&desc, 0)) return 0;

  _rx_len = quantity;
  return (uint8_t) quantity;
}

/*------------------------------------------------------------------*/
/* Stream                                                            */
/*------------------------------------------------------------------*/

size_t TwoWire::write(uint8_t data)
{
  if (_tx_len >= WIRE_BUFFER_SIZE) { _tx_overflow = true; return 0; }
  _tx[_tx_len++] = data;
  return 1;
}

size_t TwoWire::write(const uint8_t *data, size_t len)
{
  size_t n = 0;
  while (n < len && write(data[n])) n++;
  return n;
}

int TwoWire::available(void) { return (int) (_rx_len - _rx_pos); }

int TwoWire::read(void)
{
  if (_rx_pos >= _rx_len) return -1;
  return _rx[_rx_pos++];
}

int TwoWire::peek(void)
{
  if (_rx_pos >= _rx_len) return -1;
  return _rx[_rx_pos];
}

void TwoWire::flush(void) { }

/*------------------------------------------------------------------*/
/* 인스턴스 — variant 가 고른다                                       */
/*------------------------------------------------------------------*/

#ifdef WIRE_TWIM_INSTANCE
  #ifndef WIRE_TWIM_IRQ_HANDLER
    #error "variant.h 에서 WIRE_TWIM_IRQ_HANDLER 도 정의해야 한다 (예: SERIAL22_IRQHandler)"
  #endif

TwoWire Wire(WIRE_TWIM_INSTANCE, PIN_WIRE_SDA, PIN_WIRE_SCL);

/*
 * ⚠ 다중 인스턴스 드라이버라 **벡터를 직접 이어야 한다** (CLAUDE.md §7 F10 ③).
 *   nrfx 는 인스턴스 포인터를 받는 핸들러 하나만 제공하므로, 어느 벡터가 어느
 *   인스턴스인지는 우리가 알려 줘야 한다. 안 이으면 MDK 의 weak Default_Handler
 *   가 남아 인터럽트가 뜨는 순간 무한루프다.
 *   링크 후 `nm <elf> | grep SERIAL22_IRQHandler` 가 `T` 인지 확인하라.
 */
extern "C" void WIRE_TWIM_IRQ_HANDLER(void) { Wire._irqHandler(); }
#endif

#ifdef WIRE1_TWIM_INSTANCE
  #ifndef WIRE1_TWIM_IRQ_HANDLER
    #error "variant.h 에서 WIRE1_TWIM_IRQ_HANDLER 도 정의해야 한다"
  #endif

TwoWire Wire1(WIRE1_TWIM_INSTANCE, PIN_WIRE1_SDA, PIN_WIRE1_SCL);

extern "C" void WIRE1_TWIM_IRQ_HANDLER(void) { Wire1._irqHandler(); }
#endif
