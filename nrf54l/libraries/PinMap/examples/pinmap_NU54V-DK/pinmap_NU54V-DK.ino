/*********************************************************************
 NU54V-DK 핀맵 (nRF54L15)

 ⚠ 이 파일은 손으로 쓴 것이 아니다. `extras/gen_pinmap.py` 가
   Nordic Pin Planner 의 SoC 정의에서 구워 낸다. 고칠 일이 있으면
   여기가 아니라 생성기나 출처 문서를 고쳐라.

     https://github.com/NordicPlayground/PinPlanner

 읽는 법은 이렇다. 표는 **핀 기준**이라 "P1.08 에 뭘 붙일 수 있나" 를
 바로 볼 수 있다. 반대 방향("I2C 를 어디에 붙이나")은 포트 공통 줄을
 보면 된다 — nRF54L 은 페리페럴이 도메인에 묶여 있어서, 대개
 **어느 핀이냐보다 어느 포트냐가 먼저 정해진다.**

 같은 번호의 SPIM/SPIS/TWIM/TWIS/UARTE 는 **하나의 하드웨어 블록**이다.
 TWIM20 을 쓰면 UARTE20 과 SPIM20 은 못 쓴다. 표에 셋이 나란히 보이는
 것은 "골라 쓸 수 있다" 는 뜻이지 "동시에 된다" 는 뜻이 아니다.

 NU54V-DK — nRF54L15, qfn48-6x6-qfaa

 포트 공통 — 그 포트의 어느 핀에나 붙는다

   P0*     SPIM/SPIS30, TWIM/TWIS30, UARTE30, GPIOTE30
   P1*     SPIM/SPIS20, TWIM/TWIS20, UARTE20, SPIM/SPIS21, TWIM/TWIS21
           UARTE21, SPIM/SPIS22, TWIM/TWIS22, UARTE22, PDM20, PDM21
           PWM20, PWM21, PWM22, GPIOTE20, I2S20, QDEC20, QDEC21
   P2*     없다 — 이 포트는 핀마다 다르다. 아래 표를 봐라

 Arduino 함수 — 되는 곳

   analogWrite      PWM20/21/22    P1 전체 (15핀)
   attachInterrupt  GPIOTE20/30    P0 전체 (5핀), P1 전체 (15핀)
   analogRead       SAADC AIN0~7   P1.04(AIN0), P1.05(AIN1), P1.06(AIN2), P1.07(AIN3)
                                  P1.11(AIN4), P1.12(AIN5), P1.13(AIN6), P1.14(AIN7)

   ⚠ P2 에는 셋 다 없다 — 하드웨어가 없는 것이라 코어가 해 줄 수 있는 일이 아니다

 아래 표의 '이 핀만' 은 위 공통에 **더해지는** 것이다.
 PWM = analogWrite   IRQ = attachInterrupt   ADC = analogRead
 'x' 는 **그 핀에서 그 함수를 쓸 수 없다** 는 뜻이다. 하드웨어가 없는 것이라
 코어가 나중에 지원해 주는 종류의 것이 아니다.

 P1 헤더
   Pin GPIO   Name in sketch                PWM IRQ ADC  Only this pin
   --- ------ ----------------------------- --- --- ---- ----------------------------------
     1 P0.00  PIN_SERIAL_TX                 x   o   x
     2 P0.01  PIN_SERIAL_RX                 x   o   x
     3 GND
     4 P0.02  PIN_SERIAL_CTS                x   o   x
     5 P0.03  PIN_SERIAL_RTS                x   o   x    GRTC.PWM
     6 P0.04  PIN_BUTTON4                   x   o   x    GRTC.LFCLKOUT
     7 P1.00                                o   o   x    LFXO.XL1
     8 GND
     9 P1.01                                o   o   x    LFXO.XL2
    10 P1.02  PIN_NFC1                      o   o   x    NFCT.NFC1
    11 P1.03  PIN_NFC2                      o   o   x    NFCT.NFC2
    12 P1.04  PIN_A0, A0                    o   o   AIN0 SAADC.AIN0, TAMPC.ASO[0]
    13 GND
    14 P1.05  PIN_A1, A1                    o   o   AIN1 RADIO.RADIO[6], SAADC.AIN1, TAMPC.ASI[0]
    15 P1.06  PIN_A2, A2                    o   o   AIN2 SAADC.AIN2, TAMPC.ASO[1]
    16 P1.07  PIN_A3, A3                    o   o   AIN3 SAADC.AIN3, TAMPC.ASI[1]
    17 P1.08  PIN_BUTTON3                   o   o   x    SAADC.EXTREF, GRTC.CLK16M
    18 GND
    19 P1.09  PIN_BUTTON2                   o   o   x    RADIO.RADIO[0], TAMPC.ASO[2]
    20 P1.10  PIN_LED2, LED_CONN, LED_BLUE  o   o   x    RADIO.RADIO[1], TAMPC.ASI[2]
    21 P1.11  PIN_A4, PIN_WIRE_SDA, A4, SDA o   o   AIN4 RADIO.RADIO[2], SAADC.AIN4, TAMPC.ASO[3]
    22 SWDCLK
    23 SWDIO
    24 GND
    25 GND

 P3 헤더
   Pin GPIO   Name in sketch                 PWM IRQ ADC  Only this pin
   --- ------ ------------------------------ --- --- ---- ----------------------------------
     1 GND
     2 GND
     3 RESET
     4 P1.12  PIN_A5, PIN_WIRE_SCL, A5, SCL  o   o   AIN5 RADIO.RADIO[3], SAADC.AIN5, TAMPC.ASI[3]
     5 P1.13  PIN_BUTTON1, PIN_A6, A6        o   o   AIN6 RADIO.RADIO[4], SAADC.AIN6
     6 P1.14  PIN_LED4, PIN_A7, A7           o   o   AIN7 RADIO.RADIO[5], SAADC.AIN7
     7 P2.10                                 x   x   x    SPIM/SPIS00.CS, UARTE00.RTS, SPIM/SPIS21.CS, UARTE21.RTS
     8 GND
     9 P2.09  PIN_LED1, LED_BUILTIN, LED_RED x   x   x    SPIM/SPIS00.SDI, UARTE00.CTS, SPIM/SPIS21.SDI, UARTE21.CTS
    10 P2.08                                 x   x   x    SPIM/SPIS00.SDO, UARTE00.TXD, SPIM/SPIS21.SDO, UARTE21.TXD
    11 P2.07  PIN_LED3                       x   x   x    SPIM/SPIS00.DCX, UARTE00.RXD, SPIM/SPIS21.DCX, UARTE21.RXD
    12 P2.06                                 x   x   x    SPIM/SPIS00.SCK, SPIM/SPIS21.SCK
    13 GND
    14 P2.05  SS                             x   x   x    SPIM/SPIS00.CS, UARTE00.RTS, SPIM/SPIS20.CS, UARTE20.RTS, sQSPI.CSN
    15 P2.04  PIN_SPI_MISO, MISO             x   x   x    SPIM/SPIS00.SDI, UARTE00.CTS, SPIM/SPIS20.SDI, UARTE20.CTS, sQSPI.D1
    16 P2.03                                 x   x   x    sQSPI.D2
    17 P2.02  PIN_SPI_MOSI, MOSI             x   x   x    SPIM/SPIS00.SDO, UARTE00.TXD, SPIM/SPIS20.SDO, UARTE20.TXD, sQSPI.D0
    18 GND
    19 P2.01  PIN_SPI_SCK, SCK               x   x   x    SPIM/SPIS00.SCK, SPIM/SPIS20.SCK, sQSPI.SCK
    20 P2.00                                 x   x   x    SPIM/SPIS00.DCX, UARTE00.RXD, SPIM/SPIS20.DCX, UARTE20.RXD, sQSPI.D3
    21 VMCU
    22 3V3
    23 GND
    24 GND
    25 VIN


 baram-nrf54l-arduino - MIT license
*********************************************************************/

void setup()
{
  Serial.begin(115200);
  delay(300);

  /* 이 스케치는 위 주석이 본체다. 굽지 않아도 된다.
     굳이 돌리면 보드 이름과 실장 칩만 확인해 준다. */
  Serial.println("NU54V-DK 핀맵 (nRF54L15)");
  Serial.print("빌드된 보드: ");
  Serial.println(BOARD_NAME);

  uint32_t part = *(volatile uint32_t *) 0x00FFC31C;   /* FICR INFO.PART */
  Serial.print("실장 칩 FICR INFO.PART = 0x");
  Serial.println(part, HEX);
}

void loop()
{
}
