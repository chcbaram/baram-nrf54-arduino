/*
 * wiring_analog.h — analogWrite (PWM)
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * ⚠ **P1 핀에만 걸린다** (nRF54LM20A 는 P1 과 P3). PWM20/21/22 가 전부
 *   도메인 20 이고 P2·P0 를 담당하는 PWM 이 없다
 *   (docs/PERIPHERAL-PINMAP.md §4, Pin Planner 확인).
 *
 *   NU54-DK 에서 특히 조심하라 — **`LED_BUILTIN`(P2.09)과 `PIN_LED3`(P2.07)은
 *   P2 라 PWM 이 안 된다.** LED2(P1.10) 나 LED4(P1.14) 를 써라.
 *   XIAO nRF54L15 는 사용자 LED 가 P2.00 하나뿐이라 아예 안 된다.
 *
 * ⚠ 동시에 쓸 수 있는 핀은 **12개** 다. PWM20/21/22 각 4채널이다.
 *   다 쓰면 그 다음 요청은 아무 일도 하지 않는다 — 확인하려면
 *   `analogWriteOk()` 를 써라.
 */
#ifndef _WIRING_ANALOG_H_
#define _WIRING_ANALOG_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 핀에 PWM 을 출력한다.
 *
 * @param value 0 ~ (2^resolution - 1). 기본 해상도는 8비트라 0~255.
 *              0 이면 계속 LOW, 최대값이면 계속 HIGH 다.
 */
void analogWrite(uint32_t pin, uint32_t value);

/** 실패를 알려주는 판. P2 핀이거나 채널이 다 찼으면 false. */
bool analogWriteOk(uint32_t pin, uint32_t value);

/**
 * 해상도를 바꾼다 (1~15비트, 기본 8).
 *
 * ⚠ **주파수가 같이 바뀐다.** 기준 클럭이 1 MHz 로 고정이라
 *   주파수 = 1 MHz / 2^bits 다 — 8비트면 3.9 kHz, 12비트면 244 Hz.
 *   이미 출력 중인 핀에도 곧바로 적용된다.
 */
void analogWriteResolution(uint8_t bits);

/**
 * 핀의 PWM 을 놓는다. 핀은 입력으로 돌아간다.
 *
 * 보통은 직접 부를 필요가 없다 — `pinMode()` 나 `digitalWrite()` 를 하면
 * 자동으로 풀린다 (Arduino 관례).
 */
void analogWriteStop(uint32_t pin);

#ifdef __cplusplus
}
#endif

#endif /* _WIRING_ANALOG_H_ */
