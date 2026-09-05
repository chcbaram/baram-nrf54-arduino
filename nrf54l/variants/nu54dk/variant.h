/*
 * variant.h — NU54-DK (nRF54L15)
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * 회로도 분석 결과는 docs/NU54-DK.md 에 있다. 값을 바꾸기 전에 그걸 먼저 읽어라.
 *
 * 핀 번호 규약: Arduino 핀 번호 = 절대 GPIO 번호 (port * 32 + pin).
 *   P0.00~P0.04 →  0 ~  4
 *   P1.00~P1.14 → 32 ~ 46
 *   P2.00~P2.10 → 64 ~ 74
 * Adafruit nRF52 코어와 같은 방식이라 스케치의 _PINNUM(port, pin) 이 그대로 통한다.
 * 중간의 빈 번호는 g_ADigitalPinMap 에서 NC 로 채워져 무시된다.
 */
#ifndef _VARIANT_NU54DK_H_
#define _VARIANT_NU54DK_H_

#include <stdint.h>

/* 아래 도메인 검증 매크로(NRF54L_ASSERT_*)를 제공한다.
 * variant.h 는 Arduino.h 를 거치지 않고 직접 include 되기도 하므로
 * 여기서 스스로 챙긴다. */
#include "nrf54l_domains.h"

#define _PINNUM(port, pin)    ( ( (port) * 32 ) + (pin) )

/* ── 클럭 ─────────────────────────────────────────────────────────────
 * Y1 32.768 kHz 크리스털이 P1.00(XL1) / P1.01(XL2) 에 실장돼 있다.
 * 크리스털이 없는 보드로 파생시킬 때만 USE_LFRC 로 바꾼다. */
#define USE_LFXO
#define VARIANT_MCK           (128000000ul)

/* ── 핀 개수 ─────────────────────────────────────────────────────────
 * P2.10 = 74 가 마지막이다. */
#define PINS_COUNT            (75u)
#define NUM_DIGITAL_PINS      (75u)
#define NUM_ANALOG_INPUTS     (8u)
#define NUM_ANALOG_OUTPUTS    (0u)

/* ── LED ──────────────────────────────────────────────────────────────
 * ⚠ active HIGH 다. N-MOSFET 게이트를 직접 구동한다.
 *   Adafruit nRF52 보드 대부분은 active LOW 이므로 반대다.
 *   ledOn()/ledOff() 를 쓰면 이 차이를 신경 쓰지 않아도 된다.
 *
 * ⚠ LED_3(P2.07)은 SWD 커넥터 J3 의 6번핀(SWO)과 공유한다.
 *   SWO 트레이스를 켜면 이 LED 가 같이 깜빡인다. */
#define LED_STATE_ON          1

#define PIN_LED1              _PINNUM(2,  9)   /* D7  */
#define PIN_LED2              _PINNUM(1, 10)   /* D8  */
#define PIN_LED3              _PINNUM(2,  7)   /* D9  — SWO 겸용 */
#define PIN_LED4              _PINNUM(1, 14)   /* D10 — AIN7 겸용 */

#define LED_BUILTIN           PIN_LED1
#define LED_CONN              PIN_LED2

/* Adafruit 스케치 호환용 별칭. 실제 색이 아니라 위치 이름이다. */
#define LED_RED               PIN_LED1
#define LED_BLUE              PIN_LED2

/* ── 버튼 ─────────────────────────────────────────────────────────────
 * ⚠ 외부 풀업이 없다. 회로도에 "USE INTERNAL PULLUP" 이라고 명기돼 있다.
 *   반드시 pinMode(PIN_BUTTONn, INPUT_PULLUP) 으로 쓸 것. 눌리면 LOW. */
#define PIN_BUTTON1           _PINNUM(1, 13)   /* SW2 — AIN6 겸용 */
#define PIN_BUTTON2           _PINNUM(1,  9)   /* SW3 */
#define PIN_BUTTON3           _PINNUM(1,  8)   /* SW4 */
#define PIN_BUTTON4           _PINNUM(0,  4)   /* SW5 */

