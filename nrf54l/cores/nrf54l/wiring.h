/*
 * wiring.h — 부팅 초기화와 저전력 API
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 */
#ifndef _WIRING_H_
#define _WIRING_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** main() 이 setup() 전에 부른다. 클럭·리셋원인 등을 잡는다. */
void init(void);

/** 리셋 원인. nRF52 의 NRF_POWER->RESETREAS 가 아니라 NRF_RESETINFO 다. */
uint32_t readResetReason(void);

/**
 * 이벤트가 올 때까지 CPU 를 재운다 (System ON sleep).
 *
 * ⚠ Adafruit 은 여기서 sd_app_evt_wait() 를 불렀지만 S145 에는 그 API 가
 *   없다. SoftDevice 가 NVIC 를 가상화하지 않으므로 앱이 직접 잔다.
 *   Nordic 자신의 ble_pwr_profiling 샘플도 k_cpu_idle()(= __WFI) 만 쓴다.
 *   CLAUDE.md §6.1 참조.
 */
void waitForEvent(void);

/**
 * System OFF 진입. 지정한 핀의 레벨 변화로만 깨어나며, 기상은 리셋이다.
 *
 * @param pin        기상 핀 (Arduino 핀 번호)
 * @param wake_logic HIGH 면 상승, LOW 면 하강에서 깨어난다
 *
 * ⚠ SWD 프로브가 붙어 있으면 System OFF 가 시뮬레이션 모드로 동작해
 *   실제 저전력에 들어가지 않는다. 전류를 재려면 프로브를 분리하라
 *   (CLAUDE.md §7 F8).
 */
void systemOff(uint32_t pin, uint8_t wake_logic);

/** SoC 내부 온도(℃). */
float readCPUTemperature(void);

#ifdef __cplusplus
}
#endif

#endif /* _WIRING_H_ */
