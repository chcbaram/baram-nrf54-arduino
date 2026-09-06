# XIAO nRF54L15 / XIAO nRF54L15 Sense (Seeed Studio)

회로도 `XIAO nRF54L15.kicad_sch` (Rev V0.9, 2025-05-29, CC BY-SA 4.0) 분석 결과.
MCU: **nRF54L15-CAAA-R** (WLCSP).

핀 배정은 upstream Zephyr 의 보드 정의와 **전부 대조했고 일치한다**
(`zephyrproject-rtos/zephyr` `boards/seeed/xiao_nrf54l15/`).
회로도만으로 애매한 값(LFXO 로드 용량, D6/D7 UART 인스턴스)은 그쪽이 정본이다.

| | 값 |
|---|---|
| 칩 | nRF54L15 (1.5 MB RRAM / 256 KB RAM) |
| FQBN | `baram-nrf54:nrf54l:xiao_nrf54l15` |
| variant | `xiao_nrf54l15` |
| 메모리 배치 | NU54V-DK 와 동일 → [MEMORY-MAP.md](../MEMORY-MAP.md) 의 nRF54L15 절 |

실장 칩 확인 (FICR, 실측):

```
INFO.PART    @ 0x00FFC31C  0x00054B15   nRF54L15
INFO.PACKAGE @ 0x00FFC324  0x00004341   "CA"
INFO.RAM     @ 0x00FFC328  0x00000100   256 KB
INFO.RRAM    @ 0x00FFC32C  0x000005F4   1524 KB
```

> **Sense 모델과의 차이는 IMU / PDM 마이크 실장 여부뿐이다.** 핀맵·메모리·부트
> 경로가 같으므로 보드 항목과 variant 를 공유한다. 센서가 없는 모델에서
> `PIN_SENSOR_POWER` 를 켜도 아무 일도 일어나지 않는다.

---

## 1. NU54-DK 와 다른 점 (먼저 볼 것)

| | NU54-DK | **XIAO nRF54L15** |
|---|---|---|
| 디버그 프로브 | 없음. 외부 CMSIS-DAP 필요 | **온보드 ATSAMD11D14A** |
| `Serial` | UARTE30, P0.00/P0.01, CP2102N | **UARTE20, P1.09/P1.08, 같은 SAMD11** |
| LED | 4개, **active HIGH** | 1개, **active LOW** |
| 버튼 | 4개, 외부 풀업 없음 | 1개, **외부 10K 풀업 있음** |
| LFXO 로드 캡 | 외부 13 pF (C13/C14) | **없음 → 내부 16 pF** |
| 아날로그 | A0~A7 | **A0~A3** (D4 에 AIN 이 없다) |
| 안테나 | — | **RF 스위치를 거친다.** 전원을 줘야 한다 |

---

## 2. 디버그 / 업로드 — 케이블 하나면 된다

USB-C 의 D+/D− 가 **ATSAMD11D14A(U4)** 에 직결되고, SAMD11 이
레벨 변환기 U12(UM3501D4)를 거쳐 nRF54L15 의 SWD 로 나간다.
리셋도 SAMD11 의 PA05 → `nRF54_RST_CTL` → Q2 로 제어한다.

같은 SAMD11 이 **USB CDC 시리얼 브리지**도 겸한다 (PA08/PA09 ↔ P1.08/P1.09).
즉 **업로드와 `Serial` 이 케이블 하나로 동시에 된다.** 외부 프로브가 필요 없다.

실측 확인:

```
$ probe-rs list
[0]: Seeed Studio XIAO nrf54 CMSIS-DAP -- 2886:0066-1:5784477E (CMSIS-DAP)

시리얼 포트: /dev/cu.usbmodem<시리얼>3
업로드: 38 KB hex 쓰기 + verify 1.9 초
```

### 뒷면 테스트 포인트

| TP | 신호 | TP | 신호 |
|---|---|---|---|
| TP1 | nRF SWCLK | TP5 | VSYS_3V3 |
| TP2 | nRF SWDIO | TP6 | SAMD11 RESET |
| TP3 | GND | TP7 | SAMD11 SWCLK |
| TP4 | nRF RESET | TP8 | SAMD11 SWDIO |

