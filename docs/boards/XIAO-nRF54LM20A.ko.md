# XIAO nRF54LM20A / Sense (Seeed)

*[English](XIAO-nRF54LM20A.md) · [한국어](XIAO-nRF54LM20A.ko.md)*

**칩: nRF54LM20A, 패키지 FCCSP98 (3.67×3.85mm).**
회로도 `XIAO_nRF54LM20A_Schematic.pdf` V1.0 (2026-04-09, KiCad) 기준.

| | |
|---|---|
| variant | `xiao_nrf54lm20a` — FQBN `baram-nrf54:nrf54l:xiao_nrf54lm20a` |
| 보드 라이브러리 | `libraries/BOARD-XIAO-nRF54LM20A` — nPM1300 `PMIC`, Sense IMU `IMU`, 예제 5개 |
| 메모리 배치 | `docs/MEMORY-MAP.md` 의 nRF54LM20A 절 |
| 실기 기록 | `docs/HIL/M6-xiao-nrf54lm20a.md` (2026-09-14) |

> ⚠ **칩 안테나가 없다.** RF 는 u.FL 커넥터로만 나간다. 외부 안테나 없이는 BLE 가
> **정상으로 돌면서 아무에게도 들리지 않는다** (§3 RF).
>
> ⚠ **디버그 포트가 잠긴 채 출하된다 (APPROTECT).** 처음 한 번은
> `Erase all + Burn SoftDevice (probe-rs)` 로 굽는다. Seeed 기본 펌웨어는 지워진다.
>
> ⚠ **Sense 센서 전원은 GPIO 가 아니라 nPM1300 LDO1 이다** (§3 PMIC).

Sense 모델과 일반 모델은 **PCB 가 같고** IMU·마이크 실장 여부만 다르다
(블록도에서 두 부품이 점선 박스로 그려져 있다). 보드 항목 하나를 같이 쓴다.
일반 모델에서는 `IMU.begin()` 이 0 을 돌려준다.

---

## 0. 출처와 교차 확인

| 소스 | 무엇을 확인했나 |
|---|---|
| 회로도 PDF | 넷 이름 (`P1.00/A0/D0` 형태), 온보드 부품, 전원 레일, 헤더 물리 배열 |
| Zephyr `boards/seeed/xiao_nrf54lm20a/` | D0~D27 gpio-map, LED/버튼, 버스 배정(uart20/21, i2c22/30, spi23), 크리스털 캡 |
| sdk-nrf-bm v2.0.1 `boards/nordic/bm_nrf54lm20dk/` | 메모리 파티션, 크리스털 캡 (Zephyr 와 같은 값) |
| Pin Planner `mcus/nrf54lm20a/fccsp98-3.67x3.85-paaa.json` | AIN 배정, 페리페럴 제약 |

회로도·Zephyr·Pin Planner 가 **D 번호·LED·버튼·AIN 배정까지 전부 일치**한다.

**패키지 확정 근거**: 회로도의 GPIO 를 세면 P0=10 / P1=32 / P2=11 / P3=13,
합계 66. Pin Planner 의 세 패키지 중 **FCCSP98 만** 이 수와 정확히 맞는다
(CSP61 은 40, QFN52 는 32이고 **QFN52 에는 P3 가 아예 없다**).
실물 FICR `INFO.PACKAGE` 도 `"PA"` 다.

---

## 1. XIAO nRF54L15 와 다른 점 (먼저 볼 것)

| | XIAO nRF54L15 | **XIAO nRF54LM20A** |
|---|---|---|
| GPIO 포트 | P0 P1 P2 | P0 P1 P2 **P3** |
| RRAM / RAM | 1.5 MB / 256 KB | **2 MB / 512 KB** (RAM 끝 `0x2007FD40`) |
| 안테나 | 온보드 + u.FL, RF 스위치 | **u.FL 뿐** |
| `Serial` 핀 | P1.08 / P1.09 | **P1.11 / P1.10** (P1.08/09 는 헤더 D6/D7) |
| 사용자 LED | 1개, **P2.00 (PWM 불가)** | **RGB 3개, 전부 P1 → PWM·인터럽트 된다** |
| 헤더 핀 | D0~D10 (14핀) | **D0~D27** (D11~D18 은 뒷면 패드) |
| SERIAL 인스턴스 | ~22 | **~24** (SPIM/TWIM/UARTE 23·24 추가) |
| PMIC | 없음 | **nPM1300** (충전·레귤레이터) |
| Sense 센서 전원 | GPIO 로드 스위치 | **PMIC LDO1** |
| 온보드 플래시 | 없음 | **PY25Q64** (8 MB, P2.00~P2.05) |
| 출하 상태 | 굽기 가능 | **APPROTECT 잠김** |

