/*
 * nrfx 설정 — baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * nrfx는 Zephyr 없이 standalone으로 쓴다 (CLAUDE.md R1).
 * 여기서 켜는 드라이버만 컴파일된다.
 */
#ifndef NRFX_CONFIG_H__
#define NRFX_CONFIG_H__

/*
 * 여기서 먼저 값을 정의하면 SoC 템플릿의 #ifndef 기본값을 덮어쓴다.
 * 템플릿 include는 파일 맨 끝에서 한다.
 */

/* ── 기본 IRQ 우선순위 ───────────────────────────────────────────── */
#define NRFX_DEFAULT_IRQ_PRIORITY 6

/*
 * ── 인터럽트 우선순위 (CLAUDE.md §7 F2) ───────────────────────────────
 * __NVIC_PRIO_BITS = 3 (nrf54l15_application.h:193) → 0~7.
 * SoftDevice가 0(RADIO_0/TIMER10/GRTC_3, zero-latency)과
 * 4(CLOCK_POWER/ECB00/AAR00_CCM00/SWI00/SVCall)를 점유한다.
 * FreeRTOS configMAX_SYSCALL_INTERRUPT_PRIORITY = 5 이므로,
 * ...FromISR을 부르는 드라이버 ISR은 반드시 5~7이어야 한다.
 * 기본값 6은 그 범위 안이다.
 */

/* ── GRTC: FreeRTOS 틱 (CLAUDE.md §7 F3) ─────────────────────────────
 * SoftDevice가 CC7~11과 GRTC_3_IRQn을 쓴다 (nrf_sd_def.h:
 *   SD_GRTC_CC_CHANNELS_USED 0x00000F80, SD_GRTC_IRQn_USED GRTC_3_IRQn).
 * 따라서 앱은 CC0~6만 쓴다. 마스크 0x0000007F.
 */
#define NRFX_GRTC_ENABLED                        1
#define NRFX_GRTC_CONFIG_ALLOWED_CC_CHANNELS_MASK 0x0000007FUL
/*
 * ⚠ 위 마스크의 비트 수와 반드시 일치해야 한다.
 *   nrfx_grtc_init() 이 popcount(MASK) != NUM_OF_CC_CHANNELS 이면
 *   -ECANCELED 로 실패한다 (nrfx_grtc.c:364).
 *   0x7F = CC0~6 = 7개.
 *
 *   nrfx 템플릿 기본값은 마스크 0x0f0f / 개수 8 인데, 그 마스크는
 *   CC8~11 을 포함해 SoftDevice 영역(CC7~11)과 겹친다. 쓰면 안 된다.
 */
#define NRFX_GRTC_CONFIG_NUM_OF_CC_CHANNELS      7
#define NRFX_GRTC_CONFIG_AUTOEN                  0
#define NRFX_GRTC_CONFIG_IRQ_PRIORITY            NRFX_DEFAULT_IRQ_PRIORITY

/* ── M1 범위: GPIO는 HAL 직접 사용, UARTE만 드라이버 ────────────────── */
#define NRFX_UARTE_ENABLED                       1
#define NRFX_UARTE30_ENABLED                     1
#define NRFX_UARTE20_ENABLED                     1
#define NRFX_UARTE21_ENABLED                     1
#define NRFX_UARTE22_ENABLED                     1
/* nRF54LM20A 는 UARTE23/24 가 더 있다. SoC 별로 조건부. */
#if defined(NRF54LM20A_XXAA)
  #define NRFX_UARTE23_ENABLED                   1
  #define NRFX_UARTE24_ENABLED                   1
#endif
#define NRFX_UARTE_DEFAULT_CONFIG_IRQ_PRIORITY   NRFX_DEFAULT_IRQ_PRIORITY

/* ── M2에서 켠다 ──────────────────────────────────────────────────────
 * NRFX_SPIM_ENABLED / NRFX_SPIM00_ENABLED   (SPI, P2 고속 도메인)
 * NRFX_TWIM_ENABLED / NRFX_TWIM20_ENABLED   (Wire)
 * NRFX_PWM_ENABLED  / NRFX_PWM20_ENABLED    (analogWrite)
 * NRFX_SAADC_ENABLED                        (analogRead)
 * NRFX_GPIOTE_ENABLED / 20 / 30             (attachInterrupt)
 */

/* ── 나머지 기본값은 SoC 템플릿에서 (#ifndef 이므로 위 설정이 우선) ── */
#include <templates/nrfx_config_common.h>
#include <templates/nrfx_config_nrf54l15_application.h>

#endif /* NRFX_CONFIG_H__ */
