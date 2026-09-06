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

/*
 * 동시에 맺을 수 있는 peripheral 연결 수.
 *
 * ⚠ 이건 **RAM 예약과 한 몸이다.** 링크 하나가 SoftDevice RAM 을 MTU 247 기준
 *   약 3980 B 더 먹는데, 그 RAM 은 링커 스크립트가 정적으로 떼어 준다.
 *   여기만 올리고 링커를 안 고치면 sd_ble_enable() 이 NRF_ERROR_NO_MEM 을 낸다.
 *   그래서 값은 링커 스크립트를 고르는 곳 — boards.txt 의 build.extra_flags —
 *   에서 보드별로 준다. 측정표는 docs/MEMORY-MAP.md.
 *
 * 헤더에 두는 이유: Bluefruit54Lib 이 연결 배열 크기(BLE_MAX_CONNECTION)를
 * 여기에 맞춰야 한다. 코어와 라이브러리가 서로 다른 값을 보면
 * SoftDevice 는 연결을 맺는데 라이브러리가 관리하지 못한다.
 */
#ifndef SD_BLE_PERIPH_LINK_COUNT
#define SD_BLE_PERIPH_LINK_COUNT      (1)
#endif

/*
 * central 역할 동시 연결 수. 0 이면 central 을 켜지 않는다.
 * peripheral 과 마찬가지로 boards.txt 의 build.extra_flags 에서 준다.
 */
#ifndef SD_BLE_CENTRAL_LINK_COUNT
#define SD_BLE_CENTRAL_LINK_COUNT     (0)
#endif

/**
 * RRAM 에 쓴다 (SoftDevice 경유). 완료될 때까지 기다린다.
 *
 * ⚠ SoftDevice 가 켜져 있으면 **직접 RRAM 을 쓰면 안 된다.** 라디오 타이밍과
 *   겹치기 때문이다. sd_flash_write() 가 시점을 잡아 주고, 완료는 SoC 이벤트로
 *   온다 — 그 이벤트를 여기서 기다린다.
 *
 * ⚠ **BLE 이벤트 태스크에서 부르면 안 된다.** 기다리는 완료 이벤트를 퍼 올리는
 *   주체가 자기 자신이라 타임아웃까지 멈춘다.
 *
 * @param dst   RRAM 주소. 4바이트 정렬이어야 한다
 * @param src   원본. 4바이트 정렬이어야 한다
 * @param words 쓸 32비트 워드 수
 * @return 성공하면 true.
 */
bool sdFlashWrite(uint32_t *dst, const uint32_t *src, uint32_t words, uint32_t timeout_ms);

/** BLE 이벤트 관찰자. 이벤트 태스크 컨텍스트에서 불린다 (ISR 아님). */
typedef void (*sd_ble_observer_t)(const ble_evt_t *evt, void *ctx);

/**
 * BLE 스택 구성. sdEnable() 에 넘긴다.
 *
 * ⚠ 이 값들은 **sd_ble_enable() 전에만** 정할 수 있고, 그 뒤로는 못 바꾼다.
 *   그래서 Bluefruit.begin() 이 부르기 전에 확정돼야 한다.
 *
 * ⚠ RAM 은 링커가 정적으로 떼어 준다. 여기 값을 키워 RAM 이 모자라면
 *   sdEnable() 이 **실패한다** — 조용히 깎지 않는다. 얼마가 필요한지는
 *   sdCfgResults() 의 ram_required 로 알 수 있다.
 */
typedef struct {
    uint8_t  prph_count;         /**< peripheral 동시 연결 수 */
    uint8_t  central_count;      /**< central 동시 연결 수 */
    uint16_t att_mtu;            /**< ATT MTU. SD_BLE_ATT_MTU 를 넘을 수 없다 */
    uint16_t event_length;       /**< 연결 이벤트 길이 (1.25 ms 단위) */
    uint8_t  hvn_tx_queue_size;  /**< notify 송신 큐 깊이 (연결당) */
} sd_ble_conf_t;

/** 보드가 정한 컴파일 타임 기본값으로 채운다 (boards.txt 의 -D 들). */
void sdConfigDefault(sd_ble_conf_t *conf);

/**
 * SoftDevice 를 켜고 BLE 스택을 활성화한다.
 *
 * 순서가 중요하다 (구현 주석 참조). 이미 켜져 있으면 아무것도 하지 않고
 * true 를 돌려준다 — **두 번째 호출의 conf 는 무시된다.**
 *
 * @param conf 구성. NULL 이면 sdConfigDefault() 값을 쓴다.
 * @return 성공하면 true. 실패 원인은 sdLastError() 로 확인한다.
 */
bool sdEnable(const sd_ble_conf_t *conf);

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

/**
 * 이 스택이 **실제로 구성된** ATT MTU. MTU 교환 응답에 반드시 이 값을 써야 한다.
 * sdEnable() 전에는 기본값을 돌려준다.
 */
uint16_t sdAttMtu(void);

/**
 * 연결 구성 태그. `sd_ble_gap_adv_start()` 에 이 값을 넘겨야 우리가 설정한
 * MTU / 연결 구성이 실제로 쓰인다.
 */
uint8_t sdConnCfgTag(void);

/**
 * sd_ble_cfg_set() 세 건의 반환값과 SoftDevice 가 요구한 최소 RAM 시작 주소.
 * 0 이 성공. MTU 를 키웠는데 협상이 안 되면 여기부터 본다.
 */
void sdCfgResults(uint32_t *role, uint32_t *gap, uint32_t *gatt, uint32_t *ram_required);

/** BLE_CONN_CFG_GATTS(notify 큐 깊이) 설정의 반환값. 0 이 성공. */
uint32_t sdCfgGattsResult(void);

/**
 * central 로 연결할 때(`sd_ble_gap_connect`) 넘길 연결 구성 태그.
 *
 * ⚠ S145 는 연결 구성을 **하나만** 허용하므로 peripheral 태그와 같다.
 *   역할마다 다른 MTU 를 줄 수 없다 (sd_event_pump.c 주석 참조).
 */
uint8_t sdCentralConnCfgTag(void);

#ifdef __cplusplus
}
#endif

#endif /* _SD_EVENT_PUMP_H_ */
