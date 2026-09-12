# 페리페럴 ↔ GPIO 전원 도메인 (nRF54L15)

> **nRF52 습관이 깨지는 지점이다.** nRF52 는 PSEL 로 아무 GPIO나 아무 페리페럴에
> 붙일 수 있었지만, nRF54L 은 페리페럴 인스턴스마다 속한 전원 도메인이 있고
> **그 도메인이 소유한 GPIO 포트의 핀만** 선택할 수 있다.
>
> 코어는 이 규칙을 `cores/nrf54l/nrf54l_domains.h` 의 `static_assert` 로 강제한다.
> 잘못 배정하면 **빌드가 실패한다.**

---

## 0. 먼저 알아야 할 것 — 같은 번호대는 **하나의 블록**이다

도메인 규칙보다 이게 먼저다. **`SPIM`/`SPIS`/`TWIM`/`TWIS`/`UARTE` 는 번호가 같으면
같은 하드웨어 블록이고 모드만 다르다. 동시에 쓸 수 없다.**

MDK 의 베이스 주소가 그대로 말해 준다 (`nrf54l15_global.h`):

| 베이스 | 같은 블록 | 포트 |
|---|---|---|
| `0x5004A000` | SPIM00, SPIS00, UARTE00 | P2 |
| `0x500C6000` | SPIM20, SPIS20, **TWIM20**, TWIS20, UARTE20 | P1 |
| `0x500C7000` | SPIM21, SPIS21, **TWIM21**, TWIS21, UARTE21 | P1 |
| `0x500C8000` | SPIM22, SPIS22, **TWIM22**, TWIS22, UARTE22 | P1 |
| `0x50104000` | SPIM30, SPIS30, **TWIM30**, TWIS30, UARTE30 | P0 |

**여기서 나오는 두 가지 결론:**

1. `Serial` 이 UARTE30 이면 그 보드에서 **TWIM30 / SPIM30 은 쓸 수 없다.**
   실제로 NU54-DK variant 가 `Serial`(UARTE30)과 `Wire`(TWIM30)를 함께 잡고
   있었다 — M2 에서 `Wire` 를 붙이는 순간 `Serial` 이 죽었을 구성이다
2. **TWIM00 은 존재하지 않는다.** 즉 **P2 에는 I2C 를 놓을 수 없다**

> 벡터 이름이 `UARTE30_IRQHandler` 가 아니라 **`SERIAL30_IRQHandler`** 인 것도
> 같은 이유다 (§7 F10 ③). 하나의 SERIAL 블록이기 때문이다.

새 보드의 핀을 배정할 때는 **먼저 이 표로 블록 충돌을 확인하고**, 그다음 아래
도메인 규칙으로 포트를 확인한다.

---

## 1. 규칙

**인스턴스 번호의 첫 자리가 도메인이고, 도메인마다 GPIO 포트를 하나씩 소유한다.**

| 인스턴스 | 도메인 | GPIO 포트 | 성격 |
|---|---|---|---|
| `x00` | 00 | **P2** | 고속 |
| `x20` `x21` `x22` | 20 | **P1** | 메인 |
| `x30` | 30 | **P0** | 상시 전원 |

외우기 쉽다: `UARTE30` → P0, `TWIM20` → P1, `SPIM00` → P2.

### ⚠ 예외 — SPIM/SPIS 20·21 과 UARTE 20·21 은 **P2 의 특정 핀**을 쓸 수 있다

위 표가 기본 규칙이지만 전부는 아니다. Nordic 핀 계획 가이드의 원문:

> Rule 1: "Generally, peripherals must use pins in their own power domain."
>
> "**Selected pins on P2 can also be used by certain serial interfaces
> (SPIS, UARTE) located in PERI**, although this configuration is less
> power-efficient."

**"selected pins" 가 어느 핀인지 확정했다 (2026-09-12).** 근거는 Nordic 이 공개한
Pin Planner 앱의 SoC 정의 JSON 이다 (§4). 문서 사이트는 스크립트 접근을 막지만
이 저장소는 GitHub 이라 그대로 읽을 수 있다.

