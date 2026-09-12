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

/*═══════════════════════════════════════════════════════════════════════
 * analogRead (SAADC)
 *═══════════════════════════════════════════════════════════════════════
 *
 * ⚠ **nRF52 와 전압 구성이 다르다.** 내부 기준전압이 **900 mV** 다
 *   (nRF52 는 600 mV). 게인도 1/6 이 없고 1/4 부터 시작한다.
 *   그래서 Adafruit 의 `AR_INTERNAL_3_0` 같은 이름이 **같은 전압을 주지 못한다.**
 *   아래 별칭은 가장 가까운 값으로 이어 두었고, 실제 값을 주석에 적었다.
 *   정확한 전압이 필요하면 `analogReadMillivolts()` 를 써라 — 어떤 기준을
 *   골랐든 올바른 mV 를 돌려준다.
 *
 * ⚠ **AIN 이 있는 핀에만 된다.** SAADC 는 도메인 20(P1) 소속이고,
 *   nRF54L15/L05 에서 AIN0~7 은 **P1.04~P1.07 / P1.11~P1.14** 로 고정이다.
 *   variant 의 `A0`~`A7` 이 그 핀들이다. 다른 핀을 주면 0 을 돌려준다.
 */

/** 기준전압 + 게인 조합. 이름의 숫자가 **풀스케일 전압**이다. */
typedef enum
{
  AR_INTERNAL_3_6 = 0,   /**< 게인 1/4 — 기본값 */
  AR_INTERNAL_3_15,      /**< 게인 2/7 */
  AR_INTERNAL_2_7,       /**< 게인 1/3 */
  AR_INTERNAL_2_25,      /**< 게인 2/5 */
  AR_INTERNAL_1_8,       /**< 게인 1/2 */
  AR_INTERNAL_1_35,      /**< 게인 2/3 */
  AR_INTERNAL_0_9,       /**< 게인 1 */
  AR_INTERNAL_0_45,      /**< 게인 2 */
} eAnalogReference;

/*
 * Adafruit nRF52 호환 별칭 (CLAUDE.md R12).
 * ⚠ **표시된 것과 실제 전압이 다른 항목이 있다.** nRF54L 에 그 조합이 없다.
 */
#define AR_DEFAULT        AR_INTERNAL_3_6    /* 3.6 V — 같다 */
#define AR_INTERNAL       AR_INTERNAL_3_6    /* 3.6 V — 같다 */
#define AR_INTERNAL_3_0   AR_INTERNAL_3_15   /* ⚠ 실제 3.15 V */
#define AR_INTERNAL_2_4   AR_INTERNAL_2_25   /* ⚠ 실제 2.25 V */
#define AR_INTERNAL_1_2   AR_INTERNAL_1_35   /* ⚠ 실제 1.35 V */
#define AR_VDD4           AR_INTERNAL_3_6    /* ⚠ nRF54L 에 VDD/4 기준이 없다 */

/** 0 ~ (2^해상도 - 1). 읽을 수 없는 핀이면 0. */
int analogRead(uint32_t pin);

/** 실패를 알려주는 판. AIN 이 없는 핀이면 false. */
bool analogReadOk(uint32_t pin, int *out_value);

/** 핀 전압을 mV 로. 기준전압 설정을 알아서 반영한다. */
int analogReadMillivolts(uint32_t pin);

/** 8 / 10 / 12 / 14 비트. 기본 10. 그 밖의 값은 무시된다. */
void analogReadResolution(uint8_t bits);

/** 기준전압 + 게인 조합을 고른다. */
void analogReference(eAnalogReference ref);

/** 지금 설정의 풀스케일 전압(mV). `analogReference()` 가 정확한지 확인할 때 쓴다. */
uint32_t analogReferenceMillivolts(void);

#ifdef __cplusplus
}
#endif

#endif /* _WIRING_ANALOG_H_ */
