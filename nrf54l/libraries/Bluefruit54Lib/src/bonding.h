/*
 * bonding — 본딩 키를 RRAM 에 남긴다
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * Adafruit 은 이것을 파일시스템에 파일 하나씩 둔다
 * (`/adafruit/bond_prph/<MAC>`). 우리는 파일시스템이 없고 대신 링커가 떼어 준
 * 4 KB RRAM 파티션(`__bond_storage_start__`)을 **고정 슬롯**으로 쓴다.
 *
 * ⚠ RRAM 은 erase 가 없고 **제자리에서 덮어쓸 수 있다** — 0 -> 1 전이도 된다
 *   (실기 확인). NOR 플래시식 append-log 가 필요 없다.
 *
 * ⚠ 파티션은 앱 파티션 **밖**이라 앱을 다시 구워도 본딩이 남는다.
 *   지우려면 bondClearAll() 을 부르거나 칩을 통째로 지워야 한다.
 */
#ifndef _BONDING_H_
#define _BONDING_H_

#include <Arduino.h>
#include <ble.h>
#include <ble_gap.h>

/** 상대와 나눈 키. Adafruit 의 bond_keys_t 와 같은 구성이다 (약 80 바이트). */
typedef struct {
  ble_gap_enc_key_t own_enc;    /**< 우리 LTK */
  ble_gap_enc_key_t peer_enc;   /**< 상대 LTK */
  ble_gap_id_key_t  peer_id;    /**< IRK + identity 주소 */
} bond_keys_t;

/** 슬롯 하나의 크기. 4 KB / 256 = 16 개를 남긴다. */
#define BOND_SLOT_SIZE      (256)
#define BOND_MAX_COUNT      (16)

/**
 * 슬롯에 담는 시스템 속성(CCCD) 최대 길이.
 * 우리 GATT DB 는 CCCD 가 몇 개뿐이라 넉넉하다. 넘치면 저장하지 않는다.
 */
#define BOND_SYS_ATTR_MAX   (160)

/** 본딩 저장소를 준비한다. Bluefruit.begin() 이 부른다. */
void bondInit(void);

/**
 * 키를 저장한다. 같은 상대가 이미 있으면 그 슬롯을 덮어쓴다.
 *
 * ⚠ **BLE 이벤트 태스크에서 부르면 안 된다** — RRAM 쓰기 완료를 기다린다.
 */
bool bondSaveKeys(uint8_t role, const bond_keys_t *keys);

/**
 * 상대 주소로 키를 찾는다.
 *
 * ⚠ 주소가 **resolvable private** 이면 주소로는 못 찾는다. 폰이 프라이버시
 *   때문에 주소를 주기적으로 바꾸기 때문이다. 그때는 저장된 IRK 로 주소를
 *   하나씩 풀어 맞춰 본다 (Adafruit 도 같은 2단계다).
 */
bool bondLoadKeys(uint8_t role, const ble_gap_addr_t *peer_addr, bond_keys_t *keys);

/** 연결의 시스템 속성(CCCD 등)을 그 상대 슬롯에 저장한다. */
bool bondSaveCccd(uint8_t role, uint16_t conn_hdl, const ble_gap_addr_t *peer_addr);

/** 저장된 시스템 속성을 연결에 복원한다. 없으면 false. */
bool bondLoadCccd(uint8_t role, uint16_t conn_hdl, const ble_gap_addr_t *peer_addr);

/** 그 역할의 본딩을 모두 지운다. */
void bondClear(uint8_t role);
void bondClearAll(void);

/** 저장된 본딩 수. role 이 BLE_GAP_ROLE_INVALID 면 전체. */
uint8_t bondCount(uint8_t role);

/** 저장된 본딩 목록을 Serial 로 찍는다 (진단용). */
void bondPrintList(uint8_t role);

/**
 * resolvable private 주소가 이 IRK 로 만들어진 것인지 확인한다.
 *
 * ⚠ 바이트 순서가 함정이다. `ble_gap_addr_t.addr` 와 IRK 는 리틀엔디안인데
 *   AES 블록은 빅엔디안이라 키·입력·출력을 모두 뒤집어야 한다.
 */
bool bondResolveAddress(const ble_gap_addr_t *addr, const ble_gap_irk_t *irk);

#endif