/* ── UART ─────────────────────────────────────────────────────────────
 * Serial = UARTE30. CP2102N USB 브리지에 물려 있다.
 * Nordic nRF54L15 DK 의 BOARD_APP_UARTE_* 와 같은 배선이다. */
#define PIN_SERIAL_TX         _PINNUM(0, 0)
#define PIN_SERIAL_RX         _PINNUM(0, 1)
/*
 * CP2102N 의 RTS/CTS 가 P0.02 / P0.03 에 배선돼 있지만 **사용하지 않는다.**
 * 하드웨어 흐름제어를 켜면 호스트가 RTS 를 올리지 않을 때 TX 가 멈춘다
 * (Uart.h 주석 참조). 따라서 이 두 핀은 일반 GPIO 로 쓸 수 있다.
 */
#define PIN_SERIAL_CTS        _PINNUM(0, 2)   /* 미사용. CP2102N RTS 배선 */
#define PIN_SERIAL_RTS        _PINNUM(0, 3)   /* 미사용. CP2102N CTS 배선 */

/*
 * Serial 이 쓸 UARTE 인스턴스. 코어가 아니라 variant 가 고른다.
 * SoC 마다 인스턴스 구성이 다르다 (예: nRF54LM20A 는 UARTE23/24 도 있다).
 * 반드시 핀이 속한 도메인의 인스턴스여야 한다 (nrf54l_domains.h).
 */
#define SERIAL_UARTE_INSTANCE     NRF_UARTE30

#define SERIAL_PORT_MONITOR       Serial
#define SERIAL_PORT_HARDWARE      Serial
#define SERIAL_PORT_HARDWARE_OPEN Serial

/* ── 아날로그 (SAADC) ─────────────────────────────────────────────────
 * 회로도의 AIN 표기 그대로다.
 * ⚠ A6 는 버튼 SW2, A7 은 LED D10 과 핀을 공유한다. 동시에 못 쓴다. */
#define PIN_A0                _PINNUM(1,  4)   /* AIN0 */
#define PIN_A1                _PINNUM(1,  5)   /* AIN1 */
#define PIN_A2                _PINNUM(1,  6)   /* AIN2 */
#define PIN_A3                _PINNUM(1,  7)   /* AIN3 */
#define PIN_A4                _PINNUM(1, 11)   /* AIN4 */
#define PIN_A5                _PINNUM(1, 12)   /* AIN5 */
#define PIN_A6                _PINNUM(1, 13)   /* AIN6 — SW2 겸용 */
#define PIN_A7                _PINNUM(1, 14)   /* AIN7 — LED D10 겸용 */

static const uint8_t A0 = PIN_A0;
static const uint8_t A1 = PIN_A1;
static const uint8_t A2 = PIN_A2;
static const uint8_t A3 = PIN_A3;
static const uint8_t A4 = PIN_A4;
static const uint8_t A5 = PIN_A5;
static const uint8_t A6 = PIN_A6;
static const uint8_t A7 = PIN_A7;

#define ADC_RESOLUTION        12

/* ── NFC ──────────────────────────────────────────────────────────────
 * NFC 안테나 핀으로 쓸지 GPIO 로 쓸지는 UICR 설정이다. */
#define PIN_NFC1              _PINNUM(1, 2)
#define PIN_NFC2              _PINNUM(1, 3)

/* ── SPI — SPIM00 (M2) ────────────────────────────────────────────────
 * SPIM00 은 고속 도메인이라 **P2 핀만** 쓸 수 있다 (nrf54l_domains.h).
 * ⚠ 도메인은 맞지만 P2 안에서 어느 핀이 어느 신호로 갈 수 있는지는 미확정이다.
 *   M2 에서 Product Spec 의 GPIO 배치표로 확인하고 실기 검증할 것.
 *   고속 신호라 OUTPUT_H0H1 또는 OUTPUT_E0E1 드라이브가 필요할 수 있다
 *   (CLAUDE.md §4 SPI 주의사항). */
