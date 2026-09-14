/*
 * variant.h — Seeed Studio XIAO nRF54LM20A / XIAO nRF54LM20A Sense
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * 회로도 분석 결과는 docs/boards/XIAO-nRF54LM20A.md 에 있다.
 * 값을 바꾸기 전에 그걸 먼저 읽어라. 회로도는 Seeed XIAO_nRF54LM20A_Schematic
 * V1.0 (2026-04-09) 이고, 버스 배정은 Zephyr boards/seeed/xiao_nrf54lm20a 와 대조했다.
 *
 * ⚠ XIAO nRF54L15 와 폼팩터만 같고 **칩이 다르다** — nRF54LM20A (FCCSP98).
 *   GPIO 포트가 넷(P0~P3)이고, 도메인 20 이 **P1 과 P3 를 함께** 소유한다
 *   (nrf54l_domains.h). L15 variant 에서 핀 배정을 옮겨 오지 마라 —
 *   Serial 핀부터 다르다.
 *
 * 핀 번호 규약: Arduino 핀 번호 = 절대 GPIO 번호 (port * 32 + pin).
 *   P0.00~P0.09 →   0 ~   9
 *   P1.00~P1.31 →  32 ~  63
 *   P2.00~P2.10 →  64 ~  74
 *   P3.00~P3.12 →  96 ~ 108
 * 다른 variant 와 같은 규약이다. XIAO 헤더 이름(D0~D27, A0~A7)은 별칭으로 둔다.
 */
#ifndef _VARIANT_XIAO_NRF54LM20A_H_
#define _VARIANT_XIAO_NRF54LM20A_H_

#include <stdint.h>

#include "nrf54l_pinmap.h"

#define _PINNUM(port, pin)    ( ( (port) * 32 ) + (pin) )

/* ── 클럭 ─────────────────────────────────────────────────────────────
 * X1 32.768 kHz (7 pF ±20 ppm) 가 P1.20(XL1) / P1.21(XL2) 에,
 * X2 32 MHz (8 pF ±10 ppm) 가 XC1/XC2 에 실장돼 있다.
 *
 * **두 크리스털 모두 외부 로드 캡이 없다.** 회로도에 "The internal capacitance
 * of the matching capacitor is configurable" 이라고 적혀 있다. 그래서 둘 다
 * 칩 내부 캡으로 맞춘다 (freertos/port_grtc.c). 빠뜨리면 LFXO 는 수백 ppm
 * 어긋나고(§7 F12, XIAO nRF54L15 실측 +805 ppm), HFXO 는 라디오에서만 드러난다.
 *
 * 값은 Zephyr 보드 정의(nrf54lm20a_cpuapp_common.dtsi)에서 가져왔다:
 *   &lfxo { load-capacitors = "internal"; load-capacitance-femtofarad = <17000>; };
 *   &hfxo { load-capacitors = "internal"; load-capacitance-femtofarad = <15000>; };
 * ⚠ XIAO nRF54L15 의 LFXO(16000 fF)와 값이 다르다. 호스트 대비 실측으로 확인한다.
 */
#define USE_LFXO
#define LFXO_LOAD_CAP_FF      17000
#define HFXO_LOAD_CAP_FF      15000
#define VARIANT_MCK           (128000000ul)

/* ── 핀 개수 ─────────────────────────────────────────────────────────
 * P3.12 = 108 이 마지막이다. */
#define PINS_COUNT            (109u)
#define NUM_DIGITAL_PINS      (109u)
#define NUM_ANALOG_INPUTS     (8u)
#define NUM_ANALOG_OUTPUTS    (0u)

/* ── LED — RGB 하나, active LOW ───────────────────────────────────────
 * 공통 애노드 RGB LED(LED1)의 애노드가 VSYS_3V3 이고, 캐소드가 각각 2K 를
 * 거쳐 핀으로 온다 → **핀을 LOW 로 내려야 켜진다.**
 *
 * 세 핀 모두 P1 이라 **analogWrite 와 attachInterrupt 가 된다.**
 * XIAO nRF54L15 는 LED 가 P2.00 하나뿐이라 둘 다 안 됐다.
 *
 * 충전 LED(D2, 적색)는 nPM1300 이 직접 구동한다. MCU 에서 못 만진다. */
#define LED_STATE_ON          0

