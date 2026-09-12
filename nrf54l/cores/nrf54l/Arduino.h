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
