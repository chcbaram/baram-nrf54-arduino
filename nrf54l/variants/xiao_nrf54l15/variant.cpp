/*
 * variant.cpp — Seeed Studio XIAO nRF54L15 / XIAO nRF54L15 Sense
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * 핀 매핑과 부팅 시 보드 초기화. 회로도 근거는 docs/boards/XIAO-nRF54L15.md.
 */

#include "Arduino.h"
#include "variant.h"

/*
 * Arduino 핀 번호 -> 절대 GPIO 번호.
 *
 * 번호를 절대 GPIO 와 1:1 로 맞춰 두었다(항등 매핑). 그래야 스케치의
 * _PINNUM(port, pin) 이 그대로 통하고 NU54-DK variant 와도 규약이 같다.
 * 존재하지 않는 핀과 GPIO 로 쓰면 안 되는 핀은 NC 로 막는다.
 * NC 인 핀에 pinMode/digitalWrite 를 해도 조용히 무시된다.
 *
 * nRF54L15-CAAA(WLCSP)가 이 보드에서 쓰는 것: P0.00~04, P1.00~15, P2.00~10
 */
const uint32_t g_ADigitalPinMap[PINS_COUNT] =
{
  0,               //   0  P0.00  USR_KEY (K2, 외부 10K 풀업)
  1,               //   1  P0.01  IMU & MIC 3V3 EN  (0:차단 1:공급)
  2,               //   2  P0.02  IMU INT1
  3,               //   3  P0.03  D11 / IMU SCL  (TWIM30)
  4,               //   4  P0.04  D12 / IMU SDA  (TWIM30)
  NRF54L_PIN_NC,   //   5  P0.05 미노출
  NRF54L_PIN_NC,   //   6  P0.06 미노출
  NRF54L_PIN_NC,   //   7  P0.07 미노출
  NRF54L_PIN_NC,   //   8  P0.08 미노출
  NRF54L_PIN_NC,   //   9  P0.09 미노출
  NRF54L_PIN_NC,   //  10  P0.10 미노출
  NRF54L_PIN_NC,   //  11  P0.11 미노출
  NRF54L_PIN_NC,   //  12  P0.12 미노출
  NRF54L_PIN_NC,   //  13  P0.13 미노출
  NRF54L_PIN_NC,   //  14  P0.14 미노출
  NRF54L_PIN_NC,   //  15  P0.15 미노출
  NRF54L_PIN_NC,   //  16  P0.16 미노출
  NRF54L_PIN_NC,   //  17  P0.17 미노출
  NRF54L_PIN_NC,   //  18  P0.18 미노출
  NRF54L_PIN_NC,   //  19  P0.19 미노출
  NRF54L_PIN_NC,   //  20  P0.20 미노출
  NRF54L_PIN_NC,   //  21  P0.21 미노출
  NRF54L_PIN_NC,   //  22  P0.22 미노출
  NRF54L_PIN_NC,   //  23  P0.23 미노출
  NRF54L_PIN_NC,   //  24  P0.24 미노출
  NRF54L_PIN_NC,   //  25  P0.25 미노출
  NRF54L_PIN_NC,   //  26  P0.26 미노출
  NRF54L_PIN_NC,   //  27  P0.27 미노출
  NRF54L_PIN_NC,   //  28  P0.28 미노출
  NRF54L_PIN_NC,   //  29  P0.29 미노출
  NRF54L_PIN_NC,   //  30  P0.30 미노출
  NRF54L_PIN_NC,   //  31  P0.31 미노출
  NRF54L_PIN_NC,   //  32  P1.00  XL1 (LFXO) — GPIO 사용 금지
  NRF54L_PIN_NC,   //  33  P1.01  XL2 (LFXO) — GPIO 사용 금지
  34,              //  34  P1.02  NFC1
  35,              //  35  P1.03  NFC2
  36,              //  36  P1.04  D0 / A0 / AIN0
  37,              //  37  P1.05  D1 / A1 / AIN1
  38,              //  38  P1.06  D2 / A2 / AIN2
  39,              //  39  P1.07  D3 / A3 / AIN3
  40,              //  40  P1.08  Serial RX  <- SAMD11 TX (UARTE20)
  41,              //  41  P1.09  Serial TX  -> SAMD11 RX (UARTE20)
  42,              //  42  P1.10  D4 / SDA  (TWIM22)
  43,              //  43  P1.11  D5 / SCL  (TWIM22) / AIN4
  44,              //  44  P1.12  MIC CLK  (PDM20) / AIN5
  45,              //  45  P1.13  MIC DATA (PDM20) / AIN6
  46,              //  46  P1.14  VBAT sense / AIN7
  47,              //  47  P1.15  VBAT sense enable
  NRF54L_PIN_NC,   //  48  P1.16 미노출
  NRF54L_PIN_NC,   //  49  P1.17 미노출
  NRF54L_PIN_NC,   //  50  P1.18 미노출
  NRF54L_PIN_NC,   //  51  P1.19 미노출
  NRF54L_PIN_NC,   //  52  P1.20 미노출
  NRF54L_PIN_NC,   //  53  P1.21 미노출
  NRF54L_PIN_NC,   //  54  P1.22 미노출
  NRF54L_PIN_NC,   //  55  P1.23 미노출
  NRF54L_PIN_NC,   //  56  P1.24 미노출
  NRF54L_PIN_NC,   //  57  P1.25 미노출
  NRF54L_PIN_NC,   //  58  P1.26 미노출
  NRF54L_PIN_NC,   //  59  P1.27 미노출
  NRF54L_PIN_NC,   //  60  P1.28 미노출
  NRF54L_PIN_NC,   //  61  P1.29 미노출
  NRF54L_PIN_NC,   //  62  P1.30 미노출
  NRF54L_PIN_NC,   //  63  P1.31 미노출
  64,              //  64  P2.00  USR_LED (D3 녹색, active LOW)
  65,              //  65  P2.01  D8  / SCK  (SPIM00)
  66,              //  66  P2.02  D10 / MOSI (SPIM00)
  67,              //  67  P2.03  RF SW power
  68,              //  68  P2.04  D9  / MISO (SPIM00)
  69,              //  69  P2.05  RF SW select (0:온보드 1:u.FL)
  70,              //  70  P2.06  D15
  71,              //  71  P2.07  D7 / SWO
  72,              //  72  P2.08  D6
  73,              //  73  P2.09  D14
  74,              //  74  P2.10  D13
};

