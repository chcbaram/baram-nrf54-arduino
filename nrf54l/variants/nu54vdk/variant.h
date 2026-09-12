/*
 * variant.h — NU54V-DK (nRF54L15)
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * 회로도 분석 결과는 docs/boards/NU54V-DK.md 에 있다. 값을 바꾸기 전에 그걸 먼저 읽어라.
 *
 * ⚠ **NU54-DK 와 다른 보드다.** 한동안 variant 를 공유했는데, 회로도
 *   (`Variant NU-54DK-C`, 2026-07-13)를 확인해 보니 PCB 가 다르다:
 *   온보드 CMSIS-DAP, USB-C, BQ25186 배터리 충전 PMIC, Qwiic I2C 커넥터,
 *   30핀 헤더 2개. LED·버튼·`Serial` 배정만 같다.
 *
 * 핀 번호 규약: Arduino 핀 번호 = 절대 GPIO 번호 (port * 32 + pin).
 *   P0.00~P0.04 →  0 ~  4
 *   P1.00~P1.14 → 32 ~ 46
 *   P2.00~P2.10 → 64 ~ 74
 * Adafruit nRF52 코어와 같은 방식이라 스케치의 _PINNUM(port, pin) 이 그대로 통한다.
 * 중간의 빈 번호는 g_ADigitalPinMap 에서 NC 로 채워져 무시된다.
 */
#ifndef _VARIANT_NU54VDK_H_
#define _VARIANT_NU54VDK_H_

#include <stdint.h>

/* 아래 도메인 검증 매크로(NRF54L_ASSERT_*)를 제공한다.
 * variant.h 는 Arduino.h 를 거치지 않고 직접 include 되기도 하므로
 * 여기서 스스로 챙긴다. */
#include "nrf54l_pinmap.h"

#define _PINNUM(port, pin)    ( ( (port) * 32 ) + (pin) )

/* ── 클럭 ─────────────────────────────────────────────────────────────
 * Y1 32.768 kHz 크리스털이 P1.00(XL1) / P1.01(XL2) 에 실장돼 있다.
 * 크리스털이 없는 보드로 파생시킬 때만 USE_LFRC 로 바꾼다. */
#define USE_LFXO
/* C13/C14 13pF **외부 캡**이 달려 있으므로 내부 캡은 쓰지 않는다.
 * 내부 캡을 쓰는 보드만 LFXO_LOAD_CAP_FF 를 정의한다 (port_grtc.c).
 * 이 상태에서 호스트 대비 +25~38 ppm 으로 실측됐다. */
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

#define PIN_LED1              _PINNUM(2,  9)   /* P2 헤더 23번 */
#define PIN_LED2              _PINNUM(1, 10)   /* P4 헤더 8번  */
#define PIN_LED3              _PINNUM(2,  7)   /* P2 헤더 21번 — SWO 겸용 */
#define PIN_LED4              _PINNUM(1, 14)   /* P4 헤더 12번 — AIN7 겸용 */

#define LED_BUILTIN           PIN_LED1
#define LED_CONN              PIN_LED2

/* Adafruit 스케치 호환용 별칭. 실제 색이 아니라 위치 이름이다. */
#define LED_RED               PIN_LED1
#define LED_BLUE              PIN_LED2

/* ── 버튼 ─────────────────────────────────────────────────────────────
 * ⚠ 외부 풀업이 없다. 회로도에 "USE INTERNAL PULLUP" 이라고 명기돼 있다.
 *   반드시 pinMode(PIN_BUTTONn, INPUT_PULLUP) 으로 쓸 것. 눌리면 LOW. */
#define PIN_BUTTON1           _PINNUM(1, 13)   /* SW1 — AIN6 겸용 */
#define PIN_BUTTON2           _PINNUM(1,  9)   /* SW2 */
#define PIN_BUTTON3           _PINNUM(1,  8)   /* SW3 */
#define PIN_BUTTON4           _PINNUM(0,  4)   /* SW4 */

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
#define PIN_SERIAL_CTS        _PINNUM(0, 2)   /* 미사용. 온보드 DAP 의 RTS 배선 */
#define PIN_SERIAL_RTS        _PINNUM(0, 3)   /* 미사용. 온보드 DAP 의 CTS 배선 */

/*
 * Serial 이 쓸 UARTE 인스턴스. 코어가 아니라 variant 가 고른다.
 * SoC 마다 인스턴스 구성이 다르다 (예: nRF54LM20A 는 UARTE23/24 도 있다).
 * 반드시 핀이 속한 도메인의 인스턴스여야 한다 (nrf54l_domains.h).
 */