**가장 중요한 칩 차이는 P3 다.** P3 는 **도메인 20 의 페리페럴을 P1 과 공유**한다 —
SPIM/TWIM/UARTE 20~24, PWM20~22, GPIOTE20 이 P3 에도 닿는다.
L15 의 "도메인당 포트 하나" 규칙이 LM20A 에서는 **"도메인 20 이 P1 과 P3 를 소유"**
로 넓어진다 (`docs/PERIPHERAL-PINMAP.md` §5). 코어는 `NRF54L_IS_DOMAIN20_PORT()` 로 처리한다.

단 **P3 에 없는 것**도 있다 — SAADC·PDM·TDM·QDEC 는 P1 전용이다.

---

## 2. 확장 헤더

### 물리 배열 — 커넥터 U7 (XIAO-Add-On-Plus)

```
 1  D0   P1.00        14  VBUS
 2  D1   P1.31        13  GND
 3  D2   P1.30        12  3V3_OUT
 4  D3   P1.29        11  D10  P1.06
 5  D4   P1.03        10  D9   P1.05
 6  D5   P1.07         9  D8   P1.04
 7  D6   P1.08         8  D7   P1.09

15  D19  P0.00        23  D27  P3.11
16  D20  P0.01        22  D26  P3.10
17  D21  P0.02        21  D25  P3.09
18  D22  P0.03
19  D23  P0.04
20  D24  P0.05
```

1~14 는 표준 XIAO 배열 그대로이고, 15~23 이 Add-On-Plus 확장이다.
⚠ **D11~D18 (P3.00~P3.07) 은 헤더가 아니라 뒷면 테스트 포인트 TP11~TP18** 이다.
3V3_OUT 은 칩 전원(VSYS_3V3)이 아니라 별도 DC/DC(TPS62843) 출력이다.

### D 번호 ↔ GPIO

| D | 이름 | GPIO | | D | 이름 | GPIO |
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
회로도 MCU 심볼도 이 핀을 `P1.07/EXTREF` 로 적는다. variant 는 `PIN_A8` 을 두지 않는다.

**실제 AIN 은 여덟이다**: A0=P1.00(AIN0), A1=P1.31(AIN1), A2=P1.30(AIN2),
A3=P1.29(AIN3), A4=P1.06(AIN4), A5=P1.05(AIN5), A6=P1.04(AIN6), A7=P1.03(AIN7).
A4~A7 은 SPI/I2C 와 핀을 공유하므로 동시에 못 쓴다.

---

## 3. 온보드 부품

### 버스 배정 (variant)

| Arduino | 인스턴스 | 핀 | 비고 |
|---|---|---|---|
| `Serial` | UARTE20 | TX P1.11 / RX P1.10 | 온보드 SAMD11 → USB CDC |
| `Serial1` | UARTE21 | TX P1.08(D6) / RX P1.09(D7) | 헤더 |
| `Wire` | TWIM22 | SDA P1.03(D4) / SCL P1.07(D5) | 헤더 |
| `Wire1` | TWIM30 | SDA P0.08 / SCL P0.07 | Sense IMU |
| `SPI` | **SPIM23** | SCK P1.04 / MISO P1.05 / MOSI P1.06 | 헤더. 도메인 20 이라 최대 8 Mbps |
| `SPI1` | SPIM00 | SCK P2.01 / MOSI P2.02 / MISO P2.04 | 온보드 플래시 |
| PMIC 버스 | **TWIM24** | SCL P1.17 / SDA P1.18 | 보드 라이브러리가 쓴다 |

