/*
 * nrf54l_pinmap.h — 핀↔신호 제약을 컴파일 타임에 검사한다
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * ⚠ **생성된 파일이다. 손으로 고치지 마라.**
 *   `extras/gen_pinmap.py` 가 Nordic Pin Planner 의 SoC 정의에서 굽는다:
 *   https://github.com/NordicPlayground/PinPlanner
 *
 * 기준: nRF54L15 qfn52-6x6-qgaa — P0, P1, P2, GPIO 35개.
 * L05 / L10 / L15 는 제약이 **완전히 동일**하다 (JSON 을 비교해 확인).
 * LM20A 는 페리페럴이 더 많아 별도다 — M6 에서 낸다.
 *
 * ── 왜 필요한가 ───────────────────────────────────────────────────────
 *
 * `nrf54l_domains.h` 는 **포트**까지만 본다. 그것만으로는 부족하다:
 *
 *   PIN_SPI_SCK = P2.03   → 포트 검사는 통과한다 (P2 가 맞으니까)
 *                         → 그런데 SPIM00.SCK 는 P2.01·P2.06 뿐이라 동작하지 않는다
 *                         → 증상은 "SPI 가 안 된다" 뿐이고 원인이 안 보인다
 *
 * 이 헤더는 그 배정을 **빌드에서 막는다.** 오류 메시지가 쓸 수 있는 핀을 알려 준다.
 *
 * ── 쓰는 법 ───────────────────────────────────────────────────────────
 *
 *   NRF54L_ASSERT_SIG(PIN_SPI_SCK, SPIM00_SCK, "SPI SCK");
 *
 * 신호 이름은 `libraries/PinMap` 의 예제 주석 표에 있는 것 그대로다.
 * `SPIM/SPIS00` 처럼 묶여 있는 것은 `SPIM00` 과 `SPIS00` 둘 다 받는다.
 *
 * ⚠ **핀은 매크로로 넘겨라.** variant 의 `static const uint8_t D6 = ...` 같은
 *   Arduino 관용 별칭은 **C 에서 상수식이 아니라** _Static_assert 에 못 넣는다.
 *   variant.h 는 코어의 .c 들에서도 include 되므로 C 로도 컴파일된다.
 *   `_PINNUM(2, 8)` 이나 `PIN_xxx` 매크로를 써라.
 */
#ifndef _NRF54L_PINMAP_H_
#define _NRF54L_PINMAP_H_

#include "nrf54l_domains.h"

/**
 * 핀이 그 신호로 갈 수 있는지 컴파일 타임에 검사한다.
 *
 * @param pin   variant 의 핀 매크로 (절대 GPIO 번호)
 * @param sig   신호 이름 — 예: SPIM00_SCK, TWIM22_SDA, SAADC_AIN0
 * @param what  오류 메시지에 넣을 설명
 */
#define NRF54L_ASSERT_SIG(pin, sig, what)     NRF54L_STATIC_ASSERT(NRF54L_SIG_##sig(pin), what " : " NRF54L_TXT_##sig)

#define NRF54L_SIG_SPIM00_SCK(p)  ((p) == 65 || (p) == 70)
#define NRF54L_TXT_SPIM00_SCK     "SPIM/SPIS00.SCK 는 P2.01, P2.06 만 된다"
#define NRF54L_SIG_SPIS00_SCK(p)  ((p) == 65 || (p) == 70)
#define NRF54L_TXT_SPIS00_SCK     "SPIM/SPIS00.SCK 는 P2.01, P2.06 만 된다"

#define NRF54L_SIG_SPIM00_SDO(p)  ((p) == 66 || (p) == 72)
#define NRF54L_TXT_SPIM00_SDO     "SPIM/SPIS00.SDO 는 P2.02, P2.08 만 된다"
#define NRF54L_SIG_SPIS00_SDO(p)  ((p) == 66 || (p) == 72)
#define NRF54L_TXT_SPIS00_SDO     "SPIM/SPIS00.SDO 는 P2.02, P2.08 만 된다"

#define NRF54L_SIG_SPIM00_SDI(p)  ((p) == 68 || (p) == 73)
#define NRF54L_TXT_SPIM00_SDI     "SPIM/SPIS00.SDI 는 P2.04, P2.09 만 된다"
#define NRF54L_SIG_SPIS00_SDI(p)  ((p) == 68 || (p) == 73)
#define NRF54L_TXT_SPIS00_SDI     "SPIM/SPIS00.SDI 는 P2.04, P2.09 만 된다"

