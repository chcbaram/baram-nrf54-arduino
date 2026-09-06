# 페리페럴 ↔ GPIO 전원 도메인 (nRF54L15)

> **nRF52 습관이 깨지는 지점이다.** nRF52 는 PSEL 로 아무 GPIO나 아무 페리페럴에
> 붙일 수 있었지만, nRF54L 은 페리페럴 인스턴스마다 속한 전원 도메인이 있고
> **그 도메인이 소유한 GPIO 포트의 핀만** 선택할 수 있다.
>
> 코어는 이 규칙을 `cores/nrf54l/nrf54l_domains.h` 의 `static_assert` 로 강제한다.
> 잘못 배정하면 **빌드가 실패한다.**

---

## 1. 규칙

**인스턴스 번호의 첫 자리가 도메인이고, 도메인마다 GPIO 포트를 하나씩 소유한다.**

| 인스턴스 | 도메인 | GPIO 포트 | 성격 |
|---|---|---|---|
| `x00` | 00 | **P2** | 고속 |
| `x20` `x21` `x22` | 20 | **P1** | 메인 |
| `x30` | 30 | **P0** | 상시 전원 |

외우기 쉽다: `UARTE30` → P0, `TWIM20` → P1, `SPIM00` → P2.

### 근거

MDK 의 페리페럴 베이스 주소가 도메인별로 뭉쳐 있다
(`nrf54l15_global.h` 의 `NRF_*_S_BASE` 를 정렬하면 그대로 나온다):

```
0x50040000  AAR00 CCM00 CRACEN DPPIC00 ECB00 KMU MPC00 PPIB00 PPIB01
            RRAMC SPIM00 SPIS00 SPU00 UARTE00 VPR00
0x50050000  CTRLAP GPIOHSPADCTRL  P2  TAD TIMER00
0x50080000  DPPIC10 EGU10 PPIB10 PPIB11 RADIO SPU10 TIMER10     ← SoftDevice 전용
0x500C0000  DPPIC20 EGU20 MEMCONF PPIB20-22 SPIM20-22 SPIS20-22
            SPU20 TIMER20-24 TWIM20-22 TWIS20-22 UARTE20-22
0x500D0000  GPIOTE20 I2S20 NFCT  P1  PDM20 PDM21 PWM20-22 SAADC TAMPC TEMP
0x500E0000  GRTC QDEC20 QDEC21
0x50100000  CLOCK COMP DPPIC30 GPIOTE30 LPCOMP  P0  POWER PPIB30
            RESET SPIM30 SPIS30 SPU30 TWIM30 TWIS30 UARTE30 WDT30 WDT31
```

**회로도가 이를 세 번 독립적으로 확인해 준다:**

1. **SAADC 가 P1 도메인** → AIN0~7 이 전부 P1.04~07, P1.11~14. 다른 포트엔 아날로그 입력이 없다
2. **NFCT 가 P1 도메인** → NFC1/NFC2 가 P1.02/P1.03
3. **UARTE30 이 P0 도메인** → 콘솔 UART 가 P0.00~03 (실기 확인)

---

## 2. 도메인별 페리페럴

### 도메인 00 → **P2** (NU54-DK 에서 P2.00~P2.10 노출)

| 페리페럴 | 비고 |
|---|---|
| **SPIM00 / SPIS00** | 유일한 고속 SPI. Arduino `SPI` 가 여기 |
| UARTE00 | |
| TIMER00 | 애플리케이션이 쓸 수 있다 |
| GPIOHSPADCTRL | 고속 패드 제어. 고속 신호에 필요 |

> ⚠ 고속 신호는 `OUTPUT_H0H1` 또는 nRF54L 전용 `OUTPUT_E0E1` 드라이브가 필요할 수 있다.
> P2 고속 라우팅, HSBIAS slew, SPIM anomaly 8 워크어라운드는 CLAUDE.md §4 참조.

### 도메인 20 → **P1** (NU54-DK 에서 P1.00~P1.14 노출)

| 페리페럴 | 인스턴스 |
|---|---|
| SPIM / SPIS | 20, 21, 22 |
| TWIM / TWIS | 20, 21, 22 |
| UARTE | 20, 21, 22 |
| PWM | 20, 21, 22 |
| **SAADC** | 1개 (AIN0~7 은 반드시 P1) |
| PDM | 20, 21 |
| I2S | 20 |
| QDEC | 20, 21 |
| GPIOTE | 20 |
| NFCT | 1개 |
| TIMER | 20~24 |

### 도메인 30 → **P0** (NU54-DK 에서 P0.00~P0.04 노출)

| 페리페럴 | 비고 |
|---|---|
| UARTE30 | **`Serial`** (P0.00 TX / P0.01 RX) |
| TWIM30 / TWIS30 | **`Wire`** (P0.02 SDA / P0.03 SCL) |
| SPIM30 / SPIS30 | |
| GPIOTE30 | P0 핀의 `attachInterrupt` |
| COMP / LPCOMP | |
| WDT30 / WDT31 | |

> P0 는 상시 전원 도메인이라 저전력 상태에서도 살아 있다.
> 웨이크업 소스로 쓰기 좋다.