TP1~TP4 로 **외부 프로브를 직접 붙일 수 있다.** SAMD11 펌웨어가 깨져도
복구 경로가 있다는 뜻이다. TP6~TP8 은 SAMD11 자체를 다시 굽는 경로다.

---

## 3. 핀 배정

### LED — **active LOW** (NU54-DK 와 반대)

| 부품 | 핀 | 회로 |
|---|---|---|
| D3 녹색 (USR_LED) | **P2.00** | `VSYS_3V3 ─ R15 1.5K ─ D3 ─ P2.00` |
| D2 적색 (Charge) | — | 충전 IC 가 직접 구동. **MCU 가 제어할 수 없다** |

핀이 캐소드 쪽이므로 LOW 로 켠다 → `LED_STATE_ON = 0`.
Zephyr 도 `gpios = <&gpio2 0 GPIO_ACTIVE_LOW>` 로 같다.

### 버튼 — 외부 풀업 있음

| 부품 | 핀 | 비고 |
|---|---|---|
| K2 (USR_KEY) | **P0.00** | R13 10K 풀업 + C24 100nF. active LOW |
| K1 | RESET | R12 10K 풀업. MCU 에서 읽을 수 없다 |

NU54-DK 와 달리 외부 풀업이 있어서 `INPUT` 만으로도 된다.
variant 는 그래도 `INPUT_PULLUP` 으로 잡는다 (병렬이라 무해).

### UART — 온보드 SAMD11 경유

| nRF54L15 | 방향 | SAMD11 |
|---|---|---|
| **P1.09** | TX → | PA09 (`SAMD11_RX`) |
| **P1.08** | ← RX | PA08 (`SAMD11_TX`) |

넷 이름이 SAMD11 기준이라 헷갈리기 쉽다. `SAMD11_TX` 는 SAMD11 이 **보내는**
선이므로 nRF 쪽에서는 RX 다. Zephyr `uart20_default` 가 같은 배정이다
(`UART_TX = 1,9` / `UART_RX = 1,8`). 흐름제어는 배선이 없다.

**인스턴스는 UARTE20** — NU54-DK 의 UARTE30 과 다르다.
P1 이 도메인 20 이기 때문이다 ([PERIPHERAL-PINMAP.md](../PERIPHERAL-PINMAP.md)).
코어가 벡터를 variant 로부터 받는 이유가 이것이다
(`SERIAL_UARTE_IRQ_HANDLER`, CLAUDE.md §7 F10 ③).

### XIAO 헤더 (14핀)

| 핀 | 이름 | GPIO | | 핀 | 이름 | GPIO |
|---|---|---|---|---|---|---|
| 1 | D0 / A0 | P1.04 | | 14 | 5V | VBUS |
| 2 | D1 / A1 | P1.05 | | 13 | GND | |
| 3 | D2 / A2 | P1.06 | | 12 | 3V3 | VSYS_3V3 |
| 4 | D3 / A3 | P1.07 | | 11 | D10 / MOSI | P2.02 |
| 5 | D4 / SDA | P1.10 | | 10 | D9 / MISO | P2.04 |
| 6 | D5 / SCL | P1.11 | | 9 | D8 / SCK | P2.01 |
| 7 | D6 / TX | P2.08 | | 8 | D7 / RX | P2.07 |

헤더 밖의 이름도 Zephyr 커넥터 정의와 같다:
`D11 = P0.03`, `D12 = P0.04`, `D13 = P2.10`, `D14 = P2.09`, `D15 = P2.06`.

> ⚠ **D7(P2.07)은 SWO 겸용이다.** SWO 트레이스를 켜면 이 핀을 쓸 수 없다.

> ⚠ **D6 / D7 의 `Serial1` 은 아직 배정하지 않았다.** 회로도와 Zephyr 이 모두
> UARTE21 로 잡는데, 그러면 인스턴스 21 이 P2 를 쓰는 셈이라
> [PERIPHERAL-PINMAP.md](../PERIPHERAL-PINMAP.md) 의 도메인 규칙과 어긋난다.
> 규칙을 바로잡은 뒤 붙인다 (M2). 지금은 일반 GPIO 다.