#define SERIAL_UARTE_INSTANCE     NRF_UARTE30
/* 벡터 이름. 인스턴스에서 자동으로 유도되지 않으므로 함께 적는다 (Uart.cpp). */
#define SERIAL_UARTE_IRQ_HANDLER  SERIAL30_IRQHandler

#define SERIAL_PORT_MONITOR       Serial
#define SERIAL_PORT_HARDWARE      Serial
#define SERIAL_PORT_HARDWARE_OPEN Serial

/* ── 아날로그 (SAADC) ─────────────────────────────────────────────────
 * 회로도의 AIN 표기 그대로다.
 * ⚠ A6 는 버튼 SW2, A7 은 LED D10 과 핀을 공유한다. 동시에 못 쓴다. */
/* ⚠ **이 보드에서는 AIN 여덟 개가 전부 다른 데 쓰인다.**
 *   A0~A3 = Serial1 → 온보드 DAP (SB9~SB12, 실기 확인)
 *   A4, A5 = PMIC 의 제어·측정 신호 (SB1, SB4 — 실물에서 붙어 있음)
 *   A6     = SW1,  A7 = LED4
 *   전부 헤더에 나와 있으므로 해당 솔더 브리지를 떼면 쓸 수 있다.
 *   Adafruit 호환을 위해 이름은 그대로 두되, 그냥 쓰면 안 된다.
 *   자세한 것은 docs/boards/NU54V-DK.md. */
#define PIN_A0                _PINNUM(1,  4)   /* AIN0 — ⚠ Serial1 TX 겸용 */
#define PIN_A1                _PINNUM(1,  5)   /* AIN1 — ⚠ Serial1 RX 겸용 */
#define PIN_A2                _PINNUM(1,  6)   /* AIN2 — ⚠ Serial1 RTS 겸용 */
#define PIN_A3                _PINNUM(1,  7)   /* AIN3 — ⚠ Serial1 CTS 겸용 */
#define PIN_A4                _PINNUM(1, 11)   /* AIN4 — ⚠ PMIC SB1 겸용 */
#define PIN_A5                _PINNUM(1, 12)   /* AIN5 — ⚠ PMIC SB4 겸용 */
#define PIN_A6                _PINNUM(1, 13)   /* AIN6 — SW1 겸용 */
#define PIN_A7                _PINNUM(1, 14)   /* AIN7 — LED4 겸용 */

static const uint8_t A0 = PIN_A0;
static const uint8_t A1 = PIN_A1;
static const uint8_t A2 = PIN_A2;
static const uint8_t A3 = PIN_A3;
static const uint8_t A4 = PIN_A4;
static const uint8_t A5 = PIN_A5;
static const uint8_t A6 = PIN_A6;
static const uint8_t A7 = PIN_A7;

#define ADC_RESOLUTION        12

/* ── NFC — **이 보드에는 없다** ───────────────────────────────────────
 * P1.02 / P1.03 은 칩의 NFC1 / NFC2 겸용 핀이지만, 이 보드에서는
 * `SB14` / `SB15` 로 **Qwiic 커넥터에 배선돼 있다** (위 I2C 참조).
 * 그래서 PIN_NFC1 / PIN_NFC2 를 정의하지 않는다 — 정의해 두면 NFC 안테나가
 * 달린 것처럼 보인다. */

/* ── SPI — SPIM00 (M2) ────────────────────────────────────────────────
 * SPIM00 은 고속 도메인이라 **P2 핀만** 쓸 수 있다 (nrf54l_domains.h).
 * ⚠ 도메인은 맞지만 P2 안에서 어느 핀이 어느 신호로 갈 수 있는지는 미확정이다.
 *   M2 에서 Product Spec 의 GPIO 배치표로 확인하고 실기 검증할 것.
 *   고속 신호라 OUTPUT_H0H1 또는 OUTPUT_E0E1 드라이브가 필요할 수 있다
 *   (CLAUDE.md §4 SPI 주의사항). */
#define PIN_SPI_SCK           _PINNUM(2, 1)
#define PIN_SPI_MOSI          _PINNUM(2, 2)
#define PIN_SPI_MISO          _PINNUM(2, 4)
#define SPI_SPIM_INSTANCE         NRF_SPIM00
#define SPI_SPIM_IRQ_HANDLER      SERIAL00_IRQHandler
static const uint8_t SS   = _PINNUM(2, 5);
static const uint8_t SCK  = PIN_SPI_SCK;
static const uint8_t MOSI = PIN_SPI_MOSI;
static const uint8_t MISO = PIN_SPI_MISO;