번호가 전부 달라 같은 하드웨어 블록을 겹쳐 쓰지 않는다 (`docs/PERIPHERAL-PINMAP.md` §0).
Zephyr 보드 정의와 같은 배정이고, PMIC 버스만 다르다 (Zephyr 는 비트뱅잉).

### RF — 외부 안테나 필수

칩의 ANT 출력이 정합 회로(L5·L6·L7, C38·C39)와 0 Ω(R25)을 지나 **ANT2(u.FL 커넥터)** 로만
나간다. 칩 안테나도 RF 스위치도 없다. 블록도의 RF 쪽에도 "IPEX" 만 그려져 있다.

**실측 (2026-09-14)**:

| | 결과 |
|---|---|
| 안테나 없음 | SoftDevice 광고 `start()`·`isRunning()` 정상, SWD 로 `RADIO.STATE` 가 TX, `FREQUENCY` 가 2402/2480 MHz 를 오감. **그런데 바로 옆 Mac 이 20초 동안 광고 746건 중 이 보드를 0건 수신** |
| 안테나 연결 | 스캔 0.4 초, **RSSI −31 dBm**, 연결·MTU 247·NUS 에코 성공 |

증상이 소프트웨어 쪽에서는 전부 정상이라 **코어 문제로 오진하기 쉽다.** BLE 가
안 들리면 안테나부터 확인한다.

### LED — RGB 1개, **active LOW**, 전부 P1

공통 애노드 RGB LED(LED1)의 애노드가 VSYS_3V3 이고, 캐소드가 각각 2 kΩ(R30~R32)을
지나 핀으로 온다.

| | GPIO | variant |
|---|---|---|
| Red | P1.22 | `PIN_LED1` = `LED_BUILTIN` = `LED_RED` |
| Blue | P1.23 | `PIN_LED2` = `LED_BLUE` = `LED_CONN` |
| Green | P1.24 | `PIN_LED3` = `LED_GREEN` |

P1 이므로 **셋 다 `analogWrite` 와 `attachInterrupt` 가 된다** (실기 확인, `rgb_led` 예제).
`LED_BUILTIN` 을 빨강, 연결 표시를 파랑으로 둔 것은 Adafruit 보드 관례를 따른 것이다.
충전 LED(D2, 적색)는 nPM1300 이 직접 구동한다.

### 버튼

| | GPIO | |
|---|---|---|
| K2 `USR_KEY` | **P0.09** | 외부 100 kΩ 풀업(R27), TVS(D4). 눌리면 LOW |
| K1 | nRF54_RESET | MCU 에서 읽을 수 없다 |

P0 이므로 GPIOTE30 으로 인터럽트가 된다 (실기 확인, `button` 예제).

### PMIC — nPM1300

| 레일 | 출력 | 설정 | 용도 |
|---|---|---|---|
| VSYS_3V3 | **BUCK2** | VSET2 = 470 kΩ → 3.3 V | **칩·LED 전원** |
| — | BUCK1 | VSET1 = GND → 꺼짐 | 미사용 ("Do not use VOUT1") |
| IMU&MIC_3V3 | **LDO1** | 레지스터로 설정 | Sense IMU·마이크 |

| 신호 | GPIO |
|---|---|
| PMIC SCL | P1.17 |
| PMIC SDA | P1.18 |
| npm_GPIO0 | P1.25 |
| npm_GPIO1 | P1.26 |

- I2C 주소 **0x6B**. 외부 4.7 kΩ 풀업(R12/R13)이 VSYS_3V3 에 있다
- Zephyr 는 이 버스를 **`gpio-i2c`(비트뱅잉)** 로 잡지만 우리 코어는 비트뱅잉을
  지원하지 않는다 (CLAUDE.md §7 F6). P1 이라 하드웨어 TWIM 으로 되므로
  다른 버스와 겹치지 않는 **TWIM24** 를 쓴다. 실기에서 응답 확인
- **배터리 분압 회로가 없다.** 배터리 전압·충전 상태·USB 전원 유무는 PMIC 가 잰다
  (`PMIC.batteryMillivolts()`, `chargeStatus()`, `vbusPresent()`)