### 클럭 — 크리스털 2개, **외부 로드 캡 없음**

| | 부품 | 핀 |
|---|---|---|
| LFXO | X2 32.768 kHz ±20 ppm | P1.00(XL1) / P1.01(XL2) |
| HFXO | X1 32 MHz ±10 ppm | XC1 / XC2 |

**⚠ 이 보드에서 가장 중요한 항목이다.** NU54-DK 는 크리스털 옆에 13 pF 외부
캡(C13/C14)이 있지만 **이 보드에는 없다.** 칩 내부 캡을 쓰는 설계다.

내부 캡을 설정하지 않으면(= `INTCAP` 에 0 을 써 넣으면) 부하용량이 모자라
발진이 빨라진다. 실측:

| 설정 | 호스트 대비 |
|---|---|
| 외부 캡(INTCAP=0) — **틀림** | **+805 ppm** |
| 내부 캡 16 pF | **-14 ppm** |

+805 ppm 은 BLE 요구치 ±250 ppm 을 훌쩍 넘으므로 **M3 에서 연결이 끊긴다.**
그런데 M1 수준에서는 아무 증상이 없다 — `millis()` 와 `micros()` 가 서로
완벽히 일치하고 틱도 정확해서, **호스트 시계와 비교해야만 드러난다.**
CLAUDE.md §7 F12 와 같은 계열의 함정이다.

16000 fF 는 Zephyr 보드 정의에서 가져왔다:

```dts
&lfxo {
	load-capacitors = "internal";
	load-capacitance-femtofarad = <16000>;
};
```

> **레지스터 값을 상수로 박지 마라.** `INTCAP` 계산에 `FICR.XOSC32KTRIM` 이
> 들어가고 그 트림은 **칩 개체마다 다르다.** variant 는 용량(fF)만 주고
> 코어가 런타임에 계산한다 (`port_grtc.c` 의 `lfxo_intcap_calc()`).
> 이 개체는 `XOSC32KTRIM = 0x013D0015` (SLOPE 21, OFFSET 317) → `INTCAP = 21`.
>
> nrfx 의 `NRF_OSCILLATORS_LFXO_CAP_CALCULATE` 매크로는 쓰지 마라.
> `((SLOPE + 392) >> 9) * (cap*2-12)` 라서 앞항이 0 으로 잘리고 **cap 값과
> 무관하게 같은 결과가 나온다** (이 칩에서 6/7/9/11 pF 전부 4).

### 아날로그 (SAADC)

| 이름 | 핀 | AIN |
|---|---|---|
| A0 ~ A3 | P1.04 ~ P1.07 | AIN0 ~ AIN3 |

**⚠ XIAO nRF52840 과 다르다. A4 / A5 가 없다.** XIAO 관례상 A4/A5 는 D4/D5 인데,
D4(P1.10)에는 AIN 이 배정돼 있지 않다. AIN 이 있는 핀은 P1.04~07 과 P1.11~14 뿐이고
그중 P1.11~14 는 SCL / 마이크 / 배터리 감지가 이미 쓴다.

### 배터리 전압

```
VBAT ─ TPS22916(U2) ─ R5 10K ─┬─ P1.14 / AIN7
       EN = P1.15             └─ R6 10K ─ GND
```

분압 경로가 로드 스위치로 게이팅돼 있어 평소에는 누설이 없다.
읽을 때만 `PIN_VBAT_ENABLE`(P1.15)을 HIGH 로 올린다.
**배터리 전압 = 측정 전압 × 2.0** (회로도 명기).

충전 IC 는 SGM40567-4.2, `iCharge = 24000 / R4(120K) = 200 mA`.

### 온보드 센서 (Sense 모델)

전원이 TPS22916(U11) 뒤에 있고 **`P0.01` 이 EN** 이다 (0: 차단, 1: 공급).
안 쓸 때 꺼 두면 저전력에 유리하다. variant 는 기본 차단으로 둔다.

| 부품 | 인터페이스 | 핀 |
|---|---|---|
| IMU LSM6DS3TR-C (주소 **0x6A**) | I2C — TWIM30 | SDA P0.04 / SCL P0.03, INT1 P0.02 |
| PDM 마이크 MSM261DGT006 | PDM20 | CLK P1.12 / DATA P1.13 |