| 인스턴스 | 신호 | 쓸 수 있는 P2 핀 |
|---|---|---|
| **SPIM/SPIS20** | SCK / SDO / SDI / CS / DCX | P2.01 / P2.02 / P2.04 / P2.05 / P2.00 |
| **SPIM/SPIS21** | SCK / SDO / SDI / CS / DCX | P2.06 / P2.08 / P2.09 / P2.10 / P2.07 |
| **UARTE20** | TXD / RXD / CTS / RTS | P2.02 / P2.00 / P2.04 / P2.05 |
| **UARTE21** | TXD / RXD / CTS / RTS | P2.08 / P2.06 / P2.09 / P2.10 |

**밝혀진 것 셋:**

1. **가이드 원문의 "SPIS" 는 좁다 — SPIM 도 된다.** JSON 이 `SPIM/SPIS20` 을 한
   묶음으로 정의하고 P2 핀을 허용한다. `chcbaram/nu54dk` 펌웨어가 SD 를
   `&spi20`(마스터)으로 P2 에서 쓰는 것이 **규격에 맞는 구성**이었다
2. **22 번은 예외가 아니다.** SPIM/SPIS22·UARTE22 는 P1 전용이다
3. **I2C 에는 예외가 없다.** TWIM/TWIS 는 20·21·22·30 전부 자기 도메인 포트만
   쓴다. TWIM00 이 없다는 사실과 합치면 **P2 에 I2C 는 어떤 방법으로도 불가능**하다

고속 도메인 SPIM00 의 P2 핀은 이렇다 (전부 `driveStrengthRequirement: extra high`):

| 신호 | 핀 |
|---|---|
| SCK / SDO / SDI / CS / DCX | P2.01·P2.06 / P2.02·P2.08 / P2.04·P2.09 / P2.05·P2.10 / P2.00·P2.07 |

우리 variant 의 SPI 배정(SCK P2.01 / MOSI P2.02 / MISO P2.04)이 여기 들어맞고
실기에서도 동작한다 (`docs/STATUS.md` §2.12).

⚠ **L05 와 L15 의 제약은 완전히 동일하다** — Pin Planner 의 두 정의를 프로그램으로
비교해 확인했다 (페리페럴 31개, 차이 없음). 같은 다이의 비닝이라는 것과 일치한다.

`nrf54l_domains.h` 는 기본 규칙을 강제하고, 예외를 쓰는 보드는 **전용 매크로로
명시**한다 (`NRF54L_ASSERT_PERI_SERIAL_PIN`). 그래야 예외가 어디서 쓰이는지
코드에서 바로 보인다.

이 규칙의 다른 조항도 함께 적어 둔다:

> Rule 2: "Some peripherals with clock signals (like SPI, TWI, and TRACE) require
> the use of specific dedicated clock pins."
>
> Rule 4: 전용 핀만 쓰는 페리페럴 — FLPR, SPIM00/UARTE00, GRTC, TAMPC, NFC,
> RADIO direction-finding.

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
| `Wire` | **TWIM22** | **P1.11 SDA / P1.12 SCL** | 20 → P1 ✅ |
| `SPI` | SPIM00 | P2.01 SCK / P2.02 MOSI / P2.04 MISO / P2.05 SS | 00 → P2 ✅ |
| `analogRead` | SAADC | A0~A7 = P1.04~07, P1.11~14 | 20 → P1 ✅ |
| `analogWrite` | PWM20~22 | P1.xx (M2 에서 배정) | 20 → P1 |
| `attachInterrupt` | GPIOTE20 / GPIOTE30 | P1 / P0 — **P2 는 불가** | — |

### XIAO nRF54L15 / Sense

근거: `docs/boards/XIAO-nRF54L15.md`. 회로도와 Zephyr 보드 정의가 일치한다.

