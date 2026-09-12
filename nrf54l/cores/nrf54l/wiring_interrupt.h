/*
 * wiring_interrupt — attachInterrupt / detachInterrupt (GPIOTE)
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * ⚠ **P2 핀에는 걸 수 없다.** nRF54L 의 GPIOTE 는 두 개뿐이고 각자 자기 도메인의
 *   포트만 본다 — GPIOTE20 은 P1(채널 8개), GPIOTE30 은 P0(채널 4개). **P2 를
 *   담당하는 GPIOTE 가 아예 없다** (docs/PERIPHERAL-PINMAP.md §4, Pin Planner 확인).
 *   P2 핀을 주면 `attachInterrupt()` 가 **아무 일도 하지 않고 조용히 끝나는 대신**
 *   `attachInterruptOk()` 로 확인할 수 있게 했다.
 *
 * ⚠ 채널이 유한하다 — P1 에 8개, P0 에 4개. 다 쓰면 그 다음 요청은 실패한다.
 */
#ifndef _WIRING_INTERRUPT_H_
#define _WIRING_INTERRUPT_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*voidFuncPtr)(void);

/* Arduino 표준 모드. 값도 관례를 따른다. */
#define LOW_LEVEL   0      /* 레벨 트리거. Arduino 의 LOW 와 같은 의미 */
#define CHANGE      1
#define FALLING     2
#define RISING      3

/**
 * 핀이 바뀔 때 함수를 부른다.
 *
 * ⚠ 콜백은 **인터럽트 문맥**에서 돈다. 짧게 끝내고, 블로킹 API(`delay`,
 *   `Serial.print`, I2C/SPI 전송)를 부르지 마라. FreeRTOS API 는 `...FromISR`
 *   판만 쓸 수 있다 (CLAUDE.md §7 F2 — 우선순위 5~7 이어야 한다).
 *
 * @return 걸렸으면 true. P2 핀이거나 채널이 없으면 false.
 */
bool attachInterruptOk(uint32_t pin, voidFuncPtr callback, uint32_t mode);

/** 상류 호환 판. 실패해도 알 수 없으므로 새 코드는 위를 쓰는 편이 낫다. */
void attachInterrupt(uint32_t pin, voidFuncPtr callback, uint32_t mode);

void detachInterrupt(uint32_t pin);

#ifdef __cplusplus
}
#endif

#endif
