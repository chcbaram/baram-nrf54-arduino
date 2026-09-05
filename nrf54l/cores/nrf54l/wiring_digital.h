/*
 * wiring_digital.h — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 */
#ifndef _WIRING_DIGITAL_H_
#define _WIRING_DIGITAL_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 핀 모드를 설정한다.
 * @param dwMode INPUT, INPUT_PULLUP, INPUT_PULLDOWN, OUTPUT,
 *               OUTPUT_S0S1(기본), OUTPUT_H0H1(고전류) 등 wiring_constants.h 참조
 */
void pinMode(uint32_t dwPin, uint32_t dwMode);

void digitalWrite(uint32_t dwPin, uint32_t dwVal);
int  digitalRead(uint32_t ulPin);
void digitalToggle(uint32_t pin);

/*
 * LED 전용 헬퍼. variant 의 LED_STATE_ON 을 반영하므로 보드마다
 * LED 극성이 달라도 스케치를 고치지 않아도 된다.
 *
 * ⚠ NU54-DK 의 LED 는 active HIGH 다 (N-MOSFET 게이트 구동).
 *   Adafruit nRF52 보드 대부분은 active LOW 라 반대다. docs/NU54-DK.md 참조.
 */
void ledOn(uint32_t pin);
void ledOff(uint32_t pin);

#ifdef __cplusplus
}
#endif

#endif /* _WIRING_DIGITAL_H_ */