#define NRF54L_SIG_SPIM00_CS(p)  ((p) == 69 || (p) == 74)
#define NRF54L_TXT_SPIM00_CS     "SPIM/SPIS00.CS 는 P2.05, P2.10 만 된다"
#define NRF54L_SIG_SPIS00_CS(p)  ((p) == 69 || (p) == 74)
#define NRF54L_TXT_SPIS00_CS     "SPIM/SPIS00.CS 는 P2.05, P2.10 만 된다"

#define NRF54L_SIG_SPIM00_DCX(p)  ((p) == 64 || (p) == 71)
#define NRF54L_TXT_SPIM00_DCX     "SPIM/SPIS00.DCX 는 P2.00, P2.07 만 된다"
#define NRF54L_SIG_SPIS00_DCX(p)  ((p) == 64 || (p) == 71)
#define NRF54L_TXT_SPIS00_DCX     "SPIM/SPIS00.DCX 는 P2.00, P2.07 만 된다"

#define NRF54L_SIG_UARTE00_TXD(p)  ((p) == 66 || (p) == 72)
#define NRF54L_TXT_UARTE00_TXD     "UARTE00.TXD 는 P2.02, P2.08 만 된다"

#define NRF54L_SIG_UARTE00_RXD(p)  ((p) == 64 || (p) == 71)
#define NRF54L_TXT_UARTE00_RXD     "UARTE00.RXD 는 P2.00, P2.07 만 된다"

#define NRF54L_SIG_UARTE00_CTS(p)  ((p) == 68 || (p) == 73)
#define NRF54L_TXT_UARTE00_CTS     "UARTE00.CTS 는 P2.04, P2.09 만 된다"

#define NRF54L_SIG_UARTE00_RTS(p)  ((p) == 69 || (p) == 74)
#define NRF54L_TXT_UARTE00_RTS     "UARTE00.RTS 는 P2.05, P2.10 만 된다"

#define NRF54L_SIG_RADIO_RADIO_0(p)  ((p) == 41)
#define NRF54L_TXT_RADIO_RADIO_0     "RADIO.RADIO[0] 는 P1.09 만 된다"

#define NRF54L_SIG_RADIO_RADIO_1(p)  ((p) == 42)
#define NRF54L_TXT_RADIO_RADIO_1     "RADIO.RADIO[1] 는 P1.10 만 된다"

#define NRF54L_SIG_RADIO_RADIO_2(p)  ((p) == 43)
#define NRF54L_TXT_RADIO_RADIO_2     "RADIO.RADIO[2] 는 P1.11 만 된다"

#define NRF54L_SIG_RADIO_RADIO_3(p)  ((p) == 44)
#define NRF54L_TXT_RADIO_RADIO_3     "RADIO.RADIO[3] 는 P1.12 만 된다"

#define NRF54L_SIG_RADIO_RADIO_4(p)  ((p) == 45)
#define NRF54L_TXT_RADIO_RADIO_4     "RADIO.RADIO[4] 는 P1.13 만 된다"

#define NRF54L_SIG_RADIO_RADIO_5(p)  ((p) == 46)
#define NRF54L_TXT_RADIO_RADIO_5     "RADIO.RADIO[5] 는 P1.14 만 된다"

#define NRF54L_SIG_RADIO_RADIO_6(p)  ((p) == 37)
#define NRF54L_TXT_RADIO_RADIO_6     "RADIO.RADIO[6] 는 P1.05 만 된다"

#define NRF54L_SIG_SPIM20_SCK(p)  (NRF54L_PORT_OF(p) == 1 || (p) == 65)
#define NRF54L_TXT_SPIM20_SCK     "SPIM/SPIS20.SCK 는 P1 의 아무 핀, P2.01 만 된다"
#define NRF54L_SIG_SPIS20_SCK(p)  (NRF54L_PORT_OF(p) == 1 || (p) == 65)
#define NRF54L_TXT_SPIS20_SCK     "SPIM/SPIS20.SCK 는 P1 의 아무 핀, P2.01 만 된다"

