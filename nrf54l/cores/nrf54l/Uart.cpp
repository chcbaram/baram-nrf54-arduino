/*
 * Uart.cpp — UARTE 기반 HardwareSerial (nRF54L15)
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * Adafruit_nRF52_Arduino cores/nRF5/Uart.cpp 의 API 를 따르되
 * nrfx_uarte 드라이버 기반으로 다시 구현했다.
 */

#include "Arduino.h"
#include "Uart.h"

#include <hal/nrf_gpio.h>

/* ── 인스턴스 ─────────────────────────────────────────────────────────
 * NU54-DK 는 CP2102N USB 브리지가 UARTE30(P0.00~P0.03)에 물려 있다.
 * Nordic nRF54L15 DK 의 BOARD_APP_UARTE_* 와 같은 배선이다.
 * docs/NU54-DK.md 참조.
 */
static nrfx_uarte_t _uarte30 = NRFX_UARTE_INSTANCE(30);

Uart Serial(&_uarte30, PIN_SERIAL_RX, PIN_SERIAL_TX, PIN_SERIAL_CTS, PIN_SERIAL_RTS);

#if defined(PIN_SERIAL1_RX) && defined(PIN_SERIAL1_TX)
static nrfx_uarte_t _uarte20 = NRFX_UARTE_INSTANCE(20);
Uart Serial1(&_uarte20, PIN_SERIAL1_RX, PIN_SERIAL1_TX);
#endif

/* ── nrfx 이벤트 → 인스턴스 디스패치 ─────────────────────────────────── */
static void uarte_evt_handler(const nrfx_uarte_event_t *event, void *context)
{
  static_cast<Uart *>(context)->_irqHandler(event);
}

/* ─────────────────────────────────────────────────────────────────────
 * 생성자
 * ───────────────────────────────────────────────────────────────────── */
Uart::Uart(const nrfx_uarte_t *instance, uint32_t pinRX, uint32_t pinTX)
  : _instance(instance), _pinRX(pinRX), _pinTX(pinTX),
    _pinCTS(NRF54L_PIN_NC), _pinRTS(NRF54L_PIN_NC),
    _begun(false), _rxDmaIdx(0), _txBusy(false)
{
}

Uart::Uart(const nrfx_uarte_t *instance, uint32_t pinRX, uint32_t pinTX,
           uint32_t pinCTS, uint32_t pinRTS)
  : _instance(instance), _pinRX(pinRX), _pinTX(pinTX),
    _pinCTS(pinCTS), _pinRTS(pinRTS),
    _begun(false), _rxDmaIdx(0), _txBusy(false)
{
}

void Uart::setPins(uint32_t pinRX, uint32_t pinTX)
{
  setPins(pinRX, pinTX, NRF54L_PIN_NC, NRF54L_PIN_NC);
}

void Uart::setPins(uint32_t pinRX, uint32_t pinTX, uint32_t pinCTS, uint32_t pinRTS)
{
  if (_begun) return;   /* begin() 뒤에는 무시한다 */
  _pinRX  = pinRX;
  _pinTX  = pinTX;
  _pinCTS = pinCTS;
  _pinRTS = pinRTS;
}

/* ─────────────────────────────────────────────────────────────────────
 * begin / end
 * ───────────────────────────────────────────────────────────────────── */
void Uart::begin(unsigned long baudrate)
{
  begin(baudrate, (uint16_t) SERIAL_8N1);
}