#define PIN_LED1              _PINNUM(1, 22)   /* 빨강 */
#define PIN_LED2              _PINNUM(1, 23)   /* 파랑 */
#define PIN_LED3              _PINNUM(1, 24)   /* 초록 */

#define LED_BUILTIN           PIN_LED1
/* Adafruit 보드와 같은 관례: LED_BUILTIN 은 빨강, 연결 표시는 파랑 (R12). */
#define LED_RED               PIN_LED1
#define LED_BLUE              PIN_LED2
#define LED_GREEN             PIN_LED3
#define LED_CONN              LED_BLUE

/* ── 버튼 ─────────────────────────────────────────────────────────────
 * K2(USR_KEY). **외부 100K 풀업(R27)이 있다.** 눌리면 LOW.
 * P0 이라 GPIOTE30 으로 인터럽트가 된다.
 * K1 은 nRF54_RESET 직결이라 MCU 에서 읽을 수 없다. */
#define PIN_BUTTON1           _PINNUM(0, 9)    /* USR_KEY */

/* ── UART ─────────────────────────────────────────────────────────────
 * Serial = UARTE20. 온보드 ATSAMD11D14A(CMSIS-DAP)가 USB CDC 브리지를 겸하고,
 * 그 사이에 UM3204H 레벨 버퍼가 있다.
 *   P1.11 = nRF54_TX → SAMD11
 *   P1.10 = nRF54_RX ← SAMD11
 *
 * ⚠ XIAO nRF54L15 (P1.08/P1.09) 와 핀이 다르다. 이 보드에서 그 두 핀은
 *   헤더 D6/D7 이다 (아래 Serial1).
 * 흐름제어 배선은 없다. */
#define PIN_SERIAL_TX         _PINNUM(1, 11)
#define PIN_SERIAL_RX         _PINNUM(1, 10)

#define SERIAL_UARTE_INSTANCE     NRF_UARTE20
/* 벡터 이름. 인스턴스에서 자동으로 유도되지 않으므로 함께 적는다 (Uart.cpp). */
#define SERIAL_UARTE_IRQ_HANDLER  SERIAL20_IRQHandler

#define SERIAL_PORT_MONITOR       Serial
#define SERIAL_PORT_HARDWARE      Serial
#define SERIAL_PORT_HARDWARE_OPEN Serial

/* ── Serial1 — 헤더 D6(TX) / D7(RX) ──────────────────────────────────
 * 회로도 넷 이름 `P1.08/Tx/D6`, `P1.09/Rx/D7` 과 Zephyr 의 uart21 배정 그대로다.
 * USB 로는 나가지 않는다 — 헤더에 직접 선을 대야 한다. */
#define PIN_SERIAL1_TX            _PINNUM(1, 8)    /* D6 */
#define PIN_SERIAL1_RX            _PINNUM(1, 9)    /* D7 */
#define SERIAL1_UARTE_INSTANCE    NRF_UARTE21
#define SERIAL1_UARTE_IRQ_HANDLER SERIAL21_IRQHandler

/* ── XIAO 헤더 ────────────────────────────────────────────────────────
 * 회로도 커넥터 U7(XIAO-Add-On-Plus)의 물리 핀:
 *    1~11 = D0~D10
 *   15~20 = D19~D24 (P0.00~P0.05)
 *   21~23 = D25~D27 (P3.09~P3.11)
 *   12 = 3V3_OUT, 13 = GND, 14 = VBUS
 *
 * ⚠ **D11~D18 (P3.00~P3.07) 은 헤더가 아니라 뒷면 테스트 패드**다 (TP11~TP18).
 *   보드 문서의 표는 D 번호 기준이라 이 구분이 없다. */