### GPIO 없는 것

`GRTC`(FreeRTOS 틱), `RADIO`/`TIMER10`/`EGU10`(SoftDevice 전용, `nrf_sd_def.h`),
`CRACEN`, `RRAMC`, `MEMCONF` 등.

---

## 3. 보드별 배정

핀 근거는 각 보드 문서(`docs/boards/`)에 있다. 여기서는 **도메인 규칙과 맞는지**만 본다.

### NU54-DK / NU54V-DK

| Arduino 기능 | 인스턴스 | 핀 | 도메인 |
|---|---|---|---|
| `Serial` | UARTE30 | P0.00 TX / P0.01 RX | 30 → P0 ✅ |
| `Wire` | TWIM30 | P0.02 SDA / P0.03 SCL | 30 → P0 ✅ |
| `SPI` | SPIM00 | P2.01 SCK / P2.02 MOSI / P2.04 MISO / P2.05 SS | 00 → P2 ✅ |
| `analogRead` | SAADC | A0~A7 = P1.04~07, P1.11~14 | 20 → P1 ✅ |
| `analogWrite` | PWM20~22 | P1.xx (M2 에서 배정) | 20 → P1 |
| `attachInterrupt` | GPIOTE20 / GPIOTE30 | P1·P2 / P0 | — |

### `Wire` 를 P0 에 놓은 이유

**P1 은 15핀이 전부 무언가에 배정돼 있다** — LFXO(2), NFC(2), AIN(8, 그중 2개는
버튼·LED 와 중복), 버튼(2), LED(2). 어디에 놓아도 충돌한다.

반면 P0.02 / P0.03 은 CP2102N 의 RTS/CTS 인데 **흐름제어를 쓰지 않기로 해서**
비어 있다 (`cores/nrf54l/Uart.h` 주석 참조). TWIM30 이 P0 도메인이라 그대로 쓸 수 있고,
P1 헤더의 4번·5번으로 **물리적으로도 인접**하다.

`Wire1` 이 필요하면 TWIM20~22(P1 도메인)로 별도 배정한다.

---

## 4. 아직 확인 안 된 것

도메인 규칙은 확실하지만, **도메인 안에서 어느 핀이 어느 신호로 갈 수 있는지**는
미확정이다. 예: SPIM00 의 SCK 가 P2 중 아무 핀이나 되는지, 아니면 정해진 핀만 되는지.

→ **M2 착수 시 nRF54L15 Product Specification 의 GPIO 배치표로 확정하고 실기 검증할 것.**
   결과를 이 문서 §3 에 반영한다.

`variant.h` 의 SPI/PWM 핀은 그때까지 잠정값이다 (도메인은 맞다).

---

## 5. nRF54LM20A (M6 대비)

같은 규칙이 그대로 통하는지 **아직 확인 안 됐다.** 주소 대역이 L15 와 다르게 나뉜다.

| 대역 | LM20A 구성원 |
|---|---|
| `0x50040000` | SPIM00 SPIS00 UARTE00 … |
| `0x50050000` | **P2**, TIMER00, EGU00, **USBHS**, GPIOHSPADCTRL |
| `0x500C0000` | SPIM/TWIM/UARTE **20~22**, TIMER20~24 … |
| `0x500D0000` | **P1**, **P3**, GPIOTE20, PWM20~22, SAADC, NFCT … |
| `0x500E0000` | GRTC, QDEC, **SPIM/TWIM/UARTE 23·24**, TDM |
| `0x50100000` | **P0**, SPIM30 TWIM30 UARTE30 GPIOTE30 … |

**L15 규칙이 단순 확장되지 않는 지점 두 곳:**

1. **P3 가 P1 과 같은 대역(`0x500D`)** 에 있다. 두 포트가 한 도메인을 공유하는지,
   아니면 대역과 도메인이 별개인지 확인이 필요하다
2. **SERIAL 23·24 가 GRTC 와 같은 `0x500E` 대역** 에 있다. 이들이 P3 를 쓰는지
   P1 을 쓰는지 미확정이다

→ M6 착수 시 nRF54LM20A Product Specification 의 GPIO 배치표로 확정하고
   `nrf54l_domains.h` 에 LM20A 블록을 추가한다. **추측으로 쓰지 마라.**

USB(USBHS)가 `0x50050000` 대역, 즉 **P2 와 같은 고속 도메인**에 있다는 점도 기록해 둔다.

---

## 6. 새 보드 variant 를 만들 때

`variant.h` 끝에 도메인 검증 블록을 **반드시 복사**하라.
`variants/nu54dk/variant.h` 의 "전원 도메인 검증" 절이 그것이다.

```c
NRF54L_ASSERT_DOMAIN30_PIN(PIN_SERIAL_TX, "Serial(UARTE30) TX");
NRF54L_ASSERT_SPIM00_PIN(PIN_SPI_SCK,     "SPI SCK");
NRF54L_ASSERT_ANALOG_PIN(PIN_A0,          "A0");
```

잘못 배정하면 이런 오류로 빌드가 멈춘다:

```
error: static assertion failed: SPI SCK : SPIM00/UARTE00 은 P2 도메인만 쓸 수 있다
```