| Arduino 기능 | 인스턴스 | 핀 | 도메인 |
|---|---|---|---|
| `Serial` (온보드 USB CDC) | UARTE20 | P1.09 TX / P1.08 RX | 20 → P1 ✅ |
| `Wire` (헤더 D4/D5) | TWIM22 | P1.10 SDA / P1.11 SCL | 20 → P1 ✅ |
| `Wire1` (온보드 IMU) | TWIM30 | P0.04 SDA / P0.03 SCL | 30 → P0 ✅ |
| `SPI` (헤더 D8/D9/D10) | SPIM00 | P2.01 SCK / P2.02 MOSI / P2.04 MISO | 00 → P2 ✅ |
| `analogRead` | SAADC | A0~A3 = P1.04~07 | 20 → P1 ✅ |
| PDM 마이크 | PDM20 | P1.12 CLK / P1.13 DIN | 20 → P1 ✅ |
| (미배정) `Serial1` 후보 | UARTE21 | P2.08 TX / P2.07 RX | 21 → P2 ✅ **예외로 합법** (§1) |

### NU54-DK 의 `Wire` 는 왜 P1 인가

**처음에는 P0.02 / P0.03 에 두었다. 그건 틀렸다.**

그 핀들이 비어 있는 것은 맞다 — CP2102N 의 RTS/CTS 인데 흐름제어를 쓰지 않기로
했다 (`cores/nrf54l/Uart.h`). 하지만 P0 는 도메인 30 이고, **TWIM30 은 `Serial` 이
쓰는 UARTE30 과 같은 블록이다** (§0). 둘을 동시에 켤 수 없다.

P2 는 대안이 못 된다 — **TWIM00 이 아예 없다.**

그래서 P1 뿐이고, P1 은 15핀이 이미 빽빽하다 (LFXO 2, NFC 2, AIN 8, 버튼 2, LED 2).
남은 것 중 **다른 기능과 겹치지 않는 유일한 쌍이 P1.11 / P1.12** 다.

⚠ **대가: `Wire` 를 쓰면 A4 / A5 를 못 쓴다.** 같은 핀이다.
아날로그가 더 중요한 스케치라면 `Wire` 를 쓰지 않으면 된다 (핀은 겹치지만
동시에 켜지 않으면 문제없다).

> XIAO nRF54L15 는 이 제약이 없다. `Serial` 이 UARTE20(`0x500C6000`)이고
> `Wire` 가 TWIM22(`0x500C8000`)라 블록이 다르다. Seeed 가 잘 배정했다.

---

## 4. P2 의 정확한 핀 배정 — ✅ 확정 (2026-09-12)

오래 열려 있던 두 질문 **(a) P2 예외가 적용되는 핀이 어디인가**,
**(b) 도메인 안에서 어느 핀이 어느 신호로 갈 수 있는가** 가 Pin Planner 의
SoC 정의(§7)로 **둘 다 닫혔다.** PS PDF 를 뒤질 필요가 없었다.

### P0 · P1 은 포트 전체다

`allowedGpio` 가 `P0*` / `P1*` 로 적혀 있다. **그 포트면 어느 핀이든 된다.**
핀별 제약이 없다. 그래서 variant 의 P1 핀 배정은 도메인만 맞으면 자유다.

### P2 는 신호마다 후보가 **두 개씩**이다

전체 표는 §1 에 있다. 여기서는 형태만 말한다 — **"P2 아무 핀이나" 가 아니다.**
`SPIM00.SCK` 는 P2.01 과 P2.06 **둘뿐**이고, 다른 P2 핀을 주면 조용히 동작하지 않는다.

⚠ **P2.03 은 `sQSPI.D2` 전용이라 우리에겐 GPIO 전용 핀이다.** SPIM 도 UARTE 도
닿지 않는다. P2 가 연속이라고 넘겨짚기 쉬운 자리다.

### 인터럽트·PWM·ADC 는 P2 에 아예 없다