static const uint8_t D0  = _PINNUM(1,  0);   /* A0 */
static const uint8_t D1  = _PINNUM(1, 31);   /* A1 */
static const uint8_t D2  = _PINNUM(1, 30);   /* A2 */
static const uint8_t D3  = _PINNUM(1, 29);   /* A3 */
static const uint8_t D4  = _PINNUM(1,  3);   /* SDA  / A7 */
static const uint8_t D5  = _PINNUM(1,  7);   /* SCL  — 실크의 A8 은 아날로그가 아니다 */
static const uint8_t D6  = _PINNUM(1,  8);   /* TX   — Serial1 */
static const uint8_t D7  = _PINNUM(1,  9);   /* RX   — Serial1 */
static const uint8_t D8  = _PINNUM(1,  4);   /* SCK  / A6 */
static const uint8_t D9  = _PINNUM(1,  5);   /* MISO / A5 */
static const uint8_t D10 = _PINNUM(1,  6);   /* MOSI / A4 */
static const uint8_t D11 = _PINNUM(3,  0);   /* 뒷면 패드 TP11 */
static const uint8_t D12 = _PINNUM(3,  1);   /* 뒷면 패드 TP12 */
static const uint8_t D13 = _PINNUM(3,  2);   /* 뒷면 패드 TP13 */
static const uint8_t D14 = _PINNUM(3,  3);   /* 뒷면 패드 TP14 */
static const uint8_t D15 = _PINNUM(3,  4);   /* 뒷면 패드 TP15 */
static const uint8_t D16 = _PINNUM(3,  5);   /* 뒷면 패드 TP16 */
static const uint8_t D17 = _PINNUM(3,  6);   /* 뒷면 패드 TP17 */
static const uint8_t D18 = _PINNUM(3,  7);   /* 뒷면 패드 TP18 */
static const uint8_t D19 = _PINNUM(0,  0);
static const uint8_t D20 = _PINNUM(0,  1);
static const uint8_t D21 = _PINNUM(0,  2);
static const uint8_t D22 = _PINNUM(0,  3);
static const uint8_t D23 = _PINNUM(0,  4);
static const uint8_t D24 = _PINNUM(0,  5);
static const uint8_t D25 = _PINNUM(3,  9);
static const uint8_t D26 = _PINNUM(3, 10);
static const uint8_t D27 = _PINNUM(3, 11);

/* ── 아날로그 (SAADC) ─────────────────────────────────────────────────
 * AIN 배정이 nRF54L15 와 **전혀 다르다** (A0 = P1.00, A1 = P1.31 …).
 * 코어는 핀→AIN 을 nrf54l_pinmap.h 로 찾으므로 코드는 그대로 통한다.
 * SAADC 는 LM20A 에서도 P1 전용이다 — P3 에는 AIN 이 없다.
 *
 * ⚠ 실크의 `A8`(D5, P1.07)은 아날로그 입력이 **아니다.** P1.07 에 배정된 것은
 *   SAADC.EXTREF(외부 기준전압)다. 그래서 PIN_A8 을 두지 않는다.
 * ⚠ A4~A7 은 SPI(D8~D10)·Wire(D4) 와 핀을 공유한다. 동시에 못 쓴다.
 * ⚠ 배터리 분압 회로가 없다. 배터리 전압은 nPM1300 이 잰다 (보드 라이브러리). */
#define PIN_A0                _PINNUM(1,  0)   /* AIN0 — D0 */
#define PIN_A1                _PINNUM(1, 31)   /* AIN1 — D1 */
#define PIN_A2                _PINNUM(1, 30)   /* AIN2 — D2 */
#define PIN_A3                _PINNUM(1, 29)   /* AIN3 — D3 */
#define PIN_A4                _PINNUM(1,  6)   /* AIN4 — D10 / MOSI */
#define PIN_A5                _PINNUM(1,  5)   /* AIN5 — D9  / MISO */
#define PIN_A6                _PINNUM(1,  4)   /* AIN6 — D8  / SCK */
#define PIN_A7                _PINNUM(1,  3)   /* AIN7 — D4  / SDA */

static const uint8_t A0 = PIN_A0;
static const uint8_t A1 = PIN_A1;
static const uint8_t A2 = PIN_A2;
static const uint8_t A3 = PIN_A3;
static const uint8_t A4 = PIN_A4;
static const uint8_t A5 = PIN_A5;
static const uint8_t A6 = PIN_A6;
static const uint8_t A7 = PIN_A7;

#define ADC_RESOLUTION        12

/* ── SPI — SPIM23, 헤더 D8/D9/D10 ───────────────────────────────────
 * 헤더의 SPI 핀이 P1 이라 SPIM00(P2 전용)이 아니라 도메인 20 인스턴스를 쓴다.
 * Serial(20)·Serial1(21)·Wire(22) 와 겹치지 않는 23 이다 — Zephyr 의 spi23.
 * ⚠ 도메인 20 의 SPIM 은 최대 8 Mbps 다 (SPIM00 은 32 Mbps). */
