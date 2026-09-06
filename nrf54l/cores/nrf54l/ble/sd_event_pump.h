/*
 * SoftDevice 활성화 + 이벤트 펌프 — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * SoftDevice 를 켜고 끄는 것, 인터럽트 우선순위, sd_ble_evt_get() 루프를
 * **여기 한 곳에 모은다** (CLAUDE.md §10 M3).
 * 백엔드를 갈아치우면 통째로 버려질 코드이므로 흩뿌리지 않는다.
 * 추상화 레이어가 아니라 코드 배치 규칙이다 (R11 위반이 아니다).
 */
#ifndef _SD_EVENT_PUMP_H_
#define _SD_EVENT_PUMP_H_

#include <stdbool.h>
#include <stdint.h>

#include "ble.h"

#ifdef __cplusplus
extern "C" {
#endif

/** BLE 이벤트 관찰자. 이벤트 태스크 컨텍스트에서 불린다 (ISR 아님). */
typedef void (*sd_ble_observer_t)(const ble_evt_t *evt, void *ctx);

/**
 * SoftDevice 를 켜고 BLE 스택을 활성화한다.
 *
 * 순서가 중요하다 (아래 구현 주석 참조). 이미 켜져 있으면 아무것도 하지 않고
 * true 를 돌려준다.
 *
 * @return 성공하면 true. 실패 원인은 sdLastError() 로 확인한다.
 */
bool sdEnable(void);

/** SoftDevice 가 켜져 있는가. */
bool sdIsEnabled(void);

/**
 * BLE 이벤트 관찰자를 등록한다. 등록 순서대로 호출된다.
 * @return 자리가 없으면 false.
 */
bool sdBleObserverAdd(sd_ble_observer_t handler, void *ctx);

/** 마지막으로 실패한 SoftDevice API 의 오류 코드 (NRF_ERROR_*). 0 이면 없음. */
uint32_t sdLastError(void);

/** SoftDevice 가 실제로 쓴 RAM 크기 (바이트). sd_ble_enable() 이후에 유효하다. */
uint32_t sdRamUsed(void);

#ifdef __cplusplus
}
#endif

#endif /* _SD_EVENT_PUMP_H_ */
