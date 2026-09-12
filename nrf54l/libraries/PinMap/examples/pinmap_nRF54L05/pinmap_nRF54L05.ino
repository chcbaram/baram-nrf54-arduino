/*********************************************************************
 nRF54L05 핀맵

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

 nRF54L05 — qfn48-6x6-qfaa, GPIO 31개

 포트 공통 — 그 포트의 어느 핀에나 붙는다

   P0*     SPIM/SPIS30, TWIM/TWIS30, UARTE30, GPIOTE30
   P1*     SPIM/SPIS20, TWIM/TWIS20, UARTE20, SPIM/SPIS21, TWIM/TWIS21
           UARTE21, SPIM/SPIS22, TWIM/TWIS22, UARTE22, PDM20, PDM21
           PWM20, PWM21, PWM22, GPIOTE20, I2S20, QDEC20, QDEC21
   P2*     없다 — 이 포트는 핀마다 다르다

 Arduino 함수 — 되는 곳

   analogWrite      PWM20/21/22    P1 전체 (15핀)
   attachInterrupt  GPIOTE20/30    P0 전체 (5핀), P1 전체 (15핀)
   analogRead       SAADC AIN0~7   P1.04(AIN0), P1.05(AIN1), P1.06(AIN2), P1.07(AIN3)
                                  P1.11(AIN4), P1.12(AIN5), P1.13(AIN6), P1.14(AIN7)

   ⚠ P2 에는 셋 다 없다 — 하드웨어가 없는 것이라 코어가 해 줄 수 있는 일이 아니다

 핀마다 추가로 되는 것 (위 공통에 **더해진다**)

   P0.03  GRTC.PWM
   P0.04  GRTC.LFCLKOUT
   P1.00  LFXO.XL1
   P1.01  LFXO.XL2
   P1.02  NFCT.NFC1
   P1.03  NFCT.NFC2
   P1.04  SAADC.AIN0, TAMPC.ASO[0]
   P1.05  RADIO.RADIO[6], SAADC.AIN1, TAMPC.ASI[0]
   P1.06  SAADC.AIN2, TAMPC.ASO[1]
   P1.07  SAADC.AIN3, TAMPC.ASI[1]
   P1.08  SAADC.EXTREF, GRTC.CLK16M
   P1.09  RADIO.RADIO[0], TAMPC.ASO[2]
   P1.10  RADIO.RADIO[1], TAMPC.ASI[2]
   P1.11  RADIO.RADIO[2], SAADC.AIN4, TAMPC.ASO[3]
   P1.12  RADIO.RADIO[3], SAADC.AIN5, TAMPC.ASI[3]
   P1.13  RADIO.RADIO[4], SAADC.AIN6
   P1.14  RADIO.RADIO[5], SAADC.AIN7
   P2.00  SPIM/SPIS00.DCX, UARTE00.RXD, SPIM/SPIS20.DCX, UARTE20.RXD, sQSPI.D3
   P2.01  SPIM/SPIS00.SCK, SPIM/SPIS20.SCK, sQSPI.SCK
   P2.02  SPIM/SPIS00.SDO, UARTE00.TXD, SPIM/SPIS20.SDO, UARTE20.TXD, sQSPI.D0
   P2.03  sQSPI.D2
   P2.04  SPIM/SPIS00.SDI, UARTE00.CTS, SPIM/SPIS20.SDI, UARTE20.CTS, sQSPI.D1
   P2.05  SPIM/SPIS00.CS, UARTE00.RTS, SPIM/SPIS20.CS, UARTE20.RTS, sQSPI.CSN
   P2.06  SPIM/SPIS00.SCK, SPIM/SPIS21.SCK
   P2.07  SPIM/SPIS00.DCX, UARTE00.RXD, SPIM/SPIS21.DCX, UARTE21.RXD
   P2.08  SPIM/SPIS00.SDO, UARTE00.TXD, SPIM/SPIS21.SDO, UARTE21.TXD
   P2.09  SPIM/SPIS00.SDI, UARTE00.CTS, SPIM/SPIS21.SDI, UARTE21.CTS
   P2.10  SPIM/SPIS00.CS, UARTE00.RTS, SPIM/SPIS21.CS, UARTE21.RTS

   나머지는 공통뿐: P0.00, P0.01, P0.02

 baram-nrf54l-arduino - MIT license
*********************************************************************/

void setup()
{
  Serial.begin(115200);
  delay(300);

  /* 이 스케치는 위 주석이 본체다. 굽지 않아도 된다.
     굳이 돌리면 보드 이름과 실장 칩만 확인해 준다. */
  Serial.println("nRF54L05 핀맵");
  Serial.print("빌드된 보드: ");
  Serial.println(BOARD_NAME);

  uint32_t part = *(volatile uint32_t *) 0x00FFC31C;   /* FICR INFO.PART */
  Serial.print("실장 칩 FICR INFO.PART = 0x");
  Serial.println(part, HEX);
}

void loop()
{
}
