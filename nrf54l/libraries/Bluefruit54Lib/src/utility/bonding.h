/*
 * utility/bonding.h — 상류 include 경로 호환 껍데기
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * Adafruit 예제가 `#include "utility/bonding.h"` 로 집어 온다. 구현은
 * `src/bonding.h` 에 있고 여기서는 이름만 상류식으로 맞춰 준다 (R12).
 */
#ifndef _UTILITY_BONDING_H_
#define _UTILITY_BONDING_H_

#include "../bonding.h"

static inline void bond_print_list(uint8_t role) { bondPrintList(role); }
static inline void bond_clear_prph(void)         { bondClear(BLE_GAP_ROLE_PERIPH); }
static inline void bond_clear_cntr(void)         { bondClear(BLE_GAP_ROLE_CENTRAL); }
static inline void bond_clear_all(void)          { bondClearAll(); }

#endif
