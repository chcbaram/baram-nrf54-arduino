/*********************************************************************
 XIAO_nRF54LM20A pin map (nRF54LM20A)

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

 XIAO_nRF54LM20A — nRF54LM20A, fccsp98-3.67x3.85-paaa

 포트 공통 — 그 포트의 어느 핀에나 붙는다

   P0*     GPIOTE30, SPIM/SPIS30, TWIM/TWIS30, UARTE30
   P1*     PWM20, PWM21, PWM22, GPIOTE20, SPIM/SPIS20, TWIM/TWIS20
           UARTE20, SPIM/SPIS21, TWIM/TWIS21, UARTE21, SPIM/SPIS22
           TWIM/TWIS22, UARTE22, SPIM/SPIS23, TWIM/TWIS23, UARTE23
           SPIM/SPIS24, TWIM/TWIS24, UARTE24, PDM20, PDM21, TDM, QDEC20
           QDEC21
   P3*     PWM20, PWM21, PWM22, GPIOTE20, SPIM/SPIS20, TWIM/TWIS20
           UARTE20, SPIM/SPIS21, TWIM/TWIS21, UARTE21, SPIM/SPIS22
           TWIM/TWIS22, UARTE22, SPIM/SPIS23, TWIM/TWIS23, UARTE23
           SPIM/SPIS24, TWIM/TWIS24, UARTE24
   P2*     없다 — 이 포트는 핀마다 다르다. 아래 표를 봐라

 Arduino 함수 — 되는 곳

   analogWrite      PWM20/21/22    P1 전체 (32핀), P3 전체 (13핀)
   attachInterrupt  GPIOTE20/30    P0 전체 (10핀), P1 전체 (32핀), P3 전체 (13핀)
   analogRead       SAADC AIN0~7   P1.00(AIN0), P1.03(AIN7), P1.04(AIN6), P1.05(AIN5)
                                  P1.06(AIN4), P1.29(AIN3), P1.30(AIN2), P1.31(AIN1)

   ⚠ P2 에는 셋 다 없다 — 하드웨어가 없는 것이라 코어가 해 줄 수 있는 일이 아니다

 아래 표의 '이 핀만' 은 위 공통에 **더해지는** 것이다.
 PWM = analogWrite   IRQ = attachInterrupt   ADC = analogRead
 'x' 는 **그 핀에서 그 함수를 쓸 수 없다** 는 뜻이다. 하드웨어가 없는 것이라
 코어가 나중에 지원해 주는 종류의 것이 아니다.

 XIAO header (D0–D27)
   Pin GPIO   Board           Name in sketch                      PWM IRQ ADC  Only this pin
   --- ------ --------------- ----------------------------------- --- --- ---- ----------------------------------
     0 P1.00  D0 / A0         PIN_A0, D0, A0                      o   o   AIN0 SAADC.AIN0
     1 P1.31  D1 / A1         PIN_A1, D1, A1                      o   o   AIN1 SAADC.AIN1
     2 P1.30  D2 / A2         PIN_A2, D2, A2                      o   o   AIN2 SAADC.AIN2
     3 P1.29  D3 / A3         PIN_A3, D3, A3                      o   o   AIN3 SAADC.AIN3
     4 P1.03  D4 / SDA / A7   PIN_A7, PIN_WIRE_SDA, D4, A7, SDA   o   o   AIN7 SAADC.AIN7
     5 P1.07  D5 / SCL / A8   PIN_WIRE_SCL, D5, SCL               o   o   x    SAADC.EXTREF, GRTC.CLKOUTFAST
     6 P1.08  D6 / TX         PIN_SERIAL1_TX, D6                  o   o   x    TAMPC.ASO[1]
     7 P1.09  D7 / RX         PIN_SERIAL1_RX, D7, SS              o   o   x    TAMPC.ASI[1]
     8 P1.04  D8 / SCK / A6   PIN_A6, PIN_SPI_SCK, D8, A6, SCK    o   o   AIN6 SAADC.AIN6
     9 P1.05  D9 / MISO / A5  PIN_A5, PIN_SPI_MISO, D9, A5, MISO  o   o   AIN5 SAADC.AIN5, TAMPC.ASO[0]
    10 P1.06  D10 / MOSI / A4 PIN_A4, PIN_SPI_MOSI, D10, A4, MOSI o   o   AIN4 SAADC.AIN4, TAMPC.ASI[0]
    11 P3.00  D11             D11                                 o   o   x
    12 P3.01  D12             D12                                 o   o   x
    13 P3.02  D13             D13                                 o   o   x
    14 P3.03  D14             D14                                 o   o   x
    15 P3.04  D15             D15                                 o   o   x
    16 P3.05  D16             D16                                 o   o   x
    17 P3.06  D17             D17                                 o   o   x
    18 P3.07  D18             D18                                 o   o   x
    19 P0.00  D19             D19                                 x   o   x
    20 P0.01  D20             D20                                 x   o   x
    21 P0.02  D21             D21                                 x   o   x
    22 P0.03  D22             D22                                 x   o   x    GRTC.PWM
    23 P0.04  D23             D23                                 x   o   x    GRTC.CLKOUT32K
    24 P0.05  D24             D24                                 x   o   x
    25 P3.09  D25             D25                                 o   o   x
    26 P3.10  D26             D26                                 o   o   x
    27 P3.11  D27             D27                                 o   o   x

 헤더 밖 — 온보드 부품이 쓰는 핀
       GPIO   Name in sketch                 PWM IRQ ADC  Only this pin
       ------ ------------------------------ --- --- ---- ----------------------------------
       P0.06  PIN_IMU_INT1                   x   o   x
       P0.07  PIN_WIRE1_SCL, PIN_IMU_SCL     x   o   x
       P0.08  PIN_WIRE1_SDA, PIN_IMU_SDA     x   o   x
       P0.09  PIN_BUTTON1                    x   o   x
       P1.01  PIN_NFC1                       o   o   x    NFCT.NFC1
       P1.02  PIN_NFC2                       o   o   x    NFCT.NFC2
       P1.10  PIN_SERIAL_RX                  o   o   x
       P1.11  PIN_SERIAL_TX                  o   o   x
       P1.13  PIN_PDM_CLK                    o   o   x
       P1.14  PIN_PDM_DATA                   o   o   x
       P1.17  PIN_PMIC_SCL                   o   o   x
       P1.18  PIN_PMIC_SDA                   o   o   x    RADIO.RADIO[3]
       P1.22  PIN_LED1, LED_BUILTIN, LED_RED o   o   x
       P1.23  PIN_LED2, LED_BLUE, LED_CONN   o   o   x
       P1.24  PIN_LED3, LED_GREEN            o   o   x
       P1.25  PIN_PMIC_GPIO0                 o   o   x
       P1.26  PIN_PMIC_GPIO1                 o   o   x
       P2.00  PIN_FLASH_HOLD                 x   x   x    SPIM/SPIS00.DCX, UARTE00.RXD, QSPI.D3
       P2.01  PIN_SPI1_SCK                   x   x   x    SPIM/SPIS00.SCK, QSPI.SCK
       P2.02  PIN_SPI1_MOSI                  x   x   x    SPIM/SPIS00.SDO, UARTE00.TXD, QSPI.D0
       P2.03  PIN_FLASH_WP                   x   x   x    QSPI.D2
       P2.04  PIN_SPI1_MISO                  x   x   x    SPIM/SPIS00.SDI, UARTE00.CTS, QSPI.D1
       P2.05  PIN_FLASH_CS                   x   x   x    SPIM/SPIS00.CSN, UARTE00.RTS, QSPI.CSN
       P3.12  PIN_IMU_CS                     o   o   x


 baram-nrf54l-arduino - MIT license
*********************************************************************/

void setup()
{
  Serial.begin(115200);
  delay(300);

  /* 이 스케치는 위 주석이 본체다. 굽지 않아도 된다.
     굳이 돌리면 보드 이름과 실장 칩만 확인해 준다. */
  Serial.println("XIAO_nRF54LM20A pin map (nRF54LM20A)");
  Serial.print("Built for board: ");
  Serial.println(BOARD_NAME);

  uint32_t part = *(volatile uint32_t *) 0x00FFC31C;   /* FICR INFO.PART */
  Serial.print("Chip on board, FICR INFO.PART = 0x");
  Serial.println(part, HEX);
}

void loop()
{
}