| | 되는 포트 |
|---|---|
| `GPIOTE20` | **P1 전용** (채널 8) |
| `GPIOTE30` | **P0 전용** (채널 4) |
| `PWM20/21/22` | **P1 전용** |
| `SAADC` `AIN0~7` | **P1.04~P1.07 / P1.11~P1.14 고정** |

**P2 를 담당하는 GPIOTE 도 PWM 도 없다.** 그래서 P2 핀에는
`attachInterrupt()` 도 `analogWrite()` 도 걸 수 없다. 하드웨어가 없는 것이지
소프트웨어가 안 해 준 것이 아니다.

⚠ **NU54-DK 의 `LED_BUILTIN`(`PIN_LED1` = P2.09)과 `PIN_LED3`(P2.07)가 여기 걸린다.**
그 보드에서 PWM 이나 핀 인터럽트를 시험하려면 LED2(P1.10) 나 LED4(P1.14) 를 써라.

### 빌드에서 막는다

표를 읽어 주기를 기대하지 않는다. `cores/nrf54l/nrf54l_pinmap.h` 가 같은 제약을
매크로로 갖고 있고, variant 가 배정을 거기에 걸어 둔다.

```c
NRF54L_ASSERT_SIG(PIN_SPI_SCK, SPIM00_SCK, "SPI SCK");
```

P2.03 을 주면 이렇게 멈춘다:

```
error: static assertion failed: SPI SCK : SPIM/SPIS00.SCK 는 P2.01, P2.06 만 된다
```

`nrf54l_domains.h` 의 포트 단위 검사보다 촘촘하다. 포트만 보면 P2.03 이 통과한다.

⚠ **핀은 매크로로 넘겨야 한다.** `static const uint8_t D6` 같은 Arduino 관용
별칭은 **C 에서 상수식이 아니라** `_Static_assert` 에 못 들어간다. variant.h 는
코어의 `.c` 에서도 include 되므로 C 로도 컴파일된다.

### 표를 다시 볼 때

`libraries/PinMap/` 의 예제 주석에 칩별·보드별 전체 표가 들어 있다.
IDE 의 **파일 → 예제 → PinMap** 에서 바로 열린다. 그 표는
`extras/gen_pinmap.py` 가 Pin Planner JSON 에서 굽는다 — 손으로 고치지 마라.

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

---

## 7. 이 표의 출처 — Pin Planner 앱의 SoC 정의

**https://github.com/NordicPlayground/PinPlanner**

Nordic 이 공개한 핀 계획 웹 앱인데, **핀↔페리페럴 제약이 JSON 으로 들어 있다.**
문서 사이트(`docs.nordicsemi.com`)와 DevZone 은 스크립트 접근을 403 으로 막지만
(`docs/DATASHEETS.md`), 이 저장소는 GitHub 이라 그냥 받을 수 있다.
**핀 제약을 확인할 일이 생기면 PDF 를 찾기 전에 여기부터 봐라.**

```sh
curl -sL https://raw.githubusercontent.com/NordicPlayground/PinPlanner/main/mcus/nrf54l15/qfn48-6x6-qfaa.json
```

| 경로 | 내용 |
|---|---|
| `mcus/<soc>/<package>.json` | 핀 목록(`pins`)과 페리페럴별 허용 핀(`socPeripherals`) |
| `mcus/<soc>/devicetree-templates.json` | Zephyr DTS 조각 |
| `devkits/*.json` | Nordic DK 보드 정의 |

`socPeripherals[].signals[].allowedGpio` 가 핵심이다. `P1*` 는 그 포트 전체,
`P2.01` 처럼 적힌 것은 그 핀만 가능하다는 뜻이다.
`driveStrengthRequirement` 도 함께 들어 있다 (SPIM00 은 `extra high`).

지원 SoC: nRF54L05 / L10 / L15 / LM20A / LS05A·B / LV10A — **M6 의 LM20A 까지 덮는다.**