IMU 의 I2C 는 헤더의 `Wire`(TWIM22)와 **다른 버스**다. 풀업 R18/R19 4.7K.

### 안테나 — RF 스위치를 반드시 켜야 한다

```
RADIO ─ FM8625H(U10) ─┬─ RF1 ─ ANT1  칩 안테나 (KH5220-A36)
                      └─ RF2 ─ ANT2  u.FL 커넥터
        VDD  = P2.03 (RF_SW_PWR)
        VCTL = P2.05 (RF_SW_CTL)   0: RF1, 1: RF2
```

**스위치에 전원이 없으면 RF 경로가 성립하지 않는다.** variant 의
`initVariant()` 가 전원을 켜고 온보드 칩 안테나(RF1)로 잡아 둔다.
M1 에는 라디오가 없으므로 전류만 소모하는 셈이다 — **전류를 측정할 때는
이 몫을 따로 빼고 보라.** RF1 이 실제로 온보드 안테나인지는 M3 에서 확인한다.

---

## 4. 전원

```
USB-C ─┬─ 5V ─┬─ SGM40567 충전 IC ─ VBAT ─┐
       │      ├─ SGM2040-3.3 LDO ─ SAMD11 전용 3V3 (250 mA)
       │      └────────────────────────────┴─ Q1 ─ VBUS ─ TPS62843 벅 ─ VSYS_3V3 (600 mA)
       └─ D+/D− ─ SAMD11
```

- **SAMD11 전원이 시스템 전원과 분리돼 있다** (별도 LDO). 디버거는 USB 로만 산다
- 시스템 3V3 은 LDO 가 아니라 **DC-DC 벅**이다. 저전력 측정 시 효율 곡선을 감안할 것
- 배터리는 BAT 패드. USB 와 배터리는 Q1(P-MOSFET) + D4 로 OR 결선

---

## 5. Arduino 코어 구현 시 주의 요약

| # | 항목 |
|---|---|
| 1 | LED 는 **active LOW** → `LED_STATE_ON 0` (NU54-DK 와 반대) |
| 2 | LED 가 **하나뿐**. `LED_RED`/`LED_BLUE`/`LED_CONN` 이 전부 같은 핀 |
| 3 | 버튼에 **외부 풀업이 있다** |
| 4 | `Serial` = **UARTE20** (P1.09 TX / P1.08 RX). 벡터는 `SERIAL20_IRQHandler` |
| 5 | **P1.00 / P1.01 은 LFXO 전용** — GPIO 로 노출 금지 |
| 6 | **LFXO 내부 캡 16000 fF 필수.** 빠뜨리면 +805 ppm (§3) |
| 7 | **A4 / A5 가 없다** — XIAO nRF52840 과 다르다 |
| 8 | 배터리 전압은 `P1.15` 를 켜야 읽힌다. ×2.0 |
| 9 | 센서 전원은 `P0.01`. 기본 차단 |
| 10 | **BLE 전에 RF 스위치를 켜야 한다** (P2.03) |
| 11 | D7(P2.07)은 SWO 겸용 |
| 12 | `Serial1`(D6/D7, UARTE21)은 도메인 규칙 확인 후 (M2) |

## ⚠ 시리얼 입력 — VCOM 이 큰 버스트에 멎는다

온보드 CMSIS-DAP 의 VCOM 은 **호스트 -> 타깃 방향**이 약하다. 한 번에 1 KB 정도를
밀어 넣으면 그 뒤로 수신이 완전히 죽고, **타깃 리셋으로도 안 살아난다.**
USB 를 다시 꽂아야 복구된다. 송신(보드 -> PC)은 그동안에도 정상이다.

보드 문제이지 코어 문제가 아니다 — 같은 펌웨어로 NU54-DK(CP2102N)는 1024 바이트를
손실 없이 받는다. 근거는 `docs/STATUS.md` §2.5.

**시리얼 입력이 갑자기 안 되면 USB 를 다시 꽂아라.** 긴 문자열은 나눠 보낸다.
