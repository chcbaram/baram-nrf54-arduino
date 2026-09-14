/*
 * variant.cpp — Seeed Studio XIAO nRF54LM20A / XIAO nRF54LM20A Sense
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * 핀 매핑과 부팅 시 보드 초기화. 회로도 근거는 docs/boards/XIAO-nRF54LM20A.md.
 */

#include "Arduino.h"
#include "variant.h"

/*
 * Arduino 핀 번호 -> 절대 GPIO 번호.
 *
 * 항등 매핑이다. 그래야 스케치의 _PINNUM(port, pin) 이 그대로 통한다.
 * 패키지(FCCSP98)에 없는 핀과 GPIO 로 쓰면 안 되는 핀(LFXO)만 NC 로 막는다.
 * NC 인 핀에 pinMode/digitalWrite 를 해도 조용히 무시된다.
 *
 * nRF54LM20A FCCSP98 의 GPIO: P0.00~09, P1.00~31, P2.00~10, P3.00~12 (66개).
 * "미배선" 은 칩에는 있지만 보드에서 어디에도 연결되지 않은 핀이다.
 */
const uint32_t g_ADigitalPinMap[PINS_COUNT] =
{
  0,              //   0  P0.00  D19
  1,              //   1  P0.01  D20
  2,              //   2  P0.02  D21
  3,              //   3  P0.03  D22
  4,              //   4  P0.04  D23
  5,              //   5  P0.05  D24
  6,              //   6  P0.06  IMU INT1
  7,              //   7  P0.07  IMU SCL  (Wire1, TWIM30)
  8,              //   8  P0.08  IMU SDA  (Wire1, TWIM30)
  9,              //   9  P0.09  USR_KEY (K2, 외부 100K 풀업)
  NRF54L_PIN_NC,   //  10  P0.10 패키지에 없음
  NRF54L_PIN_NC,   //  11  P0.11 패키지에 없음
  NRF54L_PIN_NC,   //  12  P0.12 패키지에 없음
  NRF54L_PIN_NC,   //  13  P0.13 패키지에 없음
  NRF54L_PIN_NC,   //  14  P0.14 패키지에 없음
  NRF54L_PIN_NC,   //  15  P0.15 패키지에 없음
  NRF54L_PIN_NC,   //  16  P0.16 패키지에 없음
  NRF54L_PIN_NC,   //  17  P0.17 패키지에 없음
  NRF54L_PIN_NC,   //  18  P0.18 패키지에 없음
  NRF54L_PIN_NC,   //  19  P0.19 패키지에 없음
  NRF54L_PIN_NC,   //  20  P0.20 패키지에 없음
  NRF54L_PIN_NC,   //  21  P0.21 패키지에 없음
  NRF54L_PIN_NC,   //  22  P0.22 패키지에 없음
  NRF54L_PIN_NC,   //  23  P0.23 패키지에 없음
  NRF54L_PIN_NC,   //  24  P0.24 패키지에 없음
  NRF54L_PIN_NC,   //  25  P0.25 패키지에 없음
  NRF54L_PIN_NC,   //  26  P0.26 패키지에 없음
  NRF54L_PIN_NC,   //  27  P0.27 패키지에 없음
  NRF54L_PIN_NC,   //  28  P0.28 패키지에 없음
  NRF54L_PIN_NC,   //  29  P0.29 패키지에 없음
  NRF54L_PIN_NC,   //  30  P0.30 패키지에 없음
  NRF54L_PIN_NC,   //  31  P0.31 패키지에 없음
  32,             //  32  P1.00  D0 / A0 / AIN0
  33,             //  33  P1.01  NFC1
  34,             //  34  P1.02  NFC2
  35,             //  35  P1.03  D4 / SDA (Wire, TWIM22) / A7
  36,             //  36  P1.04  D8 / SCK (SPIM23) / A6
  37,             //  37  P1.05  D9 / MISO (SPIM23) / A5
  38,             //  38  P1.06  D10 / MOSI (SPIM23) / A4
  39,             //  39  P1.07  D5 / SCL (Wire, TWIM22) — EXTREF, AIN 아님
  40,             //  40  P1.08  D6 / Serial1 TX (UARTE21)
  41,             //  41  P1.09  D7 / Serial1 RX (UARTE21)
  42,             //  42  P1.10  Serial RX  <- SAMD11 (UARTE20)
  43,             //  43  P1.11  Serial TX  -> SAMD11 (UARTE20)
  44,             //  44  P1.12  미배선
  45,             //  45  P1.13  MIC CLK  (PDM20)
  46,             //  46  P1.14  MIC DATA (PDM20)
  47,             //  47  P1.15  미배선
  48,             //  48  P1.16  미배선
  49,             //  49  P1.17  PMIC SCL (TWIM24)
  50,             //  50  P1.18  PMIC SDA (TWIM24)
  51,             //  51  P1.19  미배선
  NRF54L_PIN_NC,   //  52  P1.20  XL1 (LFXO) — GPIO 사용 금지
  NRF54L_PIN_NC,   //  53  P1.21  XL2 (LFXO) — GPIO 사용 금지
  54,             //  54  P1.22  LED 빨강 (active LOW)
  55,             //  55  P1.23  LED 파랑 (active LOW)
  56,             //  56  P1.24  LED 초록 (active LOW)
  57,             //  57  P1.25  nPM1300 GPIO0
  58,             //  58  P1.26  nPM1300 GPIO1
  59,             //  59  P1.27  미배선
  60,             //  60  P1.28  미배선
  61,             //  61  P1.29  D3 / A3 / AIN3
  62,             //  62  P1.30  D2 / A2 / AIN2
  63,             //  63  P1.31  D1 / A1 / AIN1
  64,             //  64  P2.00  플래시 IO3 / HOLD
  65,             //  65  P2.01  플래시 CLK (SPI1, SPIM00)
  66,             //  66  P2.02  플래시 IO0 / MOSI
  67,             //  67  P2.03  플래시 IO2 / WP
  68,             //  68  P2.04  플래시 IO1 / MISO
  69,             //  69  P2.05  플래시 CS
  70,             //  70  P2.06  미배선
  71,             //  71  P2.07  미배선
  72,             //  72  P2.08  미배선
  73,             //  73  P2.09  미배선
  74,             //  74  P2.10  미배선
  NRF54L_PIN_NC,   //  75  P2.11 패키지에 없음
  NRF54L_PIN_NC,   //  76  P2.12 패키지에 없음
  NRF54L_PIN_NC,   //  77  P2.13 패키지에 없음
  NRF54L_PIN_NC,   //  78  P2.14 패키지에 없음
  NRF54L_PIN_NC,   //  79  P2.15 패키지에 없음
  NRF54L_PIN_NC,   //  80  P2.16 패키지에 없음
  NRF54L_PIN_NC,   //  81  P2.17 패키지에 없음
  NRF54L_PIN_NC,   //  82  P2.18 패키지에 없음
  NRF54L_PIN_NC,   //  83  P2.19 패키지에 없음
  NRF54L_PIN_NC,   //  84  P2.20 패키지에 없음
  NRF54L_PIN_NC,   //  85  P2.21 패키지에 없음
  NRF54L_PIN_NC,   //  86  P2.22 패키지에 없음
  NRF54L_PIN_NC,   //  87  P2.23 패키지에 없음
  NRF54L_PIN_NC,   //  88  P2.24 패키지에 없음
  NRF54L_PIN_NC,   //  89  P2.25 패키지에 없음
  NRF54L_PIN_NC,   //  90  P2.26 패키지에 없음
  NRF54L_PIN_NC,   //  91  P2.27 패키지에 없음
  NRF54L_PIN_NC,   //  92  P2.28 패키지에 없음
  NRF54L_PIN_NC,   //  93  P2.29 패키지에 없음
  NRF54L_PIN_NC,   //  94  P2.30 패키지에 없음
  NRF54L_PIN_NC,   //  95  P2.31 패키지에 없음
  96,             //  96  P3.00  D11 (뒷면 패드)
  97,             //  97  P3.01  D12 (뒷면 패드)
  98,             //  98  P3.02  D13 (뒷면 패드)
  99,             //  99  P3.03  D14 (뒷면 패드)
  100,            // 100  P3.04  D15 (뒷면 패드)
  101,            // 101  P3.05  D16 (뒷면 패드)
  102,            // 102  P3.06  D17 (뒷면 패드)
  103,            // 103  P3.07  D18 (뒷면 패드)
  104,            // 104  P3.08  미배선
  105,            // 105  P3.09  D25
  106,            // 106  P3.10  D26
  107,            // 107  P3.11  D27
  108,            // 108  P3.12  IMU CS — 출력으로 몰지 말 것
};

