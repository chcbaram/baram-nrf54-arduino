/*
 * variant.h — Seeed Studio XIAO nRF54L15 / XIAO nRF54L15 Sense
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * 회로도 분석 결과는 docs/boards/XIAO-nRF54L15.md 에 있다.
 * 값을 바꾸기 전에 그걸 먼저 읽어라.
 *
 * 핀 번호 규약: Arduino 핀 번호 = 절대 GPIO 번호 (port * 32 + pin).
 *   P0.00~P0.04 →  0 ~  4
 *   P1.00~P1.15 → 32 ~ 47
 *   P2.00~P2.10 → 64 ~ 74
 * NU54-DK variant 와 같은 규약이라 두 보드 사이에 스케치를 옮기기 쉽다.
 * XIAO 헤더 이름(D0~D10, A0~A3)은 아래에 별칭으로 따로 둔다.
 */
#ifndef _VARIANT_XIAO_NRF54L15_H_
#define _VARIANT_XIAO_NRF54L15_H_

#include <stdint.h>

#include "nrf54l_domains.h"

#define _PINNUM(port, pin)    ( ( (port) * 32 ) + (pin) )

/* ── 클럭 ─────────────────────────────────────────────────────────────
 * X2 32.768 kHz ±20 ppm (7 pF) 가 P1.00(XL1) / P1.01(XL2) 에 실장돼 있다.
 * 빠뜨리면 내부 RC 로 돌아 +9000 ppm 이 된다 (CLAUDE.md §7 F12).
 * X1 32 MHz ±10 ppm HFXO 도 실장돼 있다 (BLE 에 필요). */
#define USE_LFXO
/* ⚠ 이 보드에는 **외부 로드 캡이 없다.** X2 옆에 캡이 실장돼 있지 않고
 * 칩 내부 캡에 의존한다 (NU54-DK 는 C13/C14 13pF 가 있다).
 * 외부 캡으로 두면 부하용량이 모자라 발진이 빨라진다 — **실측 +805 ppm.**
 * BLE 요구치 ±250 ppm 을 넘으므로 M3 에서 연결이 끊긴다.
 *
 * 16000 fF 는 Zephyr 보드 정의(boards/seeed/xiao_nrf54l15,
 * &lfxo { load-capacitors = "internal"; load-capacitance-femtofarad = <16000>; })
 * 에서 가져왔다. 이 값으로 **실측 -14 ppm** (크리스털 사양 ±20 ppm 안).
 *
 * 레지스터 값이 아니라 용량(fF)을 준다. 변환에 FICR.XOSC32KTRIM 이 들어가고
 * 그 트림은 칩 개체마다 다르므로 코어가 런타임에 계산한다 (port_grtc.c). */
#define LFXO_LOAD_CAP_FF      16000
#define VARIANT_MCK           (128000000ul)

/* ── 핀 개수 ─────────────────────────────────────────────────────────
 * P2.10 = 74 가 마지막이다. */
#define PINS_COUNT            (75u)
#define NUM_DIGITAL_PINS      (75u)
#define NUM_ANALOG_INPUTS     (4u)
#define NUM_ANALOG_OUTPUTS    (0u)

/* ── LED ──────────────────────────────────────────────────────────────
 * 사용자 LED 는 **녹색 하나뿐**이다 (D3). 회로가
 *   VSYS_3V3 ─ R15 1.5K ─ D3 ─ P2.00
 * 이라 핀이 캐소드 쪽이다 → **active LOW**. NU54-DK 와 정반대다.
 * ledOn()/ledOff() 를 쓰면 이 차이를 신경 쓰지 않아도 된다.
 *
 * 충전 LED(D2, 적색)는 충전 IC 가 직접 구동한다. MCU 가 제어할 수 없다. */
#define LED_STATE_ON          0

#define PIN_LED1              _PINNUM(2, 0)    /* D3 녹색 — USR_LED */

#define LED_BUILTIN           PIN_LED1
/* LED 가 하나뿐이라 아래 별칭은 전부 같은 핀을 가리킨다.
 * Adafruit 예제가 LED_RED / LED_BLUE / LED_CONN 을 쓰므로 컴파일은 되게 둔다
 * (CLAUDE.md R12 — 호환 우선). */
#define LED_CONN              PIN_LED1
#define LED_RED               PIN_LED1
#define LED_BLUE              PIN_LED1

/* ── 버튼 ─────────────────────────────────────────────────────────────
 * K2(USR_KEY) 하나. **외부 10K 풀업(R13)이 있다** — NU54-DK 와 다르다.
 * 그래도 initVariant 는 INPUT_PULLUP 으로 잡는다(내부·외부 병렬, 무해).
 * 눌리면 LOW.
 *
 * K1 은 RESET 에 직결이라 MCU 에서 읽을 수 없다. */