#define NRF54L_SIG_SPIM20_SDO(p)  (NRF54L_PORT_OF(p) == 1 || (p) == 66)
#define NRF54L_TXT_SPIM20_SDO     "SPIM/SPIS20.SDO 는 P1 의 아무 핀, P2.02 만 된다"
#define NRF54L_SIG_SPIS20_SDO(p)  (NRF54L_PORT_OF(p) == 1 || (p) == 66)
#define NRF54L_TXT_SPIS20_SDO     "SPIM/SPIS20.SDO 는 P1 의 아무 핀, P2.02 만 된다"

#define NRF54L_SIG_SPIM20_SDI(p)  (NRF54L_PORT_OF(p) == 1 || (p) == 68)
#define NRF54L_TXT_SPIM20_SDI     "SPIM/SPIS20.SDI 는 P1 의 아무 핀, P2.04 만 된다"
#define NRF54L_SIG_SPIS20_SDI(p)  (NRF54L_PORT_OF(p) == 1 || (p) == 68)
#define NRF54L_TXT_SPIS20_SDI     "SPIM/SPIS20.SDI 는 P1 의 아무 핀, P2.04 만 된다"

#define NRF54L_SIG_SPIM20_CS(p)  (NRF54L_PORT_OF(p) == 1 || (p) == 69)
#define NRF54L_TXT_SPIM20_CS     "SPIM/SPIS20.CS 는 P1 의 아무 핀, P2.05 만 된다"
#define NRF54L_SIG_SPIS20_CS(p)  (NRF54L_PORT_OF(p) == 1 || (p) == 69)
#define NRF54L_TXT_SPIS20_CS     "SPIM/SPIS20.CS 는 P1 의 아무 핀, P2.05 만 된다"

#define NRF54L_SIG_SPIM20_DCX(p)  (NRF54L_PORT_OF(p) == 1 || (p) == 64)
#define NRF54L_TXT_SPIM20_DCX     "SPIM/SPIS20.DCX 는 P1 의 아무 핀, P2.00 만 된다"
#define NRF54L_SIG_SPIS20_DCX(p)  (NRF54L_PORT_OF(p) == 1 || (p) == 64)
#define NRF54L_TXT_SPIS20_DCX     "SPIM/SPIS20.DCX 는 P1 의 아무 핀, P2.00 만 된다"

#define NRF54L_SIG_TWIM20_SCL(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_TWIM20_SCL     "TWIM/TWIS20.SCL 는 P1 의 아무 핀 만 된다"
#define NRF54L_SIG_TWIS20_SCL(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_TWIS20_SCL     "TWIM/TWIS20.SCL 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_TWIM20_SDA(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_TWIM20_SDA     "TWIM/TWIS20.SDA 는 P1 의 아무 핀 만 된다"
#define NRF54L_SIG_TWIS20_SDA(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_TWIS20_SDA     "TWIM/TWIS20.SDA 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_UARTE20_TXD(p)  (NRF54L_PORT_OF(p) == 1 || (p) == 66)
#define NRF54L_TXT_UARTE20_TXD     "UARTE20.TXD 는 P1 의 아무 핀, P2.02 만 된다"

#define NRF54L_SIG_UARTE20_RXD(p)  (NRF54L_PORT_OF(p) == 1 || (p) == 64)
#define NRF54L_TXT_UARTE20_RXD     "UARTE20.RXD 는 P1 의 아무 핀, P2.00 만 된다"

#define NRF54L_SIG_UARTE20_CTS(p)  (NRF54L_PORT_OF(p) == 1 || (p) == 68)
#define NRF54L_TXT_UARTE20_CTS     "UARTE20.CTS 는 P1 의 아무 핀, P2.04 만 된다"

#define NRF54L_SIG_UARTE20_RTS(p)  (NRF54L_PORT_OF(p) == 1 || (p) == 69)
#define NRF54L_TXT_UARTE20_RTS     "UARTE20.RTS 는 P1 의 아무 핀, P2.05 만 된다"

#define NRF54L_SIG_SPIM21_SCK(p)  (NRF54L_PORT_OF(p) == 1 || (p) == 70)
#define NRF54L_TXT_SPIM21_SCK     "SPIM/SPIS21.SCK 는 P1 의 아무 핀, P2.06 만 된다"
#define NRF54L_SIG_SPIS21_SCK(p)  (NRF54L_PORT_OF(p) == 1 || (p) == 70)
#define NRF54L_TXT_SPIS21_SCK     "SPIM/SPIS21.SCK 는 P1 의 아무 핀, P2.06 만 된다"

