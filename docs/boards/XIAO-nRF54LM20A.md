# XIAO nRF54LM20A / Sense (Seeed)

**칩: nRF54LM20A, 패키지 FCCSP98 (3.67×3.85mm).**
회로도 `XIAO_nRF54LM20A_Schematic.pdf` V1.0 (2026-04-09, KiCad) 기준.

> ⚠ **이 보드는 아직 variant 가 없다.** M6(LM20A 확장) 항목이다.
> 지금 이 문서는 **핀맵 표의 출처**로만 쓰인다
> (`libraries/PinMap/examples/pinmap_XIAO_nRF54LM20A`).

---

## 0. 출처와 교차 확인

세 소스를 대조했고 **D 번호·LED·버튼·AIN 배정이 전부 일치**한다.

| 소스 | 무엇을 확인했나 |
|---|---|
| 회로도 PDF | 넷 이름 (`P1.00/A0/D0` 형태), 온보드 부품 |
| Zephyr `boards/seeed/xiao_nrf54lm20a/` | `seeed_xiao_connector.dtsi` 의 D0~D27 gpio-map, LED/버튼 |
| Pin Planner `mcus/nrf54lm20a/fccsp98-3.67x3.85-paaa.json` | AIN 배정, 페리페럴 제약 |

**패키지 확정 근거**: 회로도의 GPIO 를 세면 P0=10 / P1=32 / P2=11 / P3=13,
합계 66. Pin Planner 의 세 패키지 중 **FCCSP98 만** 이 수와 정확히 맞는다
(CSP61 은 40, QFN52 는 32이고 **QFN52 에는 P3 가 아예 없다**).

---

## 1. NU54-DK / XIAO nRF54L15 와 다른 점 (먼저 볼 것)

| | XIAO nRF54L15 | **XIAO nRF54LM20A** |
|---|---|---|
| GPIO 포트 | P0 P1 P2 | P0 P1 P2 **P3** |
| 사용자 LED | 1개, **P2.00 (PWM 불가)** | **3개, 전부 P1 → PWM 된다** |
| 헤더 핀 | D0~D10 (14핀) | **D0~D27** |
| SERIAL 인스턴스 | ~22 | **~24** (SPIM/TWIM/UARTE 23·24 추가) |
| PMIC | 없음 | **nPM1300** (충전·레귤레이터) |
| 온보드 플래시 | 없음 | **PY25Q64** (8MB QSPI, P2.00~P2.05) |

**가장 중요한 차이는 P3 다.** P3 는 **도메인 20 의 페리페럴을 P1 과 공유**한다 —
SPIM/TWIM/UARTE 20~24, PWM20~22, GPIOTE20 이 P3 에도 닿는다.
즉 L15 의 "도메인당 포트 하나" 규칙이 LM20A 에서는 **"도메인 20 이 P1 과 P3 를 소유"**
로 넓어진다 (`docs/PERIPHERAL-PINMAP.md` §5).

단 **P3 에 없는 것**도 있다 — SAADC·PDM·TDM·QDEC 는 P1 전용이다.

---

## 2. 확장 헤더 — D 번호 기준

> ⚠ **아래 "핀" 은 D 번호이지 커넥터의 물리 핀 위치가 아니다.**
> 회로도의 커넥터 심볼이 15~23번 핀을 이름 없이 두고 있어 물리 배열을
> 텍스트에서 복원하지 못했다. 실물 실크스크린으로 확인할 것.
> D 번호 ↔ GPIO 대응은 회로도와 Zephyr 가 일치하므로 확실하다.

### XIAO 헤더 (D0~D27)

| 핀 | 이름 | GPIO | | 핀 | 이름 | GPIO |
|---|---|---|---|---|---|---|
| 0 | D0 / A0 | P1.00 | | 14 | D14 | P3.03 |
| 1 | D1 / A1 | P1.31 | | 15 | D15 | P3.04 |
| 2 | D2 / A2 | P1.30 | | 16 | D16 | P3.05 |
| 3 | D3 / A3 | P1.29 | | 17 | D17 | P3.06 |
| 4 | D4 / SDA / A7 | P1.03 | | 18 | D18 | P3.07 |
| 5 | D5 / SCL / A8 | P1.07 | | 19 | D19 | P0.00 |
| 6 | D6 / TX | P1.08 | | 20 | D20 | P0.01 |
| 7 | D7 / RX | P1.09 | | 21 | D21 | P0.02 |
| 8 | D8 / SCK / A6 | P1.04 | | 22 | D22 | P0.03 |
| 9 | D9 / MISO / A5 | P1.05 | | 23 | D23 | P0.04 |
| 10 | D10 / MOSI / A4 | P1.06 | | 24 | D24 | P0.05 |
| 11 | D11 | P3.00 | | 25 | D25 | P3.09 |
| 12 | D12 | P3.01 | | 26 | D26 | P3.10 |
| 13 | D13 | P3.02 | | 27 | D27 | P3.11 |

⚠ **`A8` 은 아날로그 입력이 아니다.** 넷 이름이 `P1.07/SCL/A8/D5` 라 A8 처럼
보이지만, P1.07 에 배정된 것은 `SAADC.EXTREF`(외부 기준전압)이지 AIN 이 아니다.
`analogRead(A8)` 에 해당하는 채널이 없다. 회로도 MCU 심볼도 이 핀을
`P1.07/EXTREF` 로 적는다.