/* ── I2C — TWIM22, Qwiic 커넥터 J5 ───────────────────────────────────
 * **보드에 Qwiic(4핀) 커넥터가 실장돼 있고 2.1K 풀업(R29/R30)이 붙어 있다.**
 * J5: 1=GND 2=VDD_MOD 3=SDA 4=SCL (Qwiic 표준 배열).
 * 그래서 NU54-DK 와 달리 `Wire` 를 억지로 배정할 필요가 없다 — 보드가
 * 정해 준 자리가 있다.
 *
 * P1 은 도메인 20 이라 TWIM20/21/22 중 아무거나 되고, `Serial`(UARTE30)과
 * 블록이 겹치지 않는다 (docs/PERIPHERAL-PINMAP.md §0). */
#define PIN_WIRE_SDA          _PINNUM(1, 2)    /* J5-3, SB14 */
#define PIN_WIRE_SCL          _PINNUM(1, 3)    /* J5-4, SB15 */
/* ⚠ **온보드 BQ25186 PMIC 도 이 버스에 있다** (0x6A). Qwiic 에 꽂는 장치가
 *   0x6A 를 쓰면 충돌한다. 실기 스캔: `Wire : 0x6A 0x70`. */
#define WIRE_TWIM_INSTANCE        NRF_TWIM22
#define WIRE_TWIM_IRQ_HANDLER     SERIAL22_IRQHandler
static const uint8_t SDA = PIN_WIRE_SDA;
static const uint8_t SCL = PIN_WIRE_SCL;

/* ── 보드가 이미 쓰고 있는 핀 ────────────────────────────────────────
 *
 * 아래는 **정의만 해 두고 코어가 건드리지 않는다.** 스케치가 알고 쓰라는
 * 뜻이지 못 쓴다는 뜻은 아니다 — 전부 솔더 브리지를 떼면 풀린다.
 *
 * ⚠ **PMIC — BQ25186 배터리 충전기.**
 *
 *   **I2C 는 `Wire` 와 같은 버스다** — P1.02 / P1.03 에 0x6A 로 붙어 있다.
 *   실기 스캔에서 Qwiic 의 센서와 **나란히 잡혔다** (0x6A, 0x70).
 *
 *   ⚠ 한동안 PMIC I2C 가 P1.11 / P1.12 에 있다고 적어 두었는데 **틀렸다.**
 *      `SB1`~`SB4` 가 그 핀들을 잡는 것은 맞지만 I2C 가 아니라 제어·측정
 *      신호다 (nINT / nPG / nCE / VBAT_MON). 회로도에서 포트 이름을 못 읽어
 *      추론했다가 실기에서 뒤집혔다.
 *
 *   `SB1`~`SB4` 는 **실물에서 붙어 있는 것으로 확인됐다** (2026-09-12).
 *   따라서 아래 네 핀은 자유 GPIO 가 아니다. 어느 것이 어느 신호인지는
 *   아직 확정하지 못했다 — **VBAT_MON 은 아날로그라 A4 / A5 중 하나일
 *   가능성이 높다.** */
#define PIN_PMIC_SB1          _PINNUM(1, 11)   /* = A4. SB1 */
#define PIN_PMIC_SB2          _PINNUM(2,  8)   /* SB2 */
#define PIN_PMIC_SB3          _PINNUM(2, 10)   /* SB3 */
#define PIN_PMIC_SB4          _PINNUM(1, 12)   /* = A5. SB4 */

/** PMIC 의 I2C 주소. `Wire` 에 붙어 있다. */
#define PMIC_I2C_ADDRESS      (0x6A)

/* ── Serial1 — 두 번째 UART, 온보드 DAP 으로 간다 ────────────────────
 *
 * **실기에서 확인했다 (2026-09-12)**: 호스트에 USB 시리얼 포트가 **두 개**
 * 잡히고, 두 번째 포트로 보낸 것이 여기로 나온다.
 *
 *     첫 번째 포트  Serial   P0.00 / P0.01   (SB5~SB8)
 *     두 번째 포트  Serial1  P1.04 / P1.05   (SB9~SB12)
 *
 * `SB9`~`SB12` 가 붙어 있다는 뜻이다.
 *
 * ⚠ **그래서 A0~A3 는 이 UART 가 물고 있다.** `analogRead(A0..A3)` 을 쓰려면
 *   해당 브리지를 떼야 한다. 떼면 이 Serial1 이 죽는다. 헤더에 다 나와 있으니
 *   선택은 사용자 몫이다 — docs/boards/NU54V-DK.md §4. */
#define PIN_SERIAL1_TX            _PINNUM(1, 4)    /* = A0. SB9  */
#define PIN_SERIAL1_RX            _PINNUM(1, 5)    /* = A1. SB10 */
#define SERIAL1_UARTE_INSTANCE    NRF_UARTE20
#define SERIAL1_UARTE_IRQ_HANDLER SERIAL20_IRQHandler

