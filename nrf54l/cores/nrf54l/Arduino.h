/*
 * Arduino.h — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 */
#ifndef Arduino_h
#define Arduino_h

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef __cplusplus
  #include <algorithm>
#endif

#include "nrf.h"
#include "FreeRTOSConfig.h"   /* configMAX_SYSCALL_INTERRUPT_PRIORITY — 아래 noInterrupts() */

#include "binary.h"
#include "itoa.h"
#include "wiring_constants.h"

/* Arduino 전통 타입. WCharacter.h 등이 쓴다. */
typedef bool     boolean;
typedef uint8_t  byte;
typedef uint16_t word;

#ifdef __cplusplus
extern "C" {
#endif

void setup(void);
void loop(void);
void yield(void);

/* setup() 전에 코어가 부르는 것들 */
void init(void);
void initVariant(void);

#ifdef __cplusplus
}
#endif

#include "WVariant.h"
#include "wiring.h"
#include "wiring_digital.h"
#include "wiring_interrupt.h"
#include "wiring_analog.h"
#include "wiring_shift.h"
#include "delay.h"

#ifdef __cplusplus
  #include "WCharacter.h"
  #include "WString.h"
  #include "WMath.h"
  #include "Print.h"
  #include "Printable.h"
  #include "Stream.h"
  #include "HardwareSerial.h"
  #include "RingBuffer.h"
  #include "rtos.h"
#endif

/* variant 가 핀 정의를 준다. */
#include "variant.h"

/*
 * ── Arduino 표준 매크로 ──────────────────────────────────────────────
 *
 * 스케치와 라이브러리가 당연히 있다고 가정하는 것들이다. 하나라도 없으면
 * 엉뚱한 곳에서 "was not declared in this scope" 가 난다 —
 * `Adafruit_seesaw` 가 `constrain` 에서 그렇게 깨졌다.
 *
 * ⚠ `min`/`max` 는 C++ 에서 `<algorithm>` 과 충돌하므로 건드리지 않는다.
 *   Arduino 는 C++ 빌드에서 이 둘을 표준 템플릿에 맡긴다.
 */
#ifndef abs
  #define abs(x)         ((x) > 0 ? (x) : -(x))
#endif
#define constrain(amt, low, high)  ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
#define round(x)         ((x) >= 0 ? (long)((x) + 0.5) : (long)((x) - 0.5))
#define radians(deg)     ((deg) * DEG_TO_RAD)
#define degrees(rad)     ((rad) * RAD_TO_DEG)
#define sq(x)            ((x) * (x))

#define lowByte(w)       ((uint8_t) ((w) & 0xff))
#define highByte(w)      ((uint8_t) ((w) >> 8))

#define bitRead(value, bit)            (((value) >> (bit)) & 0x01ul)
#define bitSet(value, bit)             ((value) |= (1UL << (bit)))
#define bitClear(value, bit)           ((value) &= ~(1UL << (bit)))
#define bitWrite(value, bit, bitvalue) ((bitvalue) ? bitSet(value, bit) : bitClear(value, bit))
#ifndef bit
  #define bit(b)         (1UL << (b))
#endif

/*
 * ── AVR 호환 포트 셰임 ───────────────────────────────────────────────
 *
 * `Adafruit_BusIO` 를 비롯한 라이브러리들이 AVR 시절의 포트 매크로를 쓴다.
 * 없으면 **I2C 만 쓰는 스케치도** 컴파일이 깨진다 — BusIO 의 SPI 쪽 소스가
 * 조건 없이 컴파일되기 때문이다. Adafruit nRF52 코어와 같은 이름·의미로 둔다
 * (CLAUDE.md R12).
 *
 * ⚠ **Adafruit 판을 그대로 쓸 수 없다.** 그쪽은 포트가 둘(P0/P1)이라
 *   `abs < 32 ? NRF_P0 : NRF_P1` 로 끝나는데, nRF54L 은 **셋**(P0/P1/P2)이고
 *   nRF54LM20A 는 **넷**이다 (docs/boards/NU54-DK.md 의 주의 사항).
 */
static inline NRF_GPIO_Type *nrf54lPortRegs(uint32_t abs_pin)
{
  switch (abs_pin >> 5) {
    case 0:  return NRF_P0;
    case 1:  return NRF_P1;
#if defined(NRF_P3)
    case 3:  return NRF_P3;
#endif
    default: return NRF_P2;
  }
}

#define digitalPinToPort(P)      ( nrf54lPortRegs(g_ADigitalPinMap[P]) )
#define digitalPinToBitMask(P)   ( 1UL << (g_ADigitalPinMap[P] & 31) )
#define digitalPinToPinName(P)   ( g_ADigitalPinMap[P] )

#define portOutputRegister(port) ( &((port)->OUT) )
#define portInputRegister(port)  ( (volatile uint32_t *) &((port)->IN) )
#define portModeRegister(port)   ( &((port)->DIR) )

/*
 * ⚠ Adafruit 은 `digitalPinHasPWM(P)` 를 `P > 1` 로 대충 정의해 두었다.
 *   우리는 진짜로 답할 수 있다 — PWM20/21/22 가 전부 도메인 20 이라
 *   **P1 에만** 붙는다 (LM20A 는 P3 도). 표는 Pin Planner 에서 생성된다.
 */
#define digitalPinHasPWM(P)      ( NRF54L_SIG_PWM20_CHAN_0(g_ADigitalPinMap[P]) )

/*
 * 인터럽트 마스킹 (Arduino 표준 API).
 *
 * ⚠ **`__disable_irq()`(PRIMASK)를 쓰면 안 된다.** 그건 SoftDevice 의
 *   zero-latency 인터럽트(RADIO 등, 우선순위 0)까지 막아서 라디오 타이밍이
 *   깨진다. 이 프로젝트가 tickless idle 에서 한 번 크게 물린 지점이다
 *   (CLAUDE.md §7 F9).
 *
 *   대신 BASEPRI 를 올려 **애플리케이션 우선순위만** 막는다. BASEPRI 는
 *   설정값보다 긴급한(숫자가 작은) 인터럽트를 막지 못하므로 SoftDevice 는
 *   그대로 돈다. 경계는 FreeRTOS 의 configMAX_SYSCALL_INTERRUPT_PRIORITY 다.
 *
 * ⚠ 중첩을 세지 않는다 (Arduino 의미와 같다). 중첩이 필요하면 FreeRTOS 의
 *   taskENTER_CRITICAL() 을 쓴다.
 */
#define noInterrupts()  __set_BASEPRI(configMAX_SYSCALL_INTERRUPT_PRIORITY)
#define interrupts()    __set_BASEPRI(0)

#ifdef __cplusplus
  #include "Uart.h"

  /* loop() 태스크 제어. Adafruit 과 같은 이름 (main.cpp). */
  void suspendLoop(void);
  void resumeLoop(void);

  using std::min;
  using std::max;
#endif

#endif /* Arduino_h */
