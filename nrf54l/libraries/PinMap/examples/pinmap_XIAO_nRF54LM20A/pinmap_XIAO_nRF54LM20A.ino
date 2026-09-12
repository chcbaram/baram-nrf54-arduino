/*********************************************************************
 XIAO_nRF54LM20A 핀맵 (nRF54LM20A)

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

 XIAO 헤더 (D0~D27)
   Pin GPIO   Board           PWM IRQ ADC  Only this pin
   --- ------ --------------- --- --- ---- ----------------------------------
     0 P1.00  D0 / A0         o   o   AIN0 SAADC.AIN0
     1 P1.31  D1 / A1         o   o   AIN1 SAADC.AIN1
     2 P1.30  D2 / A2         o   o   AIN2 SAADC.AIN2
     3 P1.29  D3 / A3         o   o   AIN3 SAADC.AIN3
     4 P1.03  D4 / SDA / A7   o   o   AIN7 SAADC.AIN7
     5 P1.07  D5 / SCL / A8   o   o   x    SAADC.EXTREF, GRTC.CLKOUTFAST
     6 P1.08  D6 / TX         o   o   x    TAMPC.ASO[1]
     7 P1.09  D7 / RX         o   o   x    TAMPC.ASI[1]
     8 P1.04  D8 / SCK / A6   o   o   AIN6 SAADC.AIN6
     9 P1.05  D9 / MISO / A5  o   o   AIN5 SAADC.AIN5, TAMPC.ASO[0]
    10 P1.06  D10 / MOSI / A4 o   o   AIN4 SAADC.AIN4, TAMPC.ASI[0]
    11 P3.00  D11             o   o   x
    12 P3.01  D12             o   o   x
    13 P3.02  D13             o   o   x
    14 P3.03  D14             o   o   x
    15 P3.04  D15             o   o   x
    16 P3.05  D16             o   o   x
    17 P3.06  D17             o   o   x
    18 P3.07  D18             o   o   x
    19 P0.00  D19             x   o   x
    20 P0.01  D20             x   o   x
    21 P0.02  D21             x   o   x
    22 P0.03  D22             x   o   x    GRTC.PWM
    23 P0.04  D23             x   o   x    GRTC.CLKOUT32K
    24 P0.05  D24             x   o   x
    25 P3.09  D25             o   o   x
    26 P3.10  D26             o   o   x
    27 P3.11  D27             o   o   x


 baram-nrf54l-arduino - MIT license
*********************************************************************/

void setup()
{
  Serial.begin(115200);
  delay(300);

  /* 이 스케치는 위 주석이 본체다. 굽지 않아도 된다.
     굳이 돌리면 보드 이름과 실장 칩만 확인해 준다. */
  Serial.println("XIAO_nRF54LM20A 핀맵 (nRF54LM20A)");
  Serial.print("빌드된 보드: ");
  Serial.println(BOARD_NAME);

  uint32_t part = *(volatile uint32_t *) 0x00FFC31C;   /* FICR INFO.PART */
  Serial.print("실장 칩 FICR INFO.PART = 0x");
  Serial.println(part, HEX);
}

void loop()
{
}