#define NRF54L_SIG_SPIM21_SDO(p)  (NRF54L_PORT_OF(p) == 1 || (p) == 72)
#define NRF54L_TXT_SPIM21_SDO     "SPIM/SPIS21.SDO 는 P1 의 아무 핀, P2.08 만 된다"
#define NRF54L_SIG_SPIS21_SDO(p)  (NRF54L_PORT_OF(p) == 1 || (p) == 72)
#define NRF54L_TXT_SPIS21_SDO     "SPIM/SPIS21.SDO 는 P1 의 아무 핀, P2.08 만 된다"

#define NRF54L_SIG_SPIM21_SDI(p)  (NRF54L_PORT_OF(p) == 1 || (p) == 73)
#define NRF54L_TXT_SPIM21_SDI     "SPIM/SPIS21.SDI 는 P1 의 아무 핀, P2.09 만 된다"
#define NRF54L_SIG_SPIS21_SDI(p)  (NRF54L_PORT_OF(p) == 1 || (p) == 73)
#define NRF54L_TXT_SPIS21_SDI     "SPIM/SPIS21.SDI 는 P1 의 아무 핀, P2.09 만 된다"

#define NRF54L_SIG_SPIM21_CS(p)  (NRF54L_PORT_OF(p) == 1 || (p) == 74)
#define NRF54L_TXT_SPIM21_CS     "SPIM/SPIS21.CS 는 P1 의 아무 핀, P2.10 만 된다"
#define NRF54L_SIG_SPIS21_CS(p)  (NRF54L_PORT_OF(p) == 1 || (p) == 74)
#define NRF54L_TXT_SPIS21_CS     "SPIM/SPIS21.CS 는 P1 의 아무 핀, P2.10 만 된다"

#define NRF54L_SIG_SPIM21_DCX(p)  (NRF54L_PORT_OF(p) == 1 || (p) == 71)
#define NRF54L_TXT_SPIM21_DCX     "SPIM/SPIS21.DCX 는 P1 의 아무 핀, P2.07 만 된다"
#define NRF54L_SIG_SPIS21_DCX(p)  (NRF54L_PORT_OF(p) == 1 || (p) == 71)
#define NRF54L_TXT_SPIS21_DCX     "SPIM/SPIS21.DCX 는 P1 의 아무 핀, P2.07 만 된다"

#define NRF54L_SIG_TWIM21_SCL(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_TWIM21_SCL     "TWIM/TWIS21.SCL 는 P1 의 아무 핀 만 된다"
#define NRF54L_SIG_TWIS21_SCL(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_TWIS21_SCL     "TWIM/TWIS21.SCL 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_TWIM21_SDA(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_TWIM21_SDA     "TWIM/TWIS21.SDA 는 P1 의 아무 핀 만 된다"
#define NRF54L_SIG_TWIS21_SDA(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_TWIS21_SDA     "TWIM/TWIS21.SDA 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_UARTE21_TXD(p)  (NRF54L_PORT_OF(p) == 1 || (p) == 72)
#define NRF54L_TXT_UARTE21_TXD     "UARTE21.TXD 는 P1 의 아무 핀, P2.08 만 된다"

#define NRF54L_SIG_UARTE21_RXD(p)  (NRF54L_PORT_OF(p) == 1 || (p) == 71)
#define NRF54L_TXT_UARTE21_RXD     "UARTE21.RXD 는 P1 의 아무 핀, P2.07 만 된다"

#define NRF54L_SIG_UARTE21_CTS(p)  (NRF54L_PORT_OF(p) == 1 || (p) == 73)
#define NRF54L_TXT_UARTE21_CTS     "UARTE21.CTS 는 P1 의 아무 핀, P2.09 만 된다"

#define NRF54L_SIG_UARTE21_RTS(p)  (NRF54L_PORT_OF(p) == 1 || (p) == 74)
#define NRF54L_TXT_UARTE21_RTS     "UARTE21.RTS 는 P1 의 아무 핀, P2.10 만 된다"