void Uart::begin(unsigned long baudrate, uint16_t config)
{
  if (_begun) end();

  /* variant 는 Arduino 핀 번호를 주므로 절대 GPIO 번호로 바꾼다. */
  uint32_t tx = (_pinTX  < PINS_COUNT) ? g_ADigitalPinMap[_pinTX]  : NRF54L_PIN_NC;
  uint32_t rx = (_pinRX  < PINS_COUNT) ? g_ADigitalPinMap[_pinRX]  : NRF54L_PIN_NC;
  uint32_t cts= (_pinCTS < PINS_COUNT) ? g_ADigitalPinMap[_pinCTS] : NRF54L_PIN_NC;
  uint32_t rts= (_pinRTS < PINS_COUNT) ? g_ADigitalPinMap[_pinRTS] : NRF54L_PIN_NC;

  nrfx_uarte_config_t cfg = NRFX_UARTE_DEFAULT_CONFIG(
      (tx == NRF54L_PIN_NC) ? NRF_UARTE_PSEL_DISCONNECTED : tx,
      (rx == NRF54L_PIN_NC) ? NRF_UARTE_PSEL_DISCONNECTED : rx);

  if (cts != NRF54L_PIN_NC && rts != NRF54L_PIN_NC) {
    cfg.cts_pin = cts;
    cfg.rts_pin = rts;
    cfg.config.hwfc = NRF_UARTE_HWFC_ENABLED;
  }

  /* 보레이트. nrf_uarte_baudrate_t 값이 곧 레지스터 값이라
   * 흔한 값만 매핑하고 나머지는 115200 으로 떨어뜨린다. */
  switch (baudrate) {
    case   1200: cfg.baudrate = NRF_UARTE_BAUDRATE_1200;   break;
    case   2400: cfg.baudrate = NRF_UARTE_BAUDRATE_2400;   break;
    case   4800: cfg.baudrate = NRF_UARTE_BAUDRATE_4800;   break;
    case   9600: cfg.baudrate = NRF_UARTE_BAUDRATE_9600;   break;
    case  19200: cfg.baudrate = NRF_UARTE_BAUDRATE_19200;  break;
    case  38400: cfg.baudrate = NRF_UARTE_BAUDRATE_38400;  break;
    case  57600: cfg.baudrate = NRF_UARTE_BAUDRATE_57600;  break;
    case  76800: cfg.baudrate = NRF_UARTE_BAUDRATE_76800;  break;
    case 115200: cfg.baudrate = NRF_UARTE_BAUDRATE_115200; break;
    case 230400: cfg.baudrate = NRF_UARTE_BAUDRATE_230400; break;
    case 250000: cfg.baudrate = NRF_UARTE_BAUDRATE_250000; break;
    case 460800: cfg.baudrate = NRF_UARTE_BAUDRATE_460800; break;
    case 921600: cfg.baudrate = NRF_UARTE_BAUDRATE_921600; break;
    case 1000000:cfg.baudrate = NRF_UARTE_BAUDRATE_1000000;break;
    default:     cfg.baudrate = NRF_UARTE_BAUDRATE_115200; break;
  }

  /* HardwareSerial.h 의 SERIAL_xxx 비트필드를 해석한다. */
  if (config & SERIAL_PARITY_ENABLE) {
    cfg.config.parity = NRF_UARTE_PARITY_INCLUDED;
#if NRF_UARTE_HAS_PARITY_BIT
    cfg.config.paritytype = (config & SERIAL_PARITY_ODD)
                          ? NRF_UARTE_PARITYTYPE_ODD
                          : NRF_UARTE_PARITYTYPE_EVEN;
#endif
  }
#if NRF_UARTE_HAS_STOP_BITS
  cfg.config.stop = (config & SERIAL_STOP_TWO) ? NRF_UARTE_STOP_TWO
                                               : NRF_UARTE_STOP_ONE;
#endif

  /*
   * 인터럽트 우선순위. ...FromISR 계열을 이 핸들러에서 부르므로
   * configMAX_SYSCALL_INTERRUPT_PRIORITY(=5) 이상이어야 한다.
   * nrfx 기본값 6 을 쓴다 (CLAUDE.md §7 F2, nordic/nrfx_config.h).
   */
  cfg.interrupt_priority = NRFX_UARTE_DEFAULT_CONFIG_IRQ_PRIORITY;
  cfg.p_context = this;

  if (nrfx_uarte_init((nrfx_uarte_t *) _instance, &cfg, uarte_evt_handler) != 0) {
    return;
  }

  _rxBuffer.clear();
  _txBusy   = false;
  _rxDmaIdx = 0;

  /* 연속 수신 시작. 첫 버퍼를 주고, 이후는 RX_BUF_REQUEST 에서 번갈아 준다. */
  if (rx != NRF54L_PIN_NC) {
    nrfx_uarte_rx_buffer_set((nrfx_uarte_t *) _instance, _rxDma[0], RX_CHUNK);
    nrfx_uarte_rx_enable((nrfx_uarte_t *) _instance, NRFX_UARTE_RX_ENABLE_CONT);
  }

  _begun = true;
}

