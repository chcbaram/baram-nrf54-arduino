/*
 * WVariant.h — variant 가 코어에 제공하는 것
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * Adafruit_nRF52_Arduino cores/nRF5/WVariant.h 와 같은 역할이며
 * nRF54L 의 3포트 구조에 맞춰 다시 썼다.
 */
#ifndef _WVARIANT_H_
#define _WVARIANT_H_

#include <stdint.h>
#include <stdbool.h>

#include "nrf.h"
#include "hal/nrf_gpio.h"

/* GPIO 전원 도메인 규칙과 variant 검증 매크로 */
#include "nrf54l_domains.h"

#ifdef SOFTDEVICE_PRESENT
  #include "nrf_sdm.h"
  #include "nrf_soc.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Arduino 핀 번호 → 절대 GPIO 번호 매핑.
 *
 * 절대 번호는 nrf_gpio.h 의 규약(port * 32 + pin)을 따른다:
 *   P0.00 ~ P0.31 →   0 ~  31
 *   P1.00 ~ P1.31 →  32 ~  63
 *   P2.00 ~ P2.31 →  64 ~  95
 *
 * ⚠ nRF52 는 포트가 2개(P0/P1)였지만 nRF54L15 는 3개다(P0/P1/P2).
 *   Adafruit 의 2포트 가정 코드를 그대로 옮기면 P2 가 깨진다.
 *
 * 쓸 수 없는 핀은 NRF54L_PIN_NC 로 채운다. 그러면 pinMode/digitalWrite/
 * digitalRead 가 조용히 무시한다. 핀 번호를 밀지 않고 구멍을 내기 위한
 * 방식이며 baram-stm32-arduino 의 variant 가 쓰는 것과 같은 요령이다.
 */
#define NRF54L_PIN_NC   (0xFFFFFFFFu)

extern const uint32_t g_ADigitalPinMap[];

/** 아날로그 핀 번호(A0..) → g_ADigitalPinMap 인덱스. 없으면 0xFF. */
extern const uint8_t g_APinDescription_analog[];

/** variant 가 부팅 시 한 번 불린다. weak 이므로 variant 에서 재정의한다. */
void initVariant(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _WVARIANT_H_ */