#define PIN_SPI_SCK           _PINNUM(1, 4)    /* D8  */
#define PIN_SPI_MISO          _PINNUM(1, 5)    /* D9  */
#define PIN_SPI_MOSI          _PINNUM(1, 6)    /* D10 */
#define SPI_SPIM_INSTANCE         NRF_SPIM23
#define SPI_SPIM_IRQ_HANDLER      SERIAL23_IRQHandler
static const uint8_t SS   = _PINNUM(1, 9);     /* D7 — 관례상 자리. 전용 배선 없음 (Serial1 RX 와 겹친다) */
static const uint8_t SCK  = PIN_SPI_SCK;
static const uint8_t MOSI = PIN_SPI_MOSI;
static const uint8_t MISO = PIN_SPI_MISO;

/* ── SPI1 — SPIM00, 온보드 플래시 PY25Q64 (8 MB) ──────────────────────
 * 플래시를 표준 SPI 로 쓴다 (MOSI = IO0, MISO = IO1). 회로도로 핀 결속을 확정했다:
 *   P2.00 IO3/HOLD   P2.01 CLK   P2.02 IO0   P2.03 IO2/WP   P2.04 IO1   P2.05 CS
 * sQSPI 가 쓸 수 있는 핀이 이 여섯뿐이라 Pin Planner 제약과도 맞는다.
 * Zephyr 도 이 핀을 spi00 으로 잡는다. */
#define PIN_SPI1_SCK          _PINNUM(2, 1)
#define PIN_SPI1_MOSI         _PINNUM(2, 2)
#define PIN_SPI1_MISO         _PINNUM(2, 4)
#define SPI1_SPIM_INSTANCE        NRF_SPIM00
#define SPI1_SPIM_IRQ_HANDLER     SERIAL00_IRQHandler

#define PIN_FLASH_CS          _PINNUM(2, 5)
#define PIN_FLASH_WP          _PINNUM(2, 3)
#define PIN_FLASH_HOLD        _PINNUM(2, 0)

/* ── I2C ──────────────────────────────────────────────────────────────
 * Wire  = 헤더 D4(SDA) / D5(SCL) = P1.03 / P1.07. TWIM22 (Zephyr i2c22).
 * Wire1 = 온보드 IMU = P0.08 / P0.07. P0 는 도메인 30 이라 TWIM30.
 *
 * ⚠ **Wire1 은 PMIC 가 LDO1 을 켜기 전까지 살아 있지 않다.**
 *   IMU 뿐 아니라 그 버스의 4.7K 풀업(R28/R29)도 IMU&MIC_3V3 레일에 붙어 있고,
 *   그 레일이 nPM1300 LDO1 출력이다. 꺼져 있으면 아무 주소도 응답하지 않는다.
 *   Sense 모델이 아니면 IMU 자체가 없다. */
#define PIN_WIRE_SDA          _PINNUM(1, 3)    /* D4 */
#define PIN_WIRE_SCL          _PINNUM(1, 7)    /* D5 */
#define WIRE_TWIM_INSTANCE        NRF_TWIM22
#define WIRE_TWIM_IRQ_HANDLER     SERIAL22_IRQHandler
static const uint8_t SDA = PIN_WIRE_SDA;
static const uint8_t SCL = PIN_WIRE_SCL;

#define PIN_WIRE1_SDA         _PINNUM(0, 8)    /* IMU SDA */
#define PIN_WIRE1_SCL         _PINNUM(0, 7)    /* IMU SCL */
#define WIRE1_TWIM_INSTANCE       NRF_TWIM30
#define WIRE1_TWIM_IRQ_HANDLER    SERIAL30_IRQHandler

