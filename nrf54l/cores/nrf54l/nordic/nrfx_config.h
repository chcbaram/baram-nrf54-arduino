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
#define NRFX_GRTC_CONFIG_AUTOEN                  1
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

/* ── I2C (Wire) ───────────────────────────────────────────────────────
 * ⚠ variant 가 고르는 인스턴스를 **여기서 미리 다 켜 둔다.** 보드마다 다른데
 *   (`WIRE_TWIM_INSTANCE`), nrfx 는 컴파일 시점에 인스턴스별로 코드를 넣는다.
 *   켜지 않은 인스턴스를 variant 가 고르면 링크에서 드라이버가 없다고 나온다.
 *
 * ⚠ TWIM00 은 존재하지 않는다 = P2 에 I2C 불가 (docs/PERIPHERAL-PINMAP.md §0).
 *   TWIM20/21/22 는 P1, TWIM30 은 P0 다.
 */
#define NRFX_TWIM_ENABLED                        1
#define NRFX_TWIM20_ENABLED                      1
#define NRFX_TWIM21_ENABLED                      1
#define NRFX_TWIM22_ENABLED                      1
#define NRFX_TWIM30_ENABLED                      1
#define NRFX_TWIM_DEFAULT_CONFIG_IRQ_PRIORITY    NRFX_DEFAULT_IRQ_PRIORITY

/* ── SPI ──────────────────────────────────────────────────────────────
 * SPIM00 은 **P2 전용 고속 도메인**이다 (nrf54l_domains.h).
 * P1 에 SPI 가 필요한 보드를 위해 SPIM20~22 도 함께 켠다 — Wire 와 같은 이유로
 * variant 마다 고르는 번호가 다르다. 단 같은 번호의 UARTE/TWIM 과는
 * **같은 하드웨어 블록**이라 동시에 못 쓴다 (docs/PERIPHERAL-PINMAP.md §0).
 */
#define NRFX_SPIM_ENABLED                        1
#define NRFX_SPIM00_ENABLED                      1
#define NRFX_SPIM20_ENABLED                      1
#define NRFX_SPIM21_ENABLED                      1
#define NRFX_SPIM22_ENABLED                      1
#define NRFX_SPIM_DEFAULT_CONFIG_IRQ_PRIORITY    NRFX_DEFAULT_IRQ_PRIORITY

/*
 * GPIOTE — attachInterrupt (cores/nrf54l/wiring_interrupt.c)
 *
 * ⚠ 인스턴스별 `NRFX_GPIOTE20_ENABLED` 같은 매크로는 **없다.** 다른 드라이버와
 *   달리 인스턴스 표가 SoC 헤더의 `NRFX_FOREACH_INDEXED_PRESENT` 로 자동 생성돼
 *   존재하는 GPIOTE 가 전부 들어간다. 켤 것은 이 하나뿐이다.
 *
 * 우선순위는 FreeRTOS 규칙상 5~7 이어야 한다 (§7 F2) — 콜백에서 `...FromISR`
 * 을 부를 수 있어야 하기 때문이다. `NRFX_DEFAULT_IRQ_PRIORITY` = 6.
 */
#define NRFX_GPIOTE_ENABLED                      1
#define NRFX_GPIOTE_DEFAULT_CONFIG_IRQ_PRIORITY  NRFX_DEFAULT_IRQ_PRIORITY

/*
 * PWM — analogWrite (cores/nrf54l/wiring_analog.c)
 *
 * 셋 다 켠다. 인스턴스마다 4채널이라 합쳐 12핀이고, 전부 도메인 20 이므로
 * **P1 핀만** 몰 수 있다 (docs/PERIPHERAL-PINMAP.md §4).
 * 우선순위는 §7 F2 대로 5~7 이어야 한다 — NRFX_DEFAULT_IRQ_PRIORITY = 6.
 */
#define NRFX_PWM_ENABLED                         1
#define NRFX_PWM20_ENABLED                       1
#define NRFX_PWM21_ENABLED                       1
#define NRFX_PWM22_ENABLED                       1
#define NRFX_PWM_DEFAULT_CONFIG_IRQ_PRIORITY     NRFX_DEFAULT_IRQ_PRIORITY

/* ── M2 에서 아직 안 켠 것 ────────────────────────────────────────────
 * NRFX_SAADC_ENABLED                        (analogRead)
 */

/* ── 나머지 기본값은 SoC 템플릿에서 (#ifndef 이므로 위 설정이 우선) ── */
#include <templates/nrfx_config_common.h>
#include <templates/nrfx_config_nrf54l15_application.h>

#endif /* NRFX_CONFIG_H__ */
