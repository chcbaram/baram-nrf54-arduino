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

#ifdef __cplusplus
  #include "Uart.h"

  /* loop() 태스크 제어. Adafruit 과 같은 이름 (main.cpp). */
  void suspendLoop(void);
  void resumeLoop(void);

  using std::min;
  using std::max;
#endif

#endif /* Arduino_h */