/* 흐름제어선. 코어는 쓰지 않는다. */
#define PIN_SERIAL1_RTS       _PINNUM(1, 6)    /* = A2. SB11 */
#define PIN_SERIAL1_CTS       _PINNUM(1, 7)    /* = A3. SB12 */

/* ═══════════════════════════════════════════════════════════════════
 * 핀 배정 검증 — **빌드에서 막는다**
 * ═══════════════════════════════════════════════════════════════════
 * nRF54L 은 페리페럴이 아무 핀에나 붙지 않는다. 잘못 배정하면 런타임에
 * 조용히 동작하지 않아 원인이 보이지 않으므로 여기서 컴파일을 멈춘다.
 *
 * `nrf54l_pinmap.h` 의 신호 단위 검사를 쓴다 (`nrf54l_domains.h` 의
 * 포트 단위보다 촘촘하다 — 예: P2.03 은 P2 지만 SPIM00.SCK 로는 못 쓴다).
 * 표는 Nordic Pin Planner 에서 생성되며 신호 이름은
 * `libraries/PinMap` 예제 주석의 표와 같다.
 *
 * 새 보드 variant 를 만들 때 이 블록을 반드시 복사해 오라.
 */
NRF54L_ASSERT_SIG(PIN_SERIAL_TX,  UARTE30_TXD, "Serial(UARTE30) TX");
NRF54L_ASSERT_SIG(PIN_SERIAL_RX,  UARTE30_RXD, "Serial(UARTE30) RX");
NRF54L_ASSERT_SIG(PIN_SERIAL_CTS, UARTE30_CTS, "Serial(UARTE30) CTS");
NRF54L_ASSERT_SIG(PIN_SERIAL_RTS, UARTE30_RTS, "Serial(UARTE30) RTS");

NRF54L_ASSERT_SIG(PIN_SPI_SCK,    SPIM00_SCK,  "SPI SCK");
NRF54L_ASSERT_SIG(PIN_SPI_MOSI,   SPIM00_SDO,  "SPI MOSI");
NRF54L_ASSERT_SIG(PIN_SPI_MISO,   SPIM00_SDI,  "SPI MISO");

NRF54L_ASSERT_SIG(PIN_WIRE_SDA,   TWIM22_SDA,  "Wire(TWIM22) SDA");
NRF54L_ASSERT_SIG(PIN_WIRE_SCL,   TWIM22_SCL,  "Wire(TWIM22) SCL");

/* AIN 번호까지 맞는지 본다. 포트 검사만으로는 A4 가 진짜 AIN4 인지 알 수 없다. */
NRF54L_ASSERT_SIG(PIN_A0, SAADC_AIN0, "A0");  NRF54L_ASSERT_SIG(PIN_A1, SAADC_AIN1, "A1");
NRF54L_ASSERT_SIG(PIN_A2, SAADC_AIN2, "A2");  NRF54L_ASSERT_SIG(PIN_A3, SAADC_AIN3, "A3");
NRF54L_ASSERT_SIG(PIN_A4, SAADC_AIN4, "A4");  NRF54L_ASSERT_SIG(PIN_A5, SAADC_AIN5, "A5");
NRF54L_ASSERT_SIG(PIN_A6, SAADC_AIN6, "A6");  NRF54L_ASSERT_SIG(PIN_A7, SAADC_AIN7, "A7");

/* Qwiic 은 P1.02/P1.03 이고 그 핀들은 칩의 NFC 겸용이다 — TWIM 으로도 쓸 수 있다. */
NRF54L_ASSERT_SIG(PIN_SERIAL1_TX, UARTE20_TXD, "Serial1 TX (A0)");
NRF54L_ASSERT_SIG(PIN_SERIAL1_RX, UARTE20_RXD, "Serial1 RX (A1)");

/* ⚠ LED1(P2.09)·LED3(P2.07)은 P2 다. P2 를 담당하는 GPIOTE 도 PWM 도 없으므로
 *   그 두 핀에는 attachInterrupt 도 analogWrite 도 걸리지 않는다.
 *   LED_BUILTIN 이 LED1 이라는 점에 주의하라 — PWM 시험은 LED2/LED4 로 한다. */
NRF54L_ASSERT_SIG(PIN_BUTTON1, GPIOTE20_CHAN_0, "SW2 (인터럽트 가능해야 한다)");
NRF54L_ASSERT_SIG(PIN_BUTTON4, GPIOTE30_CHAN_0, "SW5 (인터럽트 가능해야 한다)");

#endif /* _VARIANT_NU54VDK_H_ */