- ⚠ 레귤레이터 레지스터를 잘못 쓰면 **칩 자신의 전원(BUCK2)** 을 끌 수 있다.
  보드 라이브러리는 LDO1 과 ADC 만 쓰고 레지스터 쓰기를 공개하지 않는다

### 센서 (Sense 모델)

| | 부품 | GPIO |
|---|---|---|
| IMU | **LSM6DS3TR-C** @ 0x6A | SDA P0.08 / SCL P0.07 (Wire1), INT1 P0.06, CS P3.12 |
| 마이크 | MSM261DGT006 (PDM) | CLK P1.13 / DATA P1.14 (PDM20) |

⚠ **두 센서의 전원은 nPM1300 LDO1 이다.** IMU 뿐 아니라 IMU 버스의 4.7 kΩ
풀업(R28/R29)과 CS 풀업(R37, 100 kΩ)도 그 레일에 달려 있다. 그래서 LDO1 이 꺼져
있으면 **Wire1 버스 자체가 떠 있어 아무 주소도 응답하지 않는다.**

실측 (`imu` 예제): LDO1 꺼짐 → `WHO_AM_I` 무응답, LDO1 3.3 V 켬 → `0x6A`.
`IMU.begin()` 이 PMIC 로 레일을 켠다.

- **LDO1 전압은 3.3 V 로 둔다.** Zephyr 보드 정의는 1.8 V 로 켜지만, 회로도 레일 이름이
  `IMU&MIC_3V3` 이고 블록도도 "LDO 3.3V" 이며, 버스 풀업이 이 레일에 있어 3.3 V 로 도는
  nRF 가 1.8 V HIGH 를 논리 1 로 못 읽을 수 있다. 3.3 V 에서 정상 동작을 확인했다
- CS(P3.12)는 풀업으로 I2C 모드에 고정된다. ⚠ **P3.12 를 출력으로 몰지 마라.**
  레일이 꺼진 상태에서 HIGH 를 주면 IMU 가 보호 다이오드로 역급전된다
- 마이크: 코어에 PDM API 가 아직 없다

### 온보드 플래시 — PY25Q64 (8 MB)

회로도 p.6 에서 넷을 직접 확인했다 (예전에 Pin Planner `sQSPI` 제약으로 유도했던 결속과 같다):

| 신호 | GPIO |
|---|---|
| IO3 / HOLD | P2.00 |
| CLK | P2.01 |
| IO0 / MOSI | P2.02 |
| IO2 / WP | P2.03 |
| IO1 / MISO | P2.04 |
| CS | P2.05 |

100 kΩ 풀업(R42~R44)이 VSYS_3V3 에 있다. variant 는 표준 SPI 로 `SPI1`(SPIM00)에
두고 `PIN_FLASH_CS/WP/HOLD` 를 정의한다. 실측 JEDEC ID **`85 20 17`** (Puya, 2^23 B),
SFDP 서명·rev 1.0 정상 (`flash_id` 예제).

### 디버그 / UART

온보드 **ATSAMD11D14A** 가 CMSIS-DAP + USB CDC 를 겸한다 (XIAO nRF54L15 와 같은 구조).
`probe-rs list` 에 `Seeed Studio XIAO nRF54LM20A CMSIS-DAP` (`2886:0068`) 로 잡힌다.
nRF 쪽과는 UM3204H 레벨 버퍼를 거친다.

| | GPIO |
|---|---|
| nRF54_TX (→ SAMD11) | P1.11 |
| nRF54_RX (← SAMD11) | P1.10 |

SWD 는 뒷면 테스트 포인트 TP1~TP8 로도 나와 있다.

⚠ **nRF54LM20A 의 USB(D+/D-, VBUS)는 이 보드에서 연결돼 있지 않다** (회로도 K4·K5
미연결, R23 미실장). USB-C 는 SAMD11 에만 간다.

### 클럭

| | 부품 | 외부 캡 | variant |
|---|---|---|---|
| LFXO | X1 32.768 kHz, 7 pF ±20 ppm, P1.20/P1.21 | **없음** | `LFXO_LOAD_CAP_FF 17000` |
| HFXO | X2 32 MHz, 8 pF ±10 ppm | **없음** | `HFXO_LOAD_CAP_FF 15000` |

