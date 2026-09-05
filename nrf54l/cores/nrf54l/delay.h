/*
 * delay.h — 시간 (nRF54L15 / GRTC)
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 */
#ifndef _DELAY_H_
#define _DELAY_H_

#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Adafruit rtos.h 와 동일한 이름. 스케치가 그대로 쓴다. */
#define ms2tick(ms)     pdMS_TO_TICKS(ms)
#define tick2ms(tick)   ( ( (uint64_t)(tick) * 1000ULL ) / configTICK_RATE_HZ )

/** 부팅 후 경과 밀리초. configTICK_RATE_HZ = 1000 이라 틱과 1:1 이다. */
uint32_t millis(void);

/**
 * 부팅 후 경과 마이크로초.
 *
 * 틱 카운트가 아니라 GRTC SYSCOUNTER 를 직접 읽는다. SYSCOUNTER 는
 * 64비트 1 MHz 라 그 값이 곧 마이크로초다 (CLAUDE.md §7 F3).
 * nRF52 의 틱 기반 micros() 보다 해상도가 훨씬 높다.
 */
uint32_t micros(void);

/** 64비트 버전. 오버플로 없이 쓰고 싶을 때. */
uint64_t micros64(void);

/**
 * 지정 밀리초 동안 태스크를 재운다.
 *
 * ⚠ busy-wait 가 아니다. vTaskDelay() 로 양보하므로 다른 태스크가 돌고
 *   tickless 가 켜지면 CPU 가 잔다. 이게 FreeRTOS 를 넣은 이유 중 하나다
 *   (CLAUDE.md §6).
 */
void delay(uint32_t ms);

/** 마이크로초 busy-wait. 짧은 지연에만 쓸 것. */
void delayMicroseconds(uint32_t us);

#ifdef __cplusplus
}
#endif

#endif /* _DELAY_H_ */