**실제 AIN 은 다섯뿐이다**: A0=P1.00(AIN0), A1=P1.31(AIN1), A2=P1.30(AIN2),
A3=P1.29(AIN3), A4=P1.06(AIN4), A5=P1.05(AIN5), A6=P1.04(AIN6), A7=P1.03(AIN7).
여덟 개다 — 다만 A4~A7 은 SPI/I2C 와 핀을 공유하므로 동시에 못 쓴다.

---

## 3. 온보드 부품

### LED — 3개, **active LOW**, 전부 P1

| | GPIO | 비고 |
|---|---|---|
| Red | P1.22 | |
| Blue | P1.23 | |
| Green | P1.24 | |

P1 이므로 **셋 다 `analogWrite` 와 `attachInterrupt` 가 된다.**
XIAO nRF54L15 는 LED 가 P2.00 하나뿐이라 둘 다 안 됐다 — 이 보드는 그 제약이 없다.
Zephyr 보드 정의도 `pwmleds` 로 세 개를 잡아 둔다.

### 버튼

`USR_KEY` = **P0.09**. Zephyr 가 `GPIO_PULL_UP | GPIO_ACTIVE_LOW` 로 잡는다.
P0 이므로 GPIOTE30 으로 인터럽트가 된다.

### 센서 (Sense 모델)

| | GPIO |
|---|---|
| IMU SCL | P0.07 |
| IMU SDA | P0.08 |
| IMU INT1 | P0.06 |
| IMU CS | P3.12 |
| MIC CLK | P1.13 |
| MIC DATA | P1.14 |

IMU 가 P0 이므로 TWIM30 이다 (`Wire1` 자리).

### PMIC — nPM1300

| | GPIO |
|---|---|
| PMIC SCL | P1.17 |
| PMIC SDA | P1.18 |
| npm_GPIO0 | P1.25 |
| npm_GPIO1 | P1.26 |

⚠ Zephyr 는 이 버스를 **`gpio-i2c`(비트뱅잉)** 로 잡는다. 우리 코어에서는
그대로 못 쓴다 — **비트뱅잉은 지원하지 않는다** (CLAUDE.md §7 F6).
TWIM 인스턴스로 붙일 수 있는지는 M6 에서 판단한다. P1 이므로 TWIM20~24 가 후보다.

### 온보드 QSPI 플래시 — PY25Q64 (8MB)

`sQSPI` 가 쓸 수 있는 핀이 **P2.00~P2.05 여섯 개뿐**이고 회로도가 그 여섯 넷
(`SPI_CLK` `SPI_CS` `SPI_IO0`~`IO3`)을 쓰므로 배정은 이렇게 확정된다:

| 신호 | GPIO |
|---|---|
| SCK | P2.01 |
| CSN | P2.05 |
| IO0 | P2.02 |
| IO1 | P2.04 |
| IO2 | P2.03 |
| IO3 | P2.00 |

⚠ **핀별 결속은 Pin Planner 의 `sQSPI` 제약에서 유도한 것이고, 회로도 텍스트에서
직접 읽은 것이 아니다.** 여섯 핀이 플래시에 쓰인다는 사실은 확실하지만, 어느 넷이
어느 볼인지는 도면을 눈으로 봐야 확정된다. **M6 에서 확인할 것.**

### 디버그 / UART

온보드 **ATSAMD11D14A** 가 CMSIS-DAP + USB CDC 를 겸한다 (XIAO nRF54L15 와 같은 구조).

| | GPIO |
|---|---|
| nRF54_TX (→ SAMD11) | P1.11 |
| nRF54_RX (← SAMD11) | P1.10 |

P1 이므로 UARTE20~24 를 쓸 수 있다.

### 그 밖

| | GPIO |
|---|---|
| NFC1 / NFC2 | P1.01 / P1.02 |
| LFXO XL1 / XL2 | P1.20 / P1.21 |

32 MHz HFXO(X2, ±10ppm)와 32.768 kHz LFXO(X1, 7pF ±20ppm)가 모두 실장돼 있다.
⚠ XIAO nRF54L15 에서 외부 로드 캡이 없어 `LFXO_LOAD_CAP_FF` 가 필요했던 것과
같은 문제가 있는지는 **확인하지 않았다** (CLAUDE.md §7 F12). M6 에서 실측할 것.

---

## 4. M6 에서 해야 할 것

- [ ] variant 작성. `nrf54l_domains.h` 에 **P3 = 도메인 20** 추가가 먼저다
- [ ] 헤더의 물리 핀 배열 확인 (실물 실크스크린)
- [ ] QSPI 플래시 핀 결속을 도면으로 확정
- [ ] LFXO 로드 캡 실측 (§7 F12)
- [ ] nPM1300 을 TWIM 으로 붙일 수 있는지 판단 (Zephyr 는 비트뱅잉)
- [ ] USB — LM20A 는 USBHS 가 있지만 `nrfx_usbhs` 드라이버가 없다 (CLAUDE.md §4.2)