회로도에 "The internal capacitance of the matching capacitor is configurable" 이라고
적혀 있다. 값은 Zephyr XIAO 보드 정의에서 가져왔고 sdk-nrf-bm LM20 DK 도 같다.

실측: 이 개체에서 계산된 INTCAP (LFXO 23 / HFXO 40) 이 레지스터에 그대로 써졌다.
**LFXO 는 호스트 대비 −49 ppm** (5분, 151 샘플). SoftDevice 에 선언한 ±250 ppm 안이라
BLE 에는 문제없지만 크리스털 사양(±20 ppm)보다 크다. 음수 = 느리다 = 부하 용량이
크다는 뜻이므로 17000 fF 를 줄이면 들어올 가능성이 있다 (미시도).

### NFC

| | GPIO |
|---|---|
| NFC1 / NFC2 | P1.01 / P1.02 |

0 Ω(R39/R40)을 거쳐 뒷면 N1/N2 패드로 간다. 안테나는 실장돼 있지 않다.

---

## 4. 남은 것

- [ ] LFXO 로드 캡 튜닝 (위 −49 ppm)
- [ ] PDM 마이크 — 코어에 PDM API 가 없다
- [ ] 저전력 측정 (CLAUDE.md §7 F8 — 프로브 분리)
- USB — 칩에 USBHS 가 있지만 **이 보드는 배선이 없어** 해당하지 않는다

### 보드 라이브러리 예제 TODO

지금 있는 것: `pmic` `imu` `flash_id` `rgb_led` `button` (실기 확인), `i2c_scan` (컴파일만).

| | 예제 | 내용 | 준비 상태 |
|---|---|---|---|
| [ ] | `i2c_scan` 실기 확인 | Wire1 에서 IMU(0x6A)가 잡히고 `WHO_AM_I` 가 읽히는지 | 예제는 있다. 올려 보기만 하면 된다 |
| [ ] | **`ble_imu`** | IMU 값을 BLE 로 보낸다 (BLEUart 또는 커스텀 서비스) | 바로 가능. Sense + BLE 를 함께 쓰는 대표 예제가 없다 |
| [ ] | **`ble_battery`** | PMIC 배터리 전압 → `BLEBas` 잔량(%) | 바로 가능. 전압→% 는 대략 표로. HOGP 가 BAS 를 요구하는 과제(STATUS)와 이어진다 |
| [ ] | **`flash_storage`** | 온보드 8 MB 플래시에 섹터 소거 → 쓰기 → 읽기 검증 | 바로 가능. 지금은 ID 만 읽는다 |
| [ ] | `imu_wakeup` | INT1(P0.06)으로 움직임·탭을 감지해 대기에서 깨기, 센서 레일 on/off | ST 드라이버로 인터럽트 레지스터(TAP_CFG, WAKE_UP_THS, MD1_CFG 등) 확인이 먼저. 전류는 프로브를 떼고 잰다 (CLAUDE.md §7 F8) |
| [ ] | 마이크 녹음·음량 | PDM 마이크 (Sense) | **코어에 PDM API 가 먼저 필요하다** |

### IMU API TODO

| | 항목 | 이유 |
|---|---|---|
| [ ] | `temperatureSampleRate()` 추가 | Arduino_LSM6DS3 과 메서드가 완전히 같아지는 마지막 한 개 |
| [ ] | Arduino_LSM6DS3 의 필터 설정 따를지 검토 | 그쪽은 `CTRL1_XL 0x4A`(LPF1), `CTRL8_XL 0x09` 을 쓴다. 값의 잡음 특성이 달라진다 |
| [ ] | Seeed `LSM6DS3` API 호환 검토 | XIAO nRF52840 Sense 스케치(`LSM6DS3 myIMU(I2C_MODE, 0x6A)`, `readFloatAccelX()`)를 옮기려면 필요. Seeed 라이브러리는 이 보드에서 `Wire`(헤더)를 쓰고 LDO1 도 안 켜서 그대로는 안 된다. ⚠ 호환 클래스를 만들면 Seeed 라이브러리를 함께 설치한 사용자에게 `LSM6DS3.h`·`LSM6DS3` 이름이 겹친다 |
