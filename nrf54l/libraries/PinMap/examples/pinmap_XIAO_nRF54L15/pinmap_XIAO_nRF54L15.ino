/*********************************************************************
 XIAO_nRF54L15 핀맵 (nRF54L15)

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

 XIAO_nRF54L15 — nRF54L15, qfn48-6x6-qfaa

 포트 공통 — 그 포트의 어느 핀에나 붙는다

   P0*     SPIM/SPIS30, TWIM/TWIS30, UARTE30, GPIOTE30
   P1*     SPIM/SPIS20, TWIM/TWIS20, UARTE20, SPIM/SPIS21, TWIM/TWIS21
           UARTE21, SPIM/SPIS22, TWIM/TWIS22, UARTE22, PDM20, PDM21
           PWM20, PWM21, PWM22, GPIOTE20, I2S20, QDEC20, QDEC21
   P2*     없다 — 이 포트는 핀마다 다르다. 아래 표를 봐라

 아래 표의 '이 핀만' 은 위 공통에 **더해지는** 것이다.

 XIAO 헤더 (14핀)
    핀  GPIO   보드 이름      variant              이 핀만
   --- ------ ---------- -------------------- ----------------------------------------
     1 P1.04  D0 / A0    PIN_A0               SAADC.AIN0, TAMPC.ASO[0]
     2 P1.05  D1 / A1    PIN_A1               RADIO.RADIO[6], SAADC.AIN1, TAMPC.ASI[0]
     3 P1.06  D2 / A2    PIN_A2               SAADC.AIN2, TAMPC.ASO[1]
     4 P1.07  D3 / A3    PIN_A3               SAADC.AIN3, TAMPC.ASI[1]
     5 P1.10  D4 / SDA   PIN_WIRE_SDA         RADIO.RADIO[1], TAMPC.ASI[2]
     6 P1.11  D5 / SCL   PIN_WIRE_SCL         RADIO.RADIO[2], SAADC.AIN4, TAMPC.ASO[3]
     7 P2.08  D6 / TX                         SPIM/SPIS00.SDO, UARTE00.TXD, SPIM/SPIS21.SDO, UARTE21.TXD
     8 P2.07  D7 / RX                         SPIM/SPIS00.DCX, UARTE00.RXD, SPIM/SPIS21.DCX, UARTE21.RXD
     9 P2.01  D8 / SCK   PIN_SPI_SCK          SPIM/SPIS00.SCK, SPIM/SPIS20.SCK, sQSPI.SCK
    10 P2.04  D9 / MISO  PIN_SPI_MISO         SPIM/SPIS00.SDI, UARTE00.CTS, SPIM/SPIS20.SDI, UARTE20.CTS, sQSPI.D1
    11 P2.02  D10 / MOSI PIN_SPI_MOSI         SPIM/SPIS00.SDO, UARTE00.TXD, SPIM/SPIS20.SDO, UARTE20.TXD, sQSPI.D0


 baram-nrf54l-arduino - MIT license
*********************************************************************/

void setup()
{
  Serial.begin(115200);
  delay(300);

  /* 이 스케치는 위 주석이 본체다. 굽지 않아도 된다.
     굳이 돌리면 보드 이름과 실장 칩만 확인해 준다. */
  Serial.println("XIAO_nRF54L15 핀맵 (nRF54L15)");
  Serial.print("빌드된 보드: ");
  Serial.println(BOARD_NAME);

  uint32_t part = *(volatile uint32_t *) 0x00FFC31C;   /* FICR INFO.PART */
  Serial.print("실장 칩 FICR INFO.PART = 0x");
  Serial.println(part, HEX);
}

void loop()
{
}