#define NRF54L_SIG_SPIM22_SCK(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_SPIM22_SCK     "SPIM/SPIS22.SCK 는 P1 의 아무 핀 만 된다"
#define NRF54L_SIG_SPIS22_SCK(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_SPIS22_SCK     "SPIM/SPIS22.SCK 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_SPIM22_SDO(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_SPIM22_SDO     "SPIM/SPIS22.SDO 는 P1 의 아무 핀 만 된다"
#define NRF54L_SIG_SPIS22_SDO(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_SPIS22_SDO     "SPIM/SPIS22.SDO 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_SPIM22_SDI(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_SPIM22_SDI     "SPIM/SPIS22.SDI 는 P1 의 아무 핀 만 된다"
#define NRF54L_SIG_SPIS22_SDI(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_SPIS22_SDI     "SPIM/SPIS22.SDI 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_SPIM22_CS(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_SPIM22_CS     "SPIM/SPIS22.CS 는 P1 의 아무 핀 만 된다"
#define NRF54L_SIG_SPIS22_CS(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_SPIS22_CS     "SPIM/SPIS22.CS 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_SPIM22_DCX(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_SPIM22_DCX     "SPIM/SPIS22.DCX 는 P1 의 아무 핀 만 된다"
#define NRF54L_SIG_SPIS22_DCX(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_SPIS22_DCX     "SPIM/SPIS22.DCX 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_TWIM22_SCL(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_TWIM22_SCL     "TWIM/TWIS22.SCL 는 P1 의 아무 핀 만 된다"
#define NRF54L_SIG_TWIS22_SCL(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_TWIS22_SCL     "TWIM/TWIS22.SCL 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_TWIM22_SDA(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_TWIM22_SDA     "TWIM/TWIS22.SDA 는 P1 의 아무 핀 만 된다"
#define NRF54L_SIG_TWIS22_SDA(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_TWIS22_SDA     "TWIM/TWIS22.SDA 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_UARTE22_TXD(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_UARTE22_TXD     "UARTE22.TXD 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_UARTE22_RXD(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_UARTE22_RXD     "UARTE22.RXD 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_UARTE22_CTS(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_UARTE22_CTS     "UARTE22.CTS 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_UARTE22_RTS(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_UARTE22_RTS     "UARTE22.RTS 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_PDM20_CLK(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_PDM20_CLK     "PDM20.CLK 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_PDM20_DIN(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_PDM20_DIN     "PDM20.DIN 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_PDM21_CLK(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_PDM21_CLK     "PDM21.CLK 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_PDM21_DIN(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_PDM21_DIN     "PDM21.DIN 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_PWM20_CHAN_0(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_PWM20_CHAN_0     "PWM20.CHAN[0] 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_PWM20_CHAN_1(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_PWM20_CHAN_1     "PWM20.CHAN[1] 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_PWM20_CHAN_2(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_PWM20_CHAN_2     "PWM20.CHAN[2] 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_PWM20_CHAN_3(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_PWM20_CHAN_3     "PWM20.CHAN[3] 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_PWM21_CHAN_0(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_PWM21_CHAN_0     "PWM21.CHAN[0] 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_PWM21_CHAN_1(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_PWM21_CHAN_1     "PWM21.CHAN[1] 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_PWM21_CHAN_2(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_PWM21_CHAN_2     "PWM21.CHAN[2] 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_PWM21_CHAN_3(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_PWM21_CHAN_3     "PWM21.CHAN[3] 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_PWM22_CHAN_0(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_PWM22_CHAN_0     "PWM22.CHAN[0] 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_PWM22_CHAN_1(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_PWM22_CHAN_1     "PWM22.CHAN[1] 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_PWM22_CHAN_2(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_PWM22_CHAN_2     "PWM22.CHAN[2] 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_PWM22_CHAN_3(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_PWM22_CHAN_3     "PWM22.CHAN[3] 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_SAADC_AIN0(p)  ((p) == 36)
#define NRF54L_TXT_SAADC_AIN0     "SAADC.AIN0 는 P1.04 만 된다"

#define NRF54L_SIG_SAADC_AIN1(p)  ((p) == 37)
#define NRF54L_TXT_SAADC_AIN1     "SAADC.AIN1 는 P1.05 만 된다"

#define NRF54L_SIG_SAADC_AIN2(p)  ((p) == 38)
#define NRF54L_TXT_SAADC_AIN2     "SAADC.AIN2 는 P1.06 만 된다"

#define NRF54L_SIG_SAADC_AIN3(p)  ((p) == 39)
#define NRF54L_TXT_SAADC_AIN3     "SAADC.AIN3 는 P1.07 만 된다"

#define NRF54L_SIG_SAADC_AIN4(p)  ((p) == 43)
#define NRF54L_TXT_SAADC_AIN4     "SAADC.AIN4 는 P1.11 만 된다"

