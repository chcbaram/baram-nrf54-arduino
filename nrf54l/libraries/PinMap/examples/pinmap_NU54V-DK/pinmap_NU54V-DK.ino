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

 P2 헤더 (30핀)
   Pin GPIO   Board          Name in sketch                 PWM IRQ ADC  Only this pin
   --- ------ -------------- ------------------------------ --- --- ---- ----------------------------------
     9 P1.07  A3 / UART2 CTS PIN_A3, PIN_SERIAL1_CTS, A3    o   o   AIN3 SAADC.AIN3, TAMPC.ASI[1]
    10 P1.06  A2 / UART2 RTS PIN_A2, PIN_SERIAL1_RTS, A2    o   o   AIN2 SAADC.AIN2, TAMPC.ASO[1]
    11 P1.05  A1 / UART2 RX  PIN_A1, PIN_SERIAL1_RX, A1     o   o   AIN1 RADIO.RADIO[6], SAADC.AIN1, TAMPC.ASI[0]
    12 P1.04  A0 / UART2 TX  PIN_A0, PIN_SERIAL1_TX, A0     o   o   AIN0 SAADC.AIN0, TAMPC.ASO[0]
    16 P1.08  SW3            PIN_BUTTON3                    o   o   x    SAADC.EXTREF, GRTC.CLK16M
    17 P2.04  —              PIN_SPI_MISO, MISO             x   x   x    SPIM/SPIS00.SDI, UARTE00.CTS, SPIM/SPIS20.SDI, UARTE20.CTS, sQSPI.D1
    19 P2.05  —              SS                             x   x   x    SPIM/SPIS00.CS, UARTE00.RTS, SPIM/SPIS20.CS, UARTE20.RTS, sQSPI.CSN
    20 P2.06  —                                             x   x   x    SPIM/SPIS00.SCK, SPIM/SPIS21.SCK
    21 P2.07  LED3 / SWO     PIN_LED3                       x   x   x    SPIM/SPIS00.DCX, UARTE00.RXD, SPIM/SPIS21.DCX, UARTE21.RXD
    22 P2.08  PMIC           PIN_PMIC_SB2                   x   x   x    SPIM/SPIS00.SDO, UARTE00.TXD, SPIM/SPIS21.SDO, UARTE21.TXD
    23 P2.09  LED1           PIN_LED1, LED_BUILTIN, LED_RED x   x   x    SPIM/SPIS00.SDI, UARTE00.CTS, SPIM/SPIS21.SDI, UARTE21.CTS
    24 P2.10  PMIC           PIN_PMIC_SB3                   x   x   x    SPIM/SPIS00.CS, UARTE00.RTS, SPIM/SPIS21.CS, UARTE21.RTS
    25 P0.00  Serial TX      PIN_SERIAL_TX                  x   o   x
    26 P0.01  Serial RX      PIN_SERIAL_RX                  x   o   x

 P4 헤더 (30핀)
   Pin GPIO   Board      Name in sketch               PWM IRQ ADC  Only this pin
   --- ------ ---------- ---------------------------- --- --- ---- ----------------------------------
     4 P0.02  Serial CTS PIN_SERIAL_CTS               x   o   x
     5 P0.03  Serial RTS PIN_SERIAL_RTS               x   o   x    GRTC.PWM
     6 P0.04  SW4        PIN_BUTTON4                  x   o   x    GRTC.LFCLKOUT
     7 P1.09  SW2        PIN_BUTTON2                  o   o   x    RADIO.RADIO[0], TAMPC.ASO[2]
     8 P1.10  LED2       PIN_LED2, LED_CONN, LED_BLUE o   o   x    RADIO.RADIO[1], TAMPC.ASI[2]
     9 P1.11  A4 / PMIC  PIN_A4, PIN_PMIC_SB1, A4     o   o   AIN4 RADIO.RADIO[2], SAADC.AIN4, TAMPC.ASO[3]
    10 P1.12  A5 / PMIC  PIN_A5, PIN_PMIC_SB4, A5     o   o   AIN5 RADIO.RADIO[3], SAADC.AIN5, TAMPC.ASI[3]
    11 P1.13  SW1 / A6   PIN_BUTTON1, PIN_A6, A6      o   o   AIN6 RADIO.RADIO[4], SAADC.AIN6
    12 P1.14  LED4 / A7  PIN_LED4, PIN_A7, A7         o   o   AIN7 RADIO.RADIO[5], SAADC.AIN7
    16 P1.02  Qwiic SDA  PIN_WIRE_SDA, SDA            o   o   x    NFCT.NFC1
    17 P1.03  Qwiic SCL  PIN_WIRE_SCL, SCL            o   o   x    NFCT.NFC2
    19 P2.00  —                                       x   x   x    SPIM/SPIS00.DCX, UARTE00.RXD, SPIM/SPIS20.DCX, UARTE20.RXD, sQSPI.D3
    20 P2.01  —          PIN_SPI_SCK, SCK             x   x   x    SPIM/SPIS00.SCK, SPIM/SPIS20.SCK, sQSPI.SCK
    21 P2.02  —          PIN_SPI_MOSI, MOSI           x   x   x    SPIM/SPIS00.SDO, UARTE00.TXD, SPIM/SPIS20.SDO, UARTE20.TXD, sQSPI.D0
    22 P2.03  —                                       x   x   x    sQSPI.D2


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