#define PIN_BUTTON1           _PINNUM(0, 0)    /* USR_KEY */

/* ── UART ─────────────────────────────────────────────────────────────
 * Serial = UARTE20. 온보드 ATSAMD11D14A(CMSIS-DAP)가 USB CDC 브리지를
 * 겸하므로 **케이블 하나로 업로드와 Serial 이 같이 된다.**
 *
 * 넷 이름이 SAMD11 기준이다: SAMD11_TX 는 SAMD11 이 보내는 선이므로
 * nRF54L15 쪽에서는 RX 다.
 *   P1.08 = SAMD11_TX → nRF RX
 *   P1.09 = SAMD11_RX ← nRF TX
 *
 * P1 은 도메인 20 이므로 UARTE20/21/22 만 쓸 수 있다
 * (docs/PERIPHERAL-PINMAP.md). 회로도도 이 두 핀을 UART20 으로 묶어 놨다.
 *
 * 흐름제어는 배선 자체가 없다. */
#define PIN_SERIAL_RX         _PINNUM(1, 8)
#define PIN_SERIAL_TX         _PINNUM(1, 9)

#define SERIAL_UARTE_INSTANCE     NRF_UARTE20
/* 벡터 이름. 인스턴스에서 자동으로 유도되지 않으므로 함께 적는다 (Uart.cpp). */
#define SERIAL_UARTE_IRQ_HANDLER  SERIAL20_IRQHandler

#define SERIAL_PORT_MONITOR       Serial
#define SERIAL_PORT_HARDWARE      Serial
#define SERIAL_PORT_HARDWARE_OPEN Serial

/* ── XIAO 헤더 (14핀) ─────────────────────────────────────────────────
 * XIAO 폼팩터의 관용 이름이다. 기존 XIAO 스케치를 그대로 옮길 수 있게 둔다. */
static const uint8_t D0  = _PINNUM(1,  4);
static const uint8_t D1  = _PINNUM(1,  5);
static const uint8_t D2  = _PINNUM(1,  6);
static const uint8_t D3  = _PINNUM(1,  7);
static const uint8_t D4  = _PINNUM(1, 10);   /* SDA */
static const uint8_t D5  = _PINNUM(1, 11);   /* SCL */
static const uint8_t D6  = _PINNUM(2,  8);   /* 헤더 TX 자리 — 아래 주석 참조 */
static const uint8_t D7  = _PINNUM(2,  7);   /* 헤더 RX 자리 — SWO 겸용 */
static const uint8_t D8  = _PINNUM(2,  1);   /* SCK  */
static const uint8_t D9  = _PINNUM(2,  4);   /* MISO */
static const uint8_t D10 = _PINNUM(2,  2);   /* MOSI */
static const uint8_t D11 = _PINNUM(0,  3);   /* IMU SCL 겸용 */
static const uint8_t D12 = _PINNUM(0,  4);   /* IMU SDA 겸용 */
static const uint8_t D13 = _PINNUM(2, 10);
static const uint8_t D14 = _PINNUM(2,  9);
static const uint8_t D15 = _PINNUM(2,  6);

/*
 * ⚠ D6 / D7 에 Serial1 을 아직 붙이지 않았다.
 *
 *   회로도와 Zephyr 보드 정의가 모두 이 두 핀을 **UARTE21** 로 잡는다
 *   (zephyr boards/seeed/xiao_nrf54l15 의 uart21_default:
 *    UART_TX = P2.08, UART_RX = P2.07).
 *
 *   그런데 docs/PERIPHERAL-PINMAP.md 의 도메인 규칙("인스턴스 2x 는 P1")
 *   대로면 P2 를 쓸 수 없다. 즉 **그 규칙에 반례가 있다.** 규칙을
 *   Product Specification 으로 바로잡기 전에는 배정하지 않는다 (M2).
 *   지금 이 두 핀은 일반 GPIO 로 쓸 수 있다.
 *
 *   바로잡히면 아래 네 줄만 추가하면 된다:
 *     #define PIN_SERIAL1_TX            D6
 *     #define PIN_SERIAL1_RX            D7
 *     #define SERIAL1_UARTE_INSTANCE    NRF_UARTE21
 *     #define SERIAL1_UARTE_IRQ_HANDLER SERIAL21_IRQHandler
 */