#define NRF54L_SIG_SAADC_AIN5(p)  ((p) == 44)
#define NRF54L_TXT_SAADC_AIN5     "SAADC.AIN5 는 P1.12 만 된다"

#define NRF54L_SIG_SAADC_AIN6(p)  ((p) == 45)
#define NRF54L_TXT_SAADC_AIN6     "SAADC.AIN6 는 P1.13 만 된다"

#define NRF54L_SIG_SAADC_AIN7(p)  ((p) == 46)
#define NRF54L_TXT_SAADC_AIN7     "SAADC.AIN7 는 P1.14 만 된다"

#define NRF54L_SIG_SAADC_EXTREF(p)  ((p) == 40)
#define NRF54L_TXT_SAADC_EXTREF     "SAADC.EXTREF 는 P1.08 만 된다"

#define NRF54L_SIG_NFCT_NFC1(p)  ((p) == 34)
#define NRF54L_TXT_NFCT_NFC1     "NFCT.NFC1 는 P1.02 만 된다"

#define NRF54L_SIG_NFCT_NFC2(p)  ((p) == 35)
#define NRF54L_TXT_NFCT_NFC2     "NFCT.NFC2 는 P1.03 만 된다"

#define NRF54L_SIG_GPIOTE20_CHAN_0(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_GPIOTE20_CHAN_0     "GPIOTE20.CHAN[0] 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_GPIOTE20_CHAN_1(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_GPIOTE20_CHAN_1     "GPIOTE20.CHAN[1] 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_GPIOTE20_CHAN_2(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_GPIOTE20_CHAN_2     "GPIOTE20.CHAN[2] 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_GPIOTE20_CHAN_3(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_GPIOTE20_CHAN_3     "GPIOTE20.CHAN[3] 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_GPIOTE20_CHAN_4(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_GPIOTE20_CHAN_4     "GPIOTE20.CHAN[4] 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_GPIOTE20_CHAN_5(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_GPIOTE20_CHAN_5     "GPIOTE20.CHAN[5] 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_GPIOTE20_CHAN_6(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_GPIOTE20_CHAN_6     "GPIOTE20.CHAN[6] 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_GPIOTE20_CHAN_7(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_GPIOTE20_CHAN_7     "GPIOTE20.CHAN[7] 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_TAMPC_ASO_0(p)  ((p) == 36)
#define NRF54L_TXT_TAMPC_ASO_0     "TAMPC.ASO[0] 는 P1.04 만 된다"

#define NRF54L_SIG_TAMPC_ASI_0(p)  ((p) == 37)
#define NRF54L_TXT_TAMPC_ASI_0     "TAMPC.ASI[0] 는 P1.05 만 된다"

#define NRF54L_SIG_TAMPC_ASO_1(p)  ((p) == 38)
#define NRF54L_TXT_TAMPC_ASO_1     "TAMPC.ASO[1] 는 P1.06 만 된다"

#define NRF54L_SIG_TAMPC_ASI_1(p)  ((p) == 39)
#define NRF54L_TXT_TAMPC_ASI_1     "TAMPC.ASI[1] 는 P1.07 만 된다"

#define NRF54L_SIG_TAMPC_ASO_2(p)  ((p) == 41)
#define NRF54L_TXT_TAMPC_ASO_2     "TAMPC.ASO[2] 는 P1.09 만 된다"

#define NRF54L_SIG_TAMPC_ASI_2(p)  ((p) == 42)
#define NRF54L_TXT_TAMPC_ASI_2     "TAMPC.ASI[2] 는 P1.10 만 된다"

#define NRF54L_SIG_TAMPC_ASO_3(p)  ((p) == 43)
#define NRF54L_TXT_TAMPC_ASO_3     "TAMPC.ASO[3] 는 P1.11 만 된다"

#define NRF54L_SIG_TAMPC_ASI_3(p)  ((p) == 44)
#define NRF54L_TXT_TAMPC_ASI_3     "TAMPC.ASI[3] 는 P1.12 만 된다"