/* ── PMIC — nPM1300 ───────────────────────────────────────────────────
 * 충전기 + 레귤레이터다. 이 보드에서
 *   VSYS_3V3 (칩과 LED 전원)   = BUCK2   — VSET2 470K 로 3.3 V 고정
 *   IMU&MIC_3V3 (Sense 센서)   = LDO1
 *   BUCK1                      = 미사용 (VSET1 = GND)
 *
 * I2C 는 P1.17(SCL) / P1.18(SDA) 이고 외부 4.7K 풀업(R12/R13)이 VSYS_3V3 에 있다.
 * Zephyr 는 이 버스를 gpio-i2c(비트뱅잉)로 잡지만 우리는 비트뱅잉을 지원하지
 * 않는다 (CLAUDE.md §7 F6). P1 이라 하드웨어 TWIM 으로 되고, 위 버스들과 겹치지
 * 않는 **TWIM24** 를 쓴다.
 *
 * 코어는 이 버스를 만들지 않는다. 보드 라이브러리(BOARD-XIAO-nRF54LM20A)가 쓴다.
 * ⚠ 레귤레이터 레지스터를 잘못 쓰면 칩 자신의 전원(BUCK2)을 끌 수 있다. */
#define PIN_PMIC_SCL          _PINNUM(1, 17)
#define PIN_PMIC_SDA          _PINNUM(1, 18)
#define PMIC_TWIM_INSTANCE        NRF_TWIM24
#define PMIC_TWIM_IRQ_HANDLER     SERIAL24_IRQHandler
#define PMIC_I2C_ADDRESS      (0x6B)
#define PIN_PMIC_GPIO0        _PINNUM(1, 25)
#define PIN_PMIC_GPIO1        _PINNUM(1, 26)

/* ── Sense — IMU / 마이크 ─────────────────────────────────────────────
 * IMU LSM6DS3TR-C (0x6A), 마이크 MSM261DGT006 (PDM). 전원은 위 LDO1 이다.
 *
 * IMU CS(P3.12)는 100K 로 IMU 레일에 풀업돼 I2C 모드로 고정된다.
 * ⚠ **P3.12 를 출력으로 몰지 마라.** 레일이 꺼진 상태에서 HIGH 를 주면 IMU 가
 *   CS 핀의 보호 다이오드를 통해 역급전된다. */
#define PIN_IMU_SDA           PIN_WIRE1_SDA
#define PIN_IMU_SCL           PIN_WIRE1_SCL
#define PIN_IMU_INT1          _PINNUM(0, 6)
#define PIN_IMU_CS            _PINNUM(3, 12)
#define IMU_I2C_ADDRESS       (0x6A)

#define PIN_PDM_CLK           _PINNUM(1, 13)
#define PIN_PDM_DATA          _PINNUM(1, 14)

/* ── RF — **외부 안테나가 반드시 필요하다** ──────────────────────────────
 * 이 보드에는 칩 안테나가 없다. 칩의 ANT 출력이 정합 회로(L5·L6·L7)를 지나
 * u.FL(IPEX) 커넥터 ANT2 로만 나간다. XIAO nRF54L15 처럼 온보드 안테나와
 * RF 스위치가 있는 구조가 아니다.
 *
 * 안테나 없이 광고하면 SoftDevice·라디오는 정상으로 돌지만(실측: STATE 가 TX,
 * 37·39 번 채널을 오감) 바로 옆의 Mac 도 **한 번도 수신하지 못했다.**
 * 안테나를 단 뒤에는 RSSI −31 dBm, 연결·MTU 247·NUS 에코까지 됐다.
 * "BLE 가 안 된다" 를 코어 문제로 오진하기 쉬우니 안테나부터 확인하라. */

/* ── NFC ──────────────────────────────────────────────────────────────
 * 0R(R39/R40)을 거쳐 뒷면 N1/N2 패드로 간다. 안테나는 실장돼 있지 않다.
 * NFC 로 쓸지 GPIO 로 쓸지는 UICR 설정이다. */
#define PIN_NFC1              _PINNUM(1, 1)
#define PIN_NFC2              _PINNUM(1, 2)

/*
 * 블록 충돌 확인 (docs/PERIPHERAL-PINMAP.md §0) — 같은 번호는 같은 블록이다:
 *   Serial   UARTE20      SPI   SPIM23      Wire1  TWIM30
 *   Serial1  UARTE21      PMIC  TWIM24      SPI1   SPIM00
 *   Wire     TWIM22
 * 전부 다른 번호라 동시에 쓸 수 있다.
 */

/* ═══════════════════════════════════════════════════════════════════
 * 핀 배정 검증 — **빌드에서 막는다**
 * ═══════════════════════════════════════════════════════════════════
 * nrf54l_pinmap.h 가 NRF54LM20A_XXAA 이면 LM20A 표를 쓴다.
 * 핀은 매크로로 넘긴다 (static const 는 C 에서 상수식이 아니다).
 */