void Uart::end(void)
{
  if (!_begun) return;

  flush();
  nrfx_uarte_rx_abort((nrfx_uarte_t *) _instance, true, true);
  nrfx_uarte_uninit((nrfx_uarte_t *) _instance);

  _rxBuffer.clear();
  _begun = false;
  _txBusy = false;
}

/* ─────────────────────────────────────────────────────────────────────
 * 이벤트 핸들러 (ISR 컨텍스트)
 * ───────────────────────────────────────────────────────────────────── */
void Uart::_irqHandler(const nrfx_uarte_event_t *event)
{
  switch (event->type)
  {
    case NRFX_UARTE_EVT_RX_DONE:
      for (size_t i = 0; i < event->data.rx.length; i++) {
        _rxBuffer.store_char(event->data.rx.p_buffer[i]);
      }
      break;

    case NRFX_UARTE_EVT_RX_BUF_REQUEST:
      /* 다음 버퍼를 미리 건넨다. 안 주면 수신이 끊긴다. */
      _rxDmaIdx ^= 1;
      nrfx_uarte_rx_buffer_set((nrfx_uarte_t *) _instance, _rxDma[_rxDmaIdx], RX_CHUNK);
      break;

    case NRFX_UARTE_EVT_TX_DONE:
      _txBusy = false;
      break;

    case NRFX_UARTE_EVT_ERROR:
      /* 프레이밍/오버런. 버리고 계속 간다. */
      break;

    default:
      break;
  }
}

/* ─────────────────────────────────────────────────────────────────────
 * Stream / Print
 * ───────────────────────────────────────────────────────────────────── */
int Uart::available(void)          { return _rxBuffer.available(); }
int Uart::availableForWrite(void)  { return _txBusy ? 0 : SERIAL_BUFFER_SIZE; }
int Uart::peek(void)               { return _rxBuffer.peek(); }
int Uart::read(void)               { return _rxBuffer.read_char(); }

void Uart::flush(void)
{
  if (!_begun) return;
  while (_txBusy) {
    yield();
  }
}

size_t Uart::write(const uint8_t data)
{
  return write(&data, 1);
}

size_t Uart::write(const uint8_t *buffer, size_t size)
{
  if (!_begun || size == 0) return 0;

  size_t written = 0;

  while (written < size)
  {
    /* 직전 전송이 끝날 때까지 기다린다. EasyDMA 가 _txBuffer 를 읽는 중이다. */
    flush();

    size_t chunk = size - written;
    if (chunk > SERIAL_BUFFER_SIZE) chunk = SERIAL_BUFFER_SIZE;

    memcpy(_txBuffer, buffer + written, chunk);
    _txBusy = true;

    if (nrfx_uarte_tx((nrfx_uarte_t *) _instance, _txBuffer, chunk, 0) != 0) {
      _txBusy = false;
      break;
    }
    written += chunk;
  }

  return written;
}

/* printf 계열의 출력처 (syscalls.c 의 weak 심볼을 덮는다) */
extern "C" int nrf54l_serial_write_bytes(const char *buf, int len)
{
  return (int) Serial.write((const uint8_t *) buf, (size_t) len);
}