#define NRF54L_SIG_I2S20_SCK(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_I2S20_SCK     "I2S20.SCK 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_I2S20_LRCK(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_I2S20_LRCK     "I2S20.LRCK 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_I2S20_SDIN(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_I2S20_SDIN     "I2S20.SDIN 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_I2S20_SDOUT(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_I2S20_SDOUT     "I2S20.SDOUT 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_I2S20_MCK(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_I2S20_MCK     "I2S20.MCK 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_QDEC20_A(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_QDEC20_A     "QDEC20.A 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_QDEC20_B(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_QDEC20_B     "QDEC20.B 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_QDEC20_LED(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_QDEC20_LED     "QDEC20.LED 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_QDEC21_A(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_QDEC21_A     "QDEC21.A 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_QDEC21_B(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_QDEC21_B     "QDEC21.B 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_QDEC21_LED(p)  (NRF54L_PORT_OF(p) == 1)
#define NRF54L_TXT_QDEC21_LED     "QDEC21.LED 는 P1 의 아무 핀 만 된다"

#define NRF54L_SIG_GRTC_PWM(p)  ((p) == 3)
#define NRF54L_TXT_GRTC_PWM     "GRTC.PWM 는 P0.03 만 된다"

#define NRF54L_SIG_GRTC_LFCLKOUT(p)  ((p) == 4)
#define NRF54L_TXT_GRTC_LFCLKOUT     "GRTC.LFCLKOUT 는 P0.04 만 된다"

#define NRF54L_SIG_GRTC_CLK16M(p)  ((p) == 40)
#define NRF54L_TXT_GRTC_CLK16M     "GRTC.CLK16M 는 P1.08 만 된다"

#define NRF54L_SIG_SPIM30_SCK(p)  (NRF54L_PORT_OF(p) == 0)
#define NRF54L_TXT_SPIM30_SCK     "SPIM/SPIS30.SCK 는 P0 의 아무 핀 만 된다"
#define NRF54L_SIG_SPIS30_SCK(p)  (NRF54L_PORT_OF(p) == 0)
#define NRF54L_TXT_SPIS30_SCK     "SPIM/SPIS30.SCK 는 P0 의 아무 핀 만 된다"

#define NRF54L_SIG_SPIM30_SDO(p)  (NRF54L_PORT_OF(p) == 0)
#define NRF54L_TXT_SPIM30_SDO     "SPIM/SPIS30.SDO 는 P0 의 아무 핀 만 된다"
#define NRF54L_SIG_SPIS30_SDO(p)  (NRF54L_PORT_OF(p) == 0)
#define NRF54L_TXT_SPIS30_SDO     "SPIM/SPIS30.SDO 는 P0 의 아무 핀 만 된다"

#define NRF54L_SIG_SPIM30_SDI(p)  (NRF54L_PORT_OF(p) == 0)
#define NRF54L_TXT_SPIM30_SDI     "SPIM/SPIS30.SDI 는 P0 의 아무 핀 만 된다"
#define NRF54L_SIG_SPIS30_SDI(p)  (NRF54L_PORT_OF(p) == 0)
#define NRF54L_TXT_SPIS30_SDI     "SPIM/SPIS30.SDI 는 P0 의 아무 핀 만 된다"

#define NRF54L_SIG_SPIM30_CS(p)  (NRF54L_PORT_OF(p) == 0)
#define NRF54L_TXT_SPIM30_CS     "SPIM/SPIS30.CS 는 P0 의 아무 핀 만 된다"
#define NRF54L_SIG_SPIS30_CS(p)  (NRF54L_PORT_OF(p) == 0)
#define NRF54L_TXT_SPIS30_CS     "SPIM/SPIS30.CS 는 P0 의 아무 핀 만 된다"

#define NRF54L_SIG_SPIM30_DCX(p)  (NRF54L_PORT_OF(p) == 0)
#define NRF54L_TXT_SPIM30_DCX     "SPIM/SPIS30.DCX 는 P0 의 아무 핀 만 된다"
#define NRF54L_SIG_SPIS30_DCX(p)  (NRF54L_PORT_OF(p) == 0)
#define NRF54L_TXT_SPIS30_DCX     "SPIM/SPIS30.DCX 는 P0 의 아무 핀 만 된다"

#define NRF54L_SIG_TWIM30_SCL(p)  (NRF54L_PORT_OF(p) == 0)
#define NRF54L_TXT_TWIM30_SCL     "TWIM/TWIS30.SCL 는 P0 의 아무 핀 만 된다"
#define NRF54L_SIG_TWIS30_SCL(p)  (NRF54L_PORT_OF(p) == 0)
#define NRF54L_TXT_TWIS30_SCL     "TWIM/TWIS30.SCL 는 P0 의 아무 핀 만 된다"