/* 아날로그 핀(A0..A3) -> Arduino 핀 번호. SAADC 는 M2 에서 붙인다. */
const uint8_t g_APinDescription_analog[NUM_ANALOG_INPUTS] =
{
  PIN_A0, PIN_A1, PIN_A2, PIN_A3,
};

/*
 * 부팅 시 보드 초기화. main() 이 init() 다음, setup() 전에 부른다.
 */
void initVariant(void)
{
  /* 사용자 LED 하나. active LOW 지만 ledOff() 가 LED_STATE_ON 을 반영한다. */
  pinMode(PIN_LED1, OUTPUT); ledOff(PIN_LED1);

  /* 버튼에는 외부 10K 풀업(R13)이 있다. 내부 풀업을 같이 켜도 무해하고,
   * 스케치가 pinMode 를 다시 부르지 않아도 digitalRead 가 동작한다. */
  pinMode(PIN_BUTTON1, INPUT_PULLUP);

  /* 배터리 분압은 로드 스위치로 게이팅돼 있다. 평소엔 끊어 둔다(누설 0).
   * 읽을 때만 HIGH 로 올리고 안정화 시간을 준다. */
  pinMode(PIN_VBAT_ENABLE, OUTPUT);
  digitalWrite(PIN_VBAT_ENABLE, LOW);

  /* 온보드 IMU / 마이크 전원도 로드 스위치 뒤에 있다. 기본은 차단이다.
   * Sense 모델에서 센서를 쓰려면 스케치가 HIGH 로 올린다.
   * 안 쓰는 동안 꺼 두는 것이 저전력에 유리하다. */
  pinMode(PIN_SENSOR_POWER, OUTPUT);
  digitalWrite(PIN_SENSOR_POWER, LOW);

  /*
   * 안테나 경로. FM8625H RF 스위치에 전원이 없으면 RF 경로가 성립하지
   * 않으므로 BLE 가 동작하지 않는다. 온보드 칩 안테나(RF1)로 잡아 둔다.
   * u.FL 로 바꾸려면 PIN_RF_SW_SELECT 를 HIGH 로 하면 된다.
   *
   * ⚠ M1 에는 라디오가 없어 이 설정이 하는 일은 스위치에 전원을 주는 것뿐이다.
   *   전류를 측정할 때는 이 두 줄의 몫을 따로 빼서 보라 (CLAUDE.md §7 F8).
   *   실제로 RF1 이 온보드 안테나인지는 M3 에서 확인한다.
   */
  pinMode(PIN_RF_SW_SELECT, OUTPUT);
  digitalWrite(PIN_RF_SW_SELECT, RF_SW_SELECT_ONBOARD);
  pinMode(PIN_RF_SW_POWER, OUTPUT);
  digitalWrite(PIN_RF_SW_POWER, HIGH);
}
