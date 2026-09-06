/*
 * variant.cpp — NU54-DK (nRF54L15)
 * baram-nrf54l-arduino
 * SPDX-License-Identifier: MIT
 *
 * 핀 매핑과 부팅 시 보드 초기화. 회로도 근거는 docs/boards/NU54-DK.md.
 */

#include "Arduino.h"
#include "variant.h"

/*
 * Arduino 핀 번호 -> 절대 GPIO 번호.
 *
 * 번호를 절대 GPIO 와 1:1 로 맞춰 두었다(항등 매핑). 그래야 스케치의
 * _PINNUM(port, pin) 이 그대로 통하고 Adafruit 코드를 옮기기 쉽다.
 * 모듈이 뽑아내지 않은 번호와 GPIO 로 쓰면 안 되는 핀은 NC 로 막는다.
 * NC 인 핀에 pinMode/digitalWrite 를 해도 조용히 무시된다.
 *
 * NCRB54N01VC 모듈이 노출하는 것: P0.00~04, P1.00~14, P2.00~10
 */
const uint32_t g_ADigitalPinMap[PINS_COUNT] =
{
  0,               //   0  P0.00  UART TX -> CP2102N RXD
  1,               //   1  P0.01  UART RX <- CP2102N TXD
  2,               //   2  P0.02  UART CTS <- CP2102N RTS
  3,               //   3  P0.03  UART RTS -> CP2102N CTS
  4,               //   4  P0.04  SW5
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
  36,              //  36  P1.04  AIN0
  37,              //  37  P1.05  AIN1
  38,              //  38  P1.06  AIN2
  39,              //  39  P1.07  AIN3
  40,              //  40  P1.08  SW4
  41,              //  41  P1.09  SW3
  42,              //  42  P1.10  LED D8
  43,              //  43  P1.11  AIN4
  44,              //  44  P1.12  AIN5
  45,              //  45  P1.13  AIN6 / SW2
  46,              //  46  P1.14  AIN7 / LED D10
  NRF54L_PIN_NC,   //  47  P1.15 미노출
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
  64,              //  64  P2.00
  65,              //  65  P2.01
  66,              //  66  P2.02
  67,              //  67  P2.03
  68,              //  68  P2.04
  69,              //  69  P2.05
  70,              //  70  P2.06
  71,              //  71  P2.07  LED D9 / SWO (J3-6)
  72,              //  72  P2.08
  73,              //  73  P2.09  LED D7
  74,              //  74  P2.10
};

/* 아날로그 핀(A0..A7) -> Arduino 핀 번호. SAADC 는 M2 에서 붙인다. */
const uint8_t g_APinDescription_analog[NUM_ANALOG_INPUTS] =
{
  PIN_A0, PIN_A1, PIN_A2, PIN_A3,
  PIN_A4, PIN_A5, PIN_A6, PIN_A7,
};

/*
 * 부팅 시 보드 초기화. main() 이 init() 다음, setup() 전에 부른다.
 */
void initVariant(void)
{
  /* LED 4개를 출력으로 잡고 전부 끈다.
   * ledOff() 가 LED_STATE_ON 을 반영하므로 active HIGH/LOW 를 여기서
   * 신경 쓰지 않아도 된다. */
  pinMode(PIN_LED1, OUTPUT); ledOff(PIN_LED1);
  pinMode(PIN_LED2, OUTPUT); ledOff(PIN_LED2);
  pinMode(PIN_LED3, OUTPUT); ledOff(PIN_LED3);
  pinMode(PIN_LED4, OUTPUT); ledOff(PIN_LED4);

  /* 버튼은 외부 풀업이 없다. 내부 풀업을 켜 둔다.
   * 스케치가 다시 pinMode 를 부르지 않아도 digitalRead 가 동작한다. */
  pinMode(PIN_BUTTON1, INPUT_PULLUP);
  pinMode(PIN_BUTTON2, INPUT_PULLUP);
  pinMode(PIN_BUTTON3, INPUT_PULLUP);
  pinMode(PIN_BUTTON4, INPUT_PULLUP);
}