/* ── 아날로그 (SAADC) ─────────────────────────────────────────────────
 * ⚠ XIAO nRF52840 과 다르다. 이 보드에서 **A4 / A5 는 없다** —
 *   D4(P1.10)에 AIN 이 배정돼 있지 않기 때문이다. AIN 은 P1.04~07 과
 *   P1.11~14 뿐이고, 그중 P1.11~13 은 SCL / 마이크가 이미 쓴다. */
#define PIN_A0                _PINNUM(1, 4)    /* AIN0 — D0 */
#define PIN_A1                _PINNUM(1, 5)    /* AIN1 — D1 */
#define PIN_A2                _PINNUM(1, 6)    /* AIN2 — D2 */
#define PIN_A3                _PINNUM(1, 7)    /* AIN3 — D3 */

static const uint8_t A0 = PIN_A0;
static const uint8_t A1 = PIN_A1;
static const uint8_t A2 = PIN_A2;
static const uint8_t A3 = PIN_A3;

#define ADC_RESOLUTION        12

/* ── 배터리 전압 ──────────────────────────────────────────────────────
 * VBAT ─ TPS22916(로드 스위치) ─ R5 10K ─┬─ P1.14/AIN7
 *                                        └─ R6 10K ─ GND
 * 분압 경로가 로드 스위치로 게이팅돼 있다. 평소에는 끊어 두어 누설이 없고,
 * 읽을 때만 PIN_VBAT_ENABLE 을 HIGH 로 올린다.
 *   배터리 전압 = ADC 로 읽은 전압 * VBAT_DIVIDER */
#define PIN_VBAT              _PINNUM(1, 14)   /* AIN7 */
#define PIN_VBAT_ENABLE       _PINNUM(1, 15)
#define VBAT_DIVIDER          (2.0f)

/* ── 온보드 센서 (Sense 모델에만 실장) ────────────────────────────────
 * IMU 와 마이크는 TPS22916 로드 스위치 뒤에 있다.
 * PIN_SENSOR_POWER 를 HIGH 로 올려야 전원이 들어간다 (0: 차단, 1: 공급).
 * 안 쓰면 LOW 로 두는 것이 저전력에 유리하다. */
#define PIN_SENSOR_POWER      _PINNUM(0, 1)    /* IMU & MIC 3V3 EN */

/* IMU — LSM6DS3TR-C, I2C 주소 0x6A. TWIM30(도메인 30 = P0). */
#define PIN_IMU_SDA           _PINNUM(0, 4)
#define PIN_IMU_SCL           _PINNUM(0, 3)
#define PIN_IMU_INT1          _PINNUM(0, 2)
#define IMU_I2C_ADDRESS       (0x6A)

/* PDM 마이크 — MSM261DGT006. PDM20(도메인 20 = P1). */
#define PIN_PDM_CLK           _PINNUM(1, 12)
#define PIN_PDM_DATA          _PINNUM(1, 13)

/* ── RF 스위치 ────────────────────────────────────────────────────────
 * 안테나가 FM8625H RF 스위치를 지나간다. 스위치에 전원이 없으면
 * RF 경로가 성립하지 않으므로 **BLE 를 쓰려면 반드시 켜야 한다.**
 *   PIN_RF_SW_POWER = HIGH  → 스위치 전원
 *   PIN_RF_SW_SELECT = LOW  → RF1 = 온보드 칩 안테나 (ANT1)
 *                    = HIGH → RF2 = u.FL 커넥터 (ANT2)
 * initVariant 가 온보드 안테나로 잡아 둔다. M3 에서 실측 확인할 것. */
#define PIN_RF_SW_POWER       _PINNUM(2, 3)
#define PIN_RF_SW_SELECT      _PINNUM(2, 5)
#define RF_SW_SELECT_ONBOARD  0
#define RF_SW_SELECT_UFL      1

/* ── NFC ──────────────────────────────────────────────────────────────
 * NFC 안테나 핀으로 쓸지 GPIO 로 쓸지는 UICR 설정이다. */
#define PIN_NFC1              _PINNUM(1, 2)
#define PIN_NFC2              _PINNUM(1, 3)

/* ── SPI — SPIM00 (M2) ────────────────────────────────────────────────
 * XIAO 헤더의 D8/D9/D10. SPIM00 은 고속 도메인이라 **P2 핀만** 쓸 수 있고
 * 실제로 셋 다 P2 다.
 * ⚠ P2 안에서 어느 핀이 어느 신호로 갈 수 있는지는 아직 미확정이다
 *   (docs/PERIPHERAL-PINMAP.md §4). M2 에서 실기 검증할 것. */
