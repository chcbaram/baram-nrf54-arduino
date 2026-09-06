/*
 * Uart.h — UARTE 기반 HardwareSerial (nRF54L15)
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * Adafruit_nRF52_Arduino cores/nRF5/Uart.h 와 같은 API 다.
 *
 * ⚠ Adafruit nRF52840 에서는 `Serial` 이 TinyUSB CDC 였지만,
 *   nRF54L15 에는 USB 하드웨어가 없다 (CLAUDE.md R10).
 *   여기서는 `Serial` 이 진짜 UART 다.
 */
#ifndef _UART_H_
#define _UART_H_

#include "HardwareSerial.h"
#include "RingBuffer.h"

#include <nrfx.h>
#include <nrfx_uarte.h>

#ifndef SERIAL_BUFFER_SIZE
  #define SERIAL_BUFFER_SIZE 256
#endif

class Uart : public HardwareSerial
{
public:
  Uart(const nrfx_uarte_t *instance, uint32_t pinRX, uint32_t pinTX);

  /** begin() 전에만 유효하다. */
  void setPins(uint32_t pinRX, uint32_t pinTX);

  void begin(unsigned long baudrate);
  void begin(unsigned long baudrate, uint16_t config);

  /*
   * 하드웨어 흐름제어(RTS/CTS)는 지원하지 않는다. 프로젝트 결정 사항이다.
   *
   * NU54-DK 는 CP2102N 에 RTS/CTS 가 배선돼 있지만 쓰지 않는다. HWFC 를 켜면
   * UARTE 가 상대의 CTS 어서트를 기다려 한 바이트도 내보내지 않는데, 호스트
   * 터미널이 RTS 를 올리지 않는 것이 보통이라 Serial.println() 이 그대로
   * 멈춘다. 실제로 이 증상을 겪었고, Arduino 의 Serial 기대 동작도 아니다.
   *
   * 결과적으로 P0.02 / P0.03 은 일반 GPIO 로 쓸 수 있다.
   */

  /**
   * UARTE 를 완전히 내린다.
   *
   * ⚠ 저전력에 직결된다. UARTE 가 켜져 있으면 바닥 전류가 올라간다.
   *   Nordic 의 ble_pwr_profiling 샘플이 전력 최적화를 위해
   *   CONFIG_CONSOLE=n 을 거는 것과 같은 이유다 (CLAUDE.md §6.1).
   *   System OFF 나 장시간 슬립 전에는 반드시 부를 것.
   */
  void end(void);

  int    available(void);
  int    availableForWrite(void);
  int    peek(void);
  int    read(void);
  void   flush(void);
  size_t write(const uint8_t data);
  size_t write(const uint8_t *buffer, size_t size);
  using Print::write;

  operator bool(void) { return _begun; }

  /* 코어 내부용 */
  void _irqHandler(const nrfx_uarte_event_t *event);

private:
  const nrfx_uarte_t *_instance;
  uint32_t _pinRX, _pinTX;
  bool     _begun;

  RingBuffer  _rxBuffer;

  /*
   * nrfx UARTE 는 EasyDMA 라 수신 버퍼를 미리 줘야 한다.
   * 두 개를 번갈아 준다(더블 버퍼링). 하나가 채워지는 동안 다른 하나를
   * 드라이버에 건네야 바이트를 흘리지 않는다.
   *
   * ⚠ **1 이어야 한다.** 완료 이벤트는 버퍼가 **다 찼을 때만** 온다.
   *   32 였을 때 실기 증상: 2바이트·5바이트를 보내면 아무것도 안 올라오고,
   *   31바이트를 더 보내자 그제서야 32개가 한꺼번에 쏟아졌다.
   *   대화형 시리얼 입력이 사실상 불가능했다.
   *
   *   대안으로 UARTE 의 FRAMETIMEOUT(유휴 감지)으로 부분 버퍼를 끊는 길이
   *   있지만, **nRF54L 에서 그 이벤트가 수신 도중 걸리면 뒤따르는 데이터의
   *   비트가 깨진다**고 Nordic 이 확인했다. 회피책은 RXDRDY 를 DPPI 로 TIMER
   *   카운터에 물려 바이트를 세는 것인데(NCS 3.1 의
   *   UARTE_NRFX_UARTE_COUNT_BYTES_WITH_TIMER), TIMER 인스턴스와 DPPI 채널을
   *   잡아먹는다. Arduino 코어가 그 자원을 선점할 이유가 없다.
   *
   *   Adafruit nRF52 코어도 같은 이유로 `RXD.MAXCNT = 1` 로 바이트마다 ENDRX 를
   *   받는다. 115200 에서 초당 약 11.5k 인터럽트이고 ISR 이 짧아 감당된다.
   *   아주 높은 보드레이트가 필요해지면 그때 TIMER + DPPI 방식을 검토한다.
   */
  static const size_t RX_CHUNK = 1;
  uint8_t     _rxDma[2][RX_CHUNK];
  uint8_t     _rxDmaIdx;

  /* TX 는 DMA 가 읽어가는 동안 살아 있어야 하므로 멤버로 둔다. */
  uint8_t     _txBuffer[SERIAL_BUFFER_SIZE];
  volatile bool _txBusy;
};

extern Uart Serial;

#if defined(PIN_SERIAL1_RX) && defined(PIN_SERIAL1_TX)
  extern Uart Serial1;
#endif

#endif /* _UART_H_ */