#define PIN_SPI_SCK           _PINNUM(2, 1)
#define PIN_SPI_MOSI          _PINNUM(2, 2)
#define PIN_SPI_MISO          _PINNUM(2, 4)
static const uint8_t SS   = _PINNUM(2, 5);
static const uint8_t SCK  = PIN_SPI_SCK;
static const uint8_t MOSI = PIN_SPI_MOSI;
static const uint8_t MISO = PIN_SPI_MISO;

/* ── I2C — TWIM30 (M2) ────────────────────────────────────────────────
 * P0.02 / P0.03 을 쓴다. TWIM30 은 P0 도메인이다.
 *
 * 이 핀들은 원래 CP2102N 의 RTS/CTS 인데 **흐름제어를 쓰지 않기로 해서**
 * 비었다 (Uart.h 주석). P1 을 쓰면 AIN 이나 버튼/LED 와 반드시 겹치는데
 * (P1 15핀이 전부 무언가에 배정돼 있다) 여기로 오면 충돌이 없다.
 * 게다가 P1 헤더의 4번·5번으로 물리적으로 인접해 있다.
 *
 * Wire1 이 필요하면 TWIM20~22(P1 도메인)로 별도 배정한다. */
#define PIN_WIRE_SDA          _PINNUM(0, 2)
#define PIN_WIRE_SCL          _PINNUM(0, 3)
static const uint8_t SDA = PIN_WIRE_SDA;
static const uint8_t SCL = PIN_WIRE_SCL;

/* ═══════════════════════════════════════════════════════════════════
 * 전원 도메인 검증 (nrf54l_domains.h)
 * ═══════════════════════════════════════════════════════════════════
 * nRF54L 은 페리페럴 인스턴스가 자기 도메인의 GPIO 포트만 쓸 수 있다.
 * 잘못 배정하면 런타임에 조용히 동작하지 않으므로 여기서 빌드를 막는다.
 * 새 보드 variant 를 만들 때 이 블록을 반드시 복사해 오라.
 */
NRF54L_ASSERT_DOMAIN30_PIN(PIN_SERIAL_TX,  "Serial(UARTE30) TX");
NRF54L_ASSERT_DOMAIN30_PIN(PIN_SERIAL_RX,  "Serial(UARTE30) RX");

NRF54L_ASSERT_SPIM00_PIN(PIN_SPI_SCK,      "SPI SCK");
NRF54L_ASSERT_SPIM00_PIN(PIN_SPI_MOSI,     "SPI MOSI");
NRF54L_ASSERT_SPIM00_PIN(PIN_SPI_MISO,     "SPI MISO");

NRF54L_ASSERT_DOMAIN30_PIN(PIN_WIRE_SDA,   "Wire(TWIM30) SDA");
NRF54L_ASSERT_DOMAIN30_PIN(PIN_WIRE_SCL,   "Wire(TWIM30) SCL");

NRF54L_ASSERT_ANALOG_PIN(PIN_A0, "A0"); NRF54L_ASSERT_ANALOG_PIN(PIN_A1, "A1");
NRF54L_ASSERT_ANALOG_PIN(PIN_A2, "A2"); NRF54L_ASSERT_ANALOG_PIN(PIN_A3, "A3");
NRF54L_ASSERT_ANALOG_PIN(PIN_A4, "A4"); NRF54L_ASSERT_ANALOG_PIN(PIN_A5, "A5");
NRF54L_ASSERT_ANALOG_PIN(PIN_A6, "A6"); NRF54L_ASSERT_ANALOG_PIN(PIN_A7, "A7");

/* NFC 는 NFCT(P1 도메인) 전용 핀이다. */
NRF54L_ASSERT_DOMAIN20_PIN(PIN_NFC1, "NFC1");
NRF54L_ASSERT_DOMAIN20_PIN(PIN_NFC2, "NFC2");

#endif /* _VARIANT_NU54DK_H_ */