#define PIN_SPI_SCK           _PINNUM(2, 1)    /* D8  */
#define PIN_SPI_MISO          _PINNUM(2, 4)    /* D9  */
#define PIN_SPI_MOSI          _PINNUM(2, 2)    /* D10 */
static const uint8_t SS   = _PINNUM(2, 7);     /* D7 — 관례상 자리. 전용 배선 없음 */
static const uint8_t SCK  = PIN_SPI_SCK;
static const uint8_t MOSI = PIN_SPI_MOSI;
static const uint8_t MISO = PIN_SPI_MISO;

/* ── I2C ──────────────────────────────────────────────────────────────
 * Wire  = 헤더의 D4/D5 (P1.10/P1.11). P1 은 도메인 20 이므로 TWIM20~22.
 *         회로도 표기대로 TWIM22 를 쓴다.
 * Wire1 = 온보드 IMU (P0.04/P0.03). P0 는 도메인 30 이므로 TWIM30.
 *         Sense 모델이 아니면 아무것도 안 붙어 있다. */
#define PIN_WIRE_SDA          _PINNUM(1, 10)   /* D4 */
#define PIN_WIRE_SCL          _PINNUM(1, 11)   /* D5 */
#define WIRE_TWIM_INSTANCE        NRF_TWIM22
#define WIRE_TWIM_IRQ_HANDLER     SERIAL22_IRQHandler
static const uint8_t SDA = PIN_WIRE_SDA;
static const uint8_t SCL = PIN_WIRE_SCL;

#define PIN_WIRE1_SDA         PIN_IMU_SDA
#define PIN_WIRE1_SCL         PIN_IMU_SCL
#define WIRE1_TWIM_INSTANCE       NRF_TWIM30
#define WIRE1_TWIM_IRQ_HANDLER    SERIAL30_IRQHandler

/*
 * 블록 충돌 확인 (docs/PERIPHERAL-PINMAP.md §0):
 *   Serial  UARTE20  0x500C6000
 *   Wire    TWIM22   0x500C8000
 *   Wire1   TWIM30   0x50104000
 *   SPI     SPIM00   0x5004A000
 * 넷 다 다른 블록이라 동시에 쓸 수 있다.
 */

/* ═══════════════════════════════════════════════════════════════════
 * 전원 도메인 검증 (nrf54l_domains.h)
 * ═══════════════════════════════════════════════════════════════════
 * nRF54L 은 페리페럴 인스턴스가 자기 도메인의 GPIO 포트만 쓸 수 있다.
 * 잘못 배정하면 런타임에 조용히 동작하지 않으므로 여기서 빌드를 막는다.
 */
NRF54L_ASSERT_DOMAIN20_PIN(PIN_SERIAL_TX,  "Serial(UARTE20) TX");
NRF54L_ASSERT_DOMAIN20_PIN(PIN_SERIAL_RX,  "Serial(UARTE20) RX");

NRF54L_ASSERT_SPIM00_PIN(PIN_SPI_SCK,      "SPI SCK");
NRF54L_ASSERT_SPIM00_PIN(PIN_SPI_MOSI,     "SPI MOSI");
NRF54L_ASSERT_SPIM00_PIN(PIN_SPI_MISO,     "SPI MISO");

NRF54L_ASSERT_DOMAIN20_PIN(PIN_WIRE_SDA,   "Wire(TWIM22) SDA");
NRF54L_ASSERT_DOMAIN20_PIN(PIN_WIRE_SCL,   "Wire(TWIM22) SCL");

NRF54L_ASSERT_DOMAIN30_PIN(PIN_WIRE1_SDA,  "Wire1(TWIM30) SDA — 온보드 IMU");
NRF54L_ASSERT_DOMAIN30_PIN(PIN_WIRE1_SCL,  "Wire1(TWIM30) SCL — 온보드 IMU");

NRF54L_ASSERT_DOMAIN20_PIN(PIN_PDM_CLK,    "PDM(PDM20) CLK");
NRF54L_ASSERT_DOMAIN20_PIN(PIN_PDM_DATA,   "PDM(PDM20) DATA");

NRF54L_ASSERT_ANALOG_PIN(PIN_A0, "A0"); NRF54L_ASSERT_ANALOG_PIN(PIN_A1, "A1");
NRF54L_ASSERT_ANALOG_PIN(PIN_A2, "A2"); NRF54L_ASSERT_ANALOG_PIN(PIN_A3, "A3");
NRF54L_ASSERT_ANALOG_PIN(PIN_VBAT, "VBAT (AIN7)");

/* NFC 는 NFCT(P1 도메인) 전용 핀이다. */
NRF54L_ASSERT_DOMAIN20_PIN(PIN_NFC1, "NFC1");
NRF54L_ASSERT_DOMAIN20_PIN(PIN_NFC2, "NFC2");

#endif /* _VARIANT_XIAO_NRF54L15_H_ */
