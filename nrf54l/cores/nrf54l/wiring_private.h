/*
 * wiring_private.h — 코어 내부 공용 헬퍼
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * 스케치가 include 할 헤더가 아니다. wiring_*.c 끼리 나눠 쓰는 것만 둔다.
 */
#ifndef _WIRING_PRIVATE_H_
#define _WIRING_PRIVATE_H_

#include <stdint.h>
#include <stdbool.h>
#include "WVariant.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Arduino 핀 번호 → 절대 GPIO 번호.
 *
 * ⚠ **핀을 만지는 모든 코어 코드가 이걸 거쳐야 한다.** 우리 variant 는 항등
 *   매핑이지만 `g_ADigitalPinMap` 에는 `NRF54L_PIN_NC` 가 섞여 있다 —
 *   모듈이 뽑아내지 않은 핀과, GPIO 로 쓰면 안 되는 핀(LFXO 의 XL1/XL2 등)이다.
 *   건너뛰면 그런 핀에 페리페럴이 붙어 **크리스털이 죽는 식으로** 조용히 망가진다.
 *
 * @return 쓸 수 있는 핀이면 true. 범위 밖이거나 NC 면 false.
 */
static inline bool nrf54lPinResolve(uint32_t arduino_pin, uint32_t *out_abs)
{
    if (arduino_pin >= PINS_COUNT) {
        return false;
    }
    uint32_t abs = g_ADigitalPinMap[arduino_pin];
    if (abs == NRF54L_PIN_NC) {
        return false;
    }
    *out_abs = abs;
    return true;
}

/**
 * `analogWrite()` 가 잡고 있는 핀을 놓는 훅.
 *
 * ⚠ **함수 포인터인 것이 중요하다.** `wiring_digital.c` 가 PWM 코드를 직접
 *   부르면 `--whole-archive` + `--gc-sections` 아래에서 **PWM 드라이버가 모든
 *   스케치에 링크된다** (CLAUDE.md §7 F13 ①, `Wire` 때 겪은 것). 포인터로 두면
 *   `analogWrite()` 를 한 번도 안 부르는 스케치는 비용이 0 이다.
 */
typedef void (*pwm_release_fn_t)(uint32_t arduino_pin);
extern pwm_release_fn_t g_pwmRelease;

/**
 * ⭐ **IRQ 벡터를 함수 포인터로 트램폴린하라.**
 *
 * 벡터 테이블은 링커 스크립트가 `KEEP` 한다. 그래서 벡터가 드라이버의
 * 핸들러를 **직접** 부르면, `--gc-sections` 이 그 드라이버를 절대 못 걷어낸다.
 * 스케치가 그 기능을 한 번도 안 써도 플래시를 문다.
 *
 * 실측 (blink, NU54-DK):
 *
 *     기준선                        23,696 B
 *     GPIOTE 를 직접 부르면         +1,568 B
 *     PWM 을 직접 부르면              +576 B
 *
 * 포인터를 한 겹 두면 벡터는 트램폴린만 붙잡고, 진짜 핸들러는 그것을
 * **설정하는 코드**(즉 attachInterrupt / analogWrite) 에서만 참조된다.
 * 안 쓰면 통째로 사라지고 남는 비용은 트램폴린 + 포인터 ~16 B 다.
 *
 *     static irq_vector_fn_t s_isr;
 *     static void dispatch(void) { nrfx_xxx_irq_handler(&m_inst); }
 *     void XXX_IRQHandler(void)  { if (s_isr) s_isr(); }
 *     ...  s_isr = dispatch;      // 초기화 경로에서 단다
 *
 * ⚠ 벡터 자체는 **반드시 정의해 두어라.** nrfx 가 NVIC 라인을 무조건 켜므로
 *   (`nrfy_xxx_int_init` 의 `NRFX_IRQ_ENABLE`), 안 이으면 MDK 의 weak
 *   Default_Handler 가 남아 무한루프다 (CLAUDE.md §7 F10 ③).
 */
typedef void (*irq_vector_fn_t)(void);

#ifdef __cplusplus
}
#endif

#endif /* _WIRING_PRIVATE_H_ */