#define NRF54L_SIG_TWIM30_SDA(p)  (NRF54L_PORT_OF(p) == 0)
#define NRF54L_TXT_TWIM30_SDA     "TWIM/TWIS30.SDA 는 P0 의 아무 핀 만 된다"
#define NRF54L_SIG_TWIS30_SDA(p)  (NRF54L_PORT_OF(p) == 0)
#define NRF54L_TXT_TWIS30_SDA     "TWIM/TWIS30.SDA 는 P0 의 아무 핀 만 된다"

#define NRF54L_SIG_UARTE30_TXD(p)  (NRF54L_PORT_OF(p) == 0)
#define NRF54L_TXT_UARTE30_TXD     "UARTE30.TXD 는 P0 의 아무 핀 만 된다"

#define NRF54L_SIG_UARTE30_RXD(p)  (NRF54L_PORT_OF(p) == 0)
#define NRF54L_TXT_UARTE30_RXD     "UARTE30.RXD 는 P0 의 아무 핀 만 된다"

#define NRF54L_SIG_UARTE30_CTS(p)  (NRF54L_PORT_OF(p) == 0)
#define NRF54L_TXT_UARTE30_CTS     "UARTE30.CTS 는 P0 의 아무 핀 만 된다"

#define NRF54L_SIG_UARTE30_RTS(p)  (NRF54L_PORT_OF(p) == 0)
#define NRF54L_TXT_UARTE30_RTS     "UARTE30.RTS 는 P0 의 아무 핀 만 된다"

#define NRF54L_SIG_GPIOTE30_CHAN_0(p)  (NRF54L_PORT_OF(p) == 0)
#define NRF54L_TXT_GPIOTE30_CHAN_0     "GPIOTE30.CHAN[0] 는 P0 의 아무 핀 만 된다"

#define NRF54L_SIG_GPIOTE30_CHAN_1(p)  (NRF54L_PORT_OF(p) == 0)
#define NRF54L_TXT_GPIOTE30_CHAN_1     "GPIOTE30.CHAN[1] 는 P0 의 아무 핀 만 된다"

#define NRF54L_SIG_GPIOTE30_CHAN_2(p)  (NRF54L_PORT_OF(p) == 0)
#define NRF54L_TXT_GPIOTE30_CHAN_2     "GPIOTE30.CHAN[2] 는 P0 의 아무 핀 만 된다"

#define NRF54L_SIG_GPIOTE30_CHAN_3(p)  (NRF54L_PORT_OF(p) == 0)
#define NRF54L_TXT_GPIOTE30_CHAN_3     "GPIOTE30.CHAN[3] 는 P0 의 아무 핀 만 된다"

#define NRF54L_SIG_LFXO_XL1(p)  ((p) == 32)
#define NRF54L_TXT_LFXO_XL1     "LFXO.XL1 는 P1.00 만 된다"

#define NRF54L_SIG_LFXO_XL2(p)  ((p) == 33)
#define NRF54L_TXT_LFXO_XL2     "LFXO.XL2 는 P1.01 만 된다"

#define NRF54L_SIG_sQSPI_SCK(p)  ((p) == 65)
#define NRF54L_TXT_sQSPI_SCK     "sQSPI.SCK 는 P2.01 만 된다"

#define NRF54L_SIG_sQSPI_CSN(p)  ((p) == 69)
#define NRF54L_TXT_sQSPI_CSN     "sQSPI.CSN 는 P2.05 만 된다"

#define NRF54L_SIG_sQSPI_D0(p)  ((p) == 66)
#define NRF54L_TXT_sQSPI_D0     "sQSPI.D0 는 P2.02 만 된다"

#define NRF54L_SIG_sQSPI_D1(p)  ((p) == 68)
#define NRF54L_TXT_sQSPI_D1     "sQSPI.D1 는 P2.04 만 된다"

#define NRF54L_SIG_sQSPI_D2(p)  ((p) == 67)
#define NRF54L_TXT_sQSPI_D2     "sQSPI.D2 는 P2.03 만 된다"

#define NRF54L_SIG_sQSPI_D3(p)  ((p) == 64)
#define NRF54L_TXT_sQSPI_D3     "sQSPI.D3 는 P2.00 만 된다"

#endif /* _NRF54L_PINMAP_H_ */