/* 아날로그 핀(A0..A7) -> Arduino 핀 번호. */
const uint8_t g_APinDescription_analog[NUM_ANALOG_INPUTS] =
{
  PIN_A0, PIN_A1, PIN_A2, PIN_A3, PIN_A4, PIN_A5, PIN_A6, PIN_A7,
};

/*
 * 부팅 시 보드 초기화. main() 이 init() 다음, setup() 전에 부른다.
 */
void initVariant(void)
{
  /* RGB LED 셋. active LOW 지만 ledOff() 가 LED_STATE_ON 을 반영한다. */
  pinMode(PIN_LED1, OUTPUT); ledOff(PIN_LED1);
  pinMode(PIN_LED2, OUTPUT); ledOff(PIN_LED2);
  pinMode(PIN_LED3, OUTPUT); ledOff(PIN_LED3);

  /* 버튼에는 외부 100K 풀업(R27)이 있다. 내부 풀업을 같이 켜도 무해하고,
   * 스케치가 pinMode 를 다시 부르지 않아도 digitalRead 가 동작한다. */
  pinMode(PIN_BUTTON1, INPUT_PULLUP);

  /*
   * 여기서 하지 않는 것:
   *
   *   - Sense 센서 전원 켜기. 전원이 GPIO 가 아니라 nPM1300 LDO1 이라 I2C 가
   *     필요한데, I2C(Wire)는 라이브러리라 코어에서 부를 수 없다.
   *     보드 라이브러리 BOARD-XIAO-nRF54LM20A 가 한다.
   *   - IMU CS(P3.12) 설정. 레일이 꺼진 채 몰면 IMU 가 역급전된다 (variant.h).
   *   - RF 스위치. XIAO nRF54L15 와 달리 이 보드에는 없다. 안테나도 없고
   *     u.FL 커넥터뿐이다 (variant.h 의 RF 절).
   */
}