NRF54L_ASSERT_SIG(PIN_SERIAL_TX,  UARTE20_TXD, "Serial(UARTE20) TX");
NRF54L_ASSERT_SIG(PIN_SERIAL_RX,  UARTE20_RXD, "Serial(UARTE20) RX");
NRF54L_ASSERT_SIG(PIN_SERIAL1_TX, UARTE21_TXD, "Serial1(UARTE21) TX");
NRF54L_ASSERT_SIG(PIN_SERIAL1_RX, UARTE21_RXD, "Serial1(UARTE21) RX");

NRF54L_ASSERT_SIG(PIN_SPI_SCK,    SPIM23_SCK,  "SPI(SPIM23) SCK");
NRF54L_ASSERT_SIG(PIN_SPI_MOSI,   SPIM23_SDO,  "SPI(SPIM23) MOSI");
NRF54L_ASSERT_SIG(PIN_SPI_MISO,   SPIM23_SDI,  "SPI(SPIM23) MISO");

NRF54L_ASSERT_SIG(PIN_SPI1_SCK,   SPIM00_SCK,  "SPI1(SPIM00) SCK - on-board flash");
NRF54L_ASSERT_SIG(PIN_SPI1_MOSI,  SPIM00_SDO,  "SPI1(SPIM00) MOSI - on-board flash");
NRF54L_ASSERT_SIG(PIN_SPI1_MISO,  SPIM00_SDI,  "SPI1(SPIM00) MISO - on-board flash");

NRF54L_ASSERT_SIG(PIN_WIRE_SDA,   TWIM22_SDA,  "Wire(TWIM22) SDA");
NRF54L_ASSERT_SIG(PIN_WIRE_SCL,   TWIM22_SCL,  "Wire(TWIM22) SCL");
NRF54L_ASSERT_SIG(PIN_WIRE1_SDA,  TWIM30_SDA,  "Wire1(TWIM30) SDA - on-board IMU");
NRF54L_ASSERT_SIG(PIN_WIRE1_SCL,  TWIM30_SCL,  "Wire1(TWIM30) SCL - on-board IMU");
NRF54L_ASSERT_SIG(PIN_PMIC_SDA,   TWIM24_SDA,  "PMIC(TWIM24) SDA");
NRF54L_ASSERT_SIG(PIN_PMIC_SCL,   TWIM24_SCL,  "PMIC(TWIM24) SCL");

NRF54L_ASSERT_SIG(PIN_PDM_CLK,    PDM20_CLK,   "PDM(PDM20) CLK");
NRF54L_ASSERT_SIG(PIN_PDM_DATA,   PDM20_DIN,   "PDM(PDM20) DATA");

/* AIN 번호까지 본다. LM20A 는 배정이 L15 와 달라 포트 검사로는 못 잡는다. */
NRF54L_ASSERT_SIG(PIN_A0, SAADC_AIN0, "A0");  NRF54L_ASSERT_SIG(PIN_A1, SAADC_AIN1, "A1");
NRF54L_ASSERT_SIG(PIN_A2, SAADC_AIN2, "A2");  NRF54L_ASSERT_SIG(PIN_A3, SAADC_AIN3, "A3");
NRF54L_ASSERT_SIG(PIN_A4, SAADC_AIN4, "A4");  NRF54L_ASSERT_SIG(PIN_A5, SAADC_AIN5, "A5");
NRF54L_ASSERT_SIG(PIN_A6, SAADC_AIN6, "A6");  NRF54L_ASSERT_SIG(PIN_A7, SAADC_AIN7, "A7");

NRF54L_ASSERT_SIG(PIN_NFC1, NFCT_NFC1, "NFC1");
NRF54L_ASSERT_SIG(PIN_NFC2, NFCT_NFC2, "NFC2");

/* 인터럽트: 버튼은 P0(GPIOTE30), LED 는 P1(GPIOTE20). */
NRF54L_ASSERT_SIG(PIN_BUTTON1, GPIOTE30_CHAN_0, "USR_KEY (must be interrupt-capable)");
NRF54L_ASSERT_SIG(PIN_LED1,    GPIOTE20_CHAN_0, "red LED (must be on P1 for PWM and interrupts)");

#endif /* _VARIANT_XIAO_NRF54LM20A_H_ */
