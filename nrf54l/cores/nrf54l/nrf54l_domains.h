/*
 * nrf54l_domains.h — GPIO 전원 도메인과 페리페럴 배정 규칙
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * ── 왜 이 파일이 있나 ─────────────────────────────────────────────────
 *
 * nRF52 는 PSEL 로 아무 GPIO나 아무 페리페럴에 붙일 수 있었다.
 * **nRF54L 은 안 된다.** 페리페럴 인스턴스마다 속한 전원 도메인이 있고,
 * 그 도메인이 소유한 GPIO 포트의 핀만 선택할 수 있다.
 *
 * 규칙은 단순하다 — **인스턴스 번호의 첫 자리가 도메인이고,
 * 도메인마다 GPIO 포트를 하나씩 소유한다.**
 *
 *   인스턴스 x00  →  P2   (고속 도메인)
 *   인스턴스 x20  →  P1   (메인 도메인)
 *   인스턴스 x30  →  P0   (상시 전원 도메인)
 *
 * 근거: MDK 의 페리페럴 베이스 주소가 도메인별로 뭉쳐 있다.
 *   0x5004-0x5005 : SPIM00 SPIS00 UARTE00 TIMER00 **P2**
 *   0x500C-0x500E : SPIM20-22 TWIM20-22 UARTE20-22 PWM20-22 SAADC
 *                   PDM20/21 I2S20 QDEC20/21 GPIOTE20 NFCT **P1**
 *   0x5010        : SPIM30 TWIM30 UARTE30 GPIOTE30 COMP LPCOMP WDT30/31 **P0**
 *
 * 회로도가 이를 세 번 확인해 준다: SAADC 가 P1 도메인이라 AIN0~7 이 전부 P1.xx,
 * NFCT 가 P1 도메인이라 NFC1/2 가 P1.02/03, UARTE30 이 P0 도메인이라
 * 콘솔 UART 가 P0.00~03 이다.
 *
 * 상세 매트릭스는 docs/PERIPHERAL-PINMAP.md.
 *
 * ── 쓰는 법 ───────────────────────────────────────────────────────────
 * variant.h 끝에서 static_assert 로 검증한다. 잘못 배정하면 **빌드가 실패**한다.
 * 런타임에 조용히 동작하지 않거나 BusFault 로 죽는 것보다 낫다.
 */
#ifndef _NRF54L_DOMAINS_H_
#define _NRF54L_DOMAINS_H_

/** 절대 GPIO 번호(port*32 + pin) 에서 포트 번호를 뽑는다. */
#define NRF54L_PORT_OF(abs_pin)   ((abs_pin) >> 5)
#define NRF54L_PIN_OF(abs_pin)    ((abs_pin) & 0x1F)

/** 각 도메인이 소유한 GPIO 포트 */
#define NRF54L_DOMAIN00_PORT   2   /* SPIM00 SPIS00 UARTE00 TIMER00 */
#define NRF54L_DOMAIN20_PORT   1   /* SPIM/TWIM/UARTE/PWM 20~22, SAADC, GPIOTE20, NFCT */
#define NRF54L_DOMAIN30_PORT   0   /* SPIM30 TWIM30 UARTE30 GPIOTE30 COMP LPCOMP */

/*
 * variant 검증용 매크로.
 *
 * C 와 C++ 양쪽에서 쓰이므로 _Static_assert / static_assert 를 가려 쓴다.
 * variant.h 는 C 에서도 include 된다.
 */
#if defined(__cplusplus)
  #define NRF54L_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#else
  #define NRF54L_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#endif

/**
 * 핀이 지정한 포트에 속하는지 컴파일 타임에 검사한다.
 *
 * @param pin      variant 의 핀 매크로 (절대 GPIO 번호여야 한다)
 * @param port     기대 포트 (0/1/2)
 * @param what     오류 메시지에 넣을 설명
 */
#define NRF54L_ASSERT_PIN_PORT(pin, port, what) \
    NRF54L_STATIC_ASSERT(NRF54L_PORT_OF(pin) == (port), what)

/* 자주 쓰는 조합 */
#define NRF54L_ASSERT_SPIM00_PIN(pin, what) \
    NRF54L_ASSERT_PIN_PORT(pin, NRF54L_DOMAIN00_PORT, what " : SPIM00/UARTE00 은 P2 도메인만 쓸 수 있다")
#define NRF54L_ASSERT_DOMAIN20_PIN(pin, what) \
    NRF54L_ASSERT_PIN_PORT(pin, NRF54L_DOMAIN20_PORT, what " : x20 계열 인스턴스는 P1 도메인만 쓸 수 있다")
#define NRF54L_ASSERT_DOMAIN30_PIN(pin, what) \
    NRF54L_ASSERT_PIN_PORT(pin, NRF54L_DOMAIN30_PORT, what " : x30 계열 인스턴스는 P0 도메인만 쓸 수 있다")

/**
 * PERI(x20) 도메인의 **UARTE / SPIS 에 한한 예외**. P1 뿐 아니라 P2 도 허용한다.
 *
 * Nordic 핀 계획 가이드 원문:
 *   "Selected pins on P2 can also be used by certain serial interfaces
 *    (SPIS, UARTE) located in PERI, although this configuration is less
 *    power-efficient."
 *
 * ⚠ 두 가지를 알고 써라.
 *   1. **전력이 불리하다.** 기본 규칙(P1)으로 되는 배정이면 그쪽을 써라
 *   2. P2 의 **어느 핀이** 되는지는 아직 확인하지 못했다
 *      (docs/PERIPHERAL-PINMAP.md §4). 그래서 이 매크로는 포트만 보고
 *      핀 번호까지 검사하지 못한다
 *
 * TWIM/PWM/SAADC 등에는 쓰지 마라. 예외 대상이 아니다.
 * 상세는 docs/PERIPHERAL-PINMAP.md.
 */
#define NRF54L_ASSERT_PERI_SERIAL_PIN(pin, what)                              \
    NRF54L_STATIC_ASSERT(NRF54L_PORT_OF(pin) == NRF54L_DOMAIN20_PORT ||       \
                         NRF54L_PORT_OF(pin) == NRF54L_DOMAIN00_PORT,         \
                         what " : PERI 의 UARTE/SPIS 는 P1 또는 P2 만 쓸 수 있다")

/** SAADC 는 P1 도메인이다. AIN 핀은 반드시 P1. */
#define NRF54L_ASSERT_ANALOG_PIN(pin, what) \
    NRF54L_ASSERT_PIN_PORT(pin, NRF54L_DOMAIN20_PORT, what " : SAADC 는 P1 도메인만 쓸 수 있다")

#endif /* _NRF54L_DOMAINS_H_ */
