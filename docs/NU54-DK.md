# NU54-DK (nRF54L05) / NU54V-DK (nRF54L15)

회로도 `NU54_DK.SchDoc` / `NU54_Power.SchDoc` (2025-10-01, edited 2025-10-16) 분석 결과.
MCU 모듈: **NCRB54N01VC** (62핀).

**두 보드는 이 문서의 핀 배정을 그대로 공유한다.** 실장 모듈만 다르고
회로도·핀맵이 동일해서 Arduino variant 도 `variants/nu54dk` 하나를 같이 쓴다.
차이는 메모리 크기뿐이며 [MEMORY-MAP.md](MEMORY-MAP.md) 에 정리돼 있다.

| | NU54-DK | NU54V-DK |
|---|---|---|
| 칩 | nRF54L05 (500KB RRAM / 96KB RAM) | nRF54L15 (1.5MB RRAM / 256KB RAM) |
| FQBN | `baram-nrf54-arduino:nrf54l:nu54dk` | `baram-nrf54-arduino:nrf54l:nu54vdk` |

> **핵심**: LED / 버튼 / UART 핀이 **Nordic nRF54L15 DK와 완전히 동일**하다.
> sdk-nrf-bm `boards/nordic/bm_nrf54l15dk/include/board-config.h`와 1:1로 일치하므로
> Nordic 샘플이 핀 수정 없이 그대로 돈다.

---

## 1. 핀 배정

### LED — **active HIGH**

| 부품 | 핀 | 구동 | 게이트 저항 |
|---|---|---|---|
| D7 | **P2.09** | Q2A (DMN2991 N-MOSFET) | R13 10K 풀다운 |
| D8 | **P1.10** | Q2B | R14 1M 풀다운 |
| D9 | **P2.07** | Q3A | R15 1M 풀다운 |
| D10 | **P1.14** | Q3B | R16 1M 풀다운 |

애노드는 3V3에 1k(R9~R12)로 물려 있고 캐소드가 MOSFET 드레인이다.
게이트를 HIGH로 하면 켜진다 → **`LED_STATE_ON = 1`**.
Adafruit nRF52 코어의 기본값(`LED_STATE_ON 0`, active LOW)과 **반대**이므로 variant에서 뒤집어야 한다.

> ⚠ **D9(P2.07)는 SWD 커넥터 J3의 6번핀(SWO)과 공유**한다. SWO 트레이스를 켜면 LED가 같이 깜빡인다.

### 버튼 — **active LOW, 내부 풀업 필요**

| 부품 | 핀 |
|---|---|
| SW2 | **P1.13** |
| SW3 | **P1.09** |
| SW4 | **P1.08** |
| SW5 | **P0.04** |
| SW1 | RESET (R17 10K 풀업 + D6 1N4148) |

회로도에 `USE INTERNAL PULLUP`이라고 명기돼 있다. 외부 풀업 없음 → `pinMode(pin, INPUT_PULLUP)` 필수.

### UART — CP2102N USB 브리지

| nRF54L15 | 방향 | CP2102N |
|---|---|---|
| **P0.00** | TX → | RXD (pin 20) |
| **P0.01** | ← RX | TXD (pin 21) |
| **P0.02** | CTS ← | RTS (pin 19) |
| **P0.03** | RTS → | CTS (pin 18) |

> **⚠ 하드웨어 흐름제어(RTS/CTS)는 쓰지 않는다.** 프로젝트 결정 사항이다.
> HWFC 를 켜면 UARTE 가 상대의 CTS 어서트를 기다려 한 바이트도 내보내지 않는데,
> 호스트 터미널이 RTS 를 올리지 않는 것이 보통이라 `Serial.println()` 이 그대로 멈춘다.
> 실기에서 실제로 겪었다.
> **결과적으로 P0.02 / P0.03 이 비어 `Wire`(TWIM30) 에 배정됐다** — docs/PERIPHERAL-PINMAP.md 참조.
>
> UARTE30 PSEL 레지스터 필드 순서는 **TXD, CTS, RXD, RTS** 다 (nRF52 와 다르다).

**인스턴스는 `UARTE30`.** Nordic DK의 `BOARD_APP_UARTE_INST` / `BOARD_APP_UARTE_PIN_*`와 동일하다.
P0.x는 항상 켜져 있는 저전력 도메인에 속하고 UARTE30/SPIM30/TWIM30이 이 포트를 담당한다.

> Nordic DK의 **콘솔** UART(`UARTE20`, P1.04/P1.07)와는 다르다. 우리 보드는 애플리케이션 UART 쪽이
> USB 브리지에 연결돼 있으므로 `Serial` = UARTE30이다.

### 클럭 — LFXO 실장

**Y1 32.768 kHz** (Q13FC13500004) + C13/C14 13pF가 **P1.00(XL1) / P1.01(XL2)** 에 붙어 있다.

- variant에 **`USE_LFXO`** 정의
- GRTC `CLKSEL`을 LFXO로 (CLAUDE.md §7 F3)
- **P1.00 / P1.01을 GPIO로 쓰면 안 된다.** 핀 테이블에서 제외하거나 no-op 처리할 것

### 아날로그 / 특수 기능

| 기능 | 핀 |
|---|---|
| AIN0 ~ AIN3 | P1.04, P1.05, P1.06, P1.07 |
| AIN4 ~ AIN7 | P1.11, P1.12, P1.13, P1.14 |
| NFC1 / NFC2 | P1.02 / P1.03 |

> AIN6(P1.13)은 SW2와, AIN7(P1.14)은 LED D10과 겹친다.

---

## 2. 디버그 / 프로그래밍

**온보드 디버그 프로브가 없다.** 외부 CMSIS-DAP 장비를 연결해서 쓴다.

### J3 — ARM Cortex 10핀 1.27mm (표준 배열)

| 핀 | 신호 | 핀 | 신호 |
|---|---|---|---|
| 1 | VMCU (VTref) | 2 | SWDIO |
| 3 | GND | 4 | SWDCLK |
| 5 | GND | 6 | **SWO = P2.07** |
| 7 | NC (key) | 8 | NC |
| 9 | GND | 10 | nRESET |

### P2 — 5핀 1.27mm

`1 = VMCU`, `2 = SWDIO`, `3 = GND`, `4 = SWDCLK`, `5 = RESET`

업로드는 **probe-rs** (CLAUDE.md §3).

검증에 쓴 프로브: **NU-DAP** — CMSIS-DAP, VID:PID `0d28:0204` (Arm).
`probe-rs list` 로 인식되고 `--chip nRF54L15` 로 접속·플래시·verify 모두 정상.
33 KB hex 쓰기 + verify 에 약 3.3 초. `--connect-under-reset` 은 이 프로브에서 실패하니 쓰지 마라.

---

## 3. 확장 헤더

25핀 2개. P1.00/P1.01(LFXO)을 포함해 **전부 그대로 노출**된다.

**P1 헤더**
```
 1 P0.00   2 P0.01   3 GND    4 P0.02   5 P0.03
 6 P0.04   7 P1.00   8 GND    9 P1.01  10 P1.02
11 P1.03  12 P1.04  13 GND   14 P1.05  15 P1.06
16 P1.07  17 P1.08  18 GND   19 P1.09  20 P1.10
21 P1.11  22 SWDCLK 23 SWDIO 24 GND    25 GND
```

**P3 헤더**
```
 1 GND     2 GND     3 RESET  4 P1.12   5 P1.13
 6 P1.14   7 P2.10   8 GND    9 P2.09  10 P2.08
11 P2.07  12 P2.06  13 GND   14 P2.05  15 P2.04
16 P2.03  17 P2.02  18 GND   19 P2.01  20 P2.00
21 VMCU   22 3V3    23 GND   24 GND    25 VIN
```

**P2.00 ~ P2.10이 전부 노출**되므로 고속 도메인(SPIM00)을 쓸 수 있다. CLAUDE.md §4 SPI 주의사항 참조.

---

## 4. 전원

```
USB-C (J1) ──┬─ VBUS ─ FB1 ─ CP2102N (U3)
             └─ D2 ─┐
VIN (J2, 5~14V) ─ D1 ─┴─ AZ1117CR-3.3 (U1) ─ 3V3 ─ VMCU
```

- USB-C: CC1/CC2에 5.1k(R1/R2) — sink 전용
- 입력 보호: D11 SD15C, D12 PTVS5V5D1BLYL
- USB와 VIN은 SBR2U60S1F 다이오드로 OR 결선 → 동시 인가 가능
- **3V3와 VMCU가 분리된 네트**다. 전류 측정 시 어느 지점을 재는지 확인할 것 (CLAUDE.md §7 F8)

---

## 5. Arduino 코어 구현 시 주의 요약

| # | 항목 |
|---|---|
| 1 | LED는 **active HIGH** → `LED_STATE_ON 1` (Adafruit 기본값과 반대) |
| 2 | 버튼은 외부 풀업 없음 → `INPUT_PULLUP` 필수 |
| 3 | **P1.00 / P1.01은 LFXO 전용** — GPIO로 노출 금지 |
| 4 | **P2.07 = LED D9 + SWO** 겸용 — variant.h에 주석 명기 |
| 5 | `Serial` = **UARTE30** (P0.00/P0.01, 흐름제어 P0.02/P0.03) |
| 6 | **포트가 3개**(P0/P1/P2) — Adafruit의 2포트 `digitalPinToPort()`를 확장해야 함 |
| 7 | 온보드 프로브 없음 — 외부 CMSIS-DAP 필요 |
| 8 | **`USE_LFXO` 정의 필수** — 빠뜨리면 내부 RC 로 돌아 클럭이 0.9% 틀린다 (CLAUDE.md §7 F12) |
| 9 | **흐름제어 미사용** → P0.02 / P0.03 은 `Wire`(TWIM30) 가 쓴다 |
| 10 | **레지스터 오프셋이 nRF52 와 다르다** — GPIO `OUT` 이 `0x504` 가 아니라 **`0x000`**. 리셋 원인은 `NRF_RESET`. 상세는 [HIL/M1-nu54dk.md](HIL/M1-nu54dk.md) §3 |
| 11 | 페리페럴은 **전원 도메인이 소유한 GPIO 포트만** 쓸 수 있다 — [PERIPHERAL-PINMAP.md](PERIPHERAL-PINMAP.md) |
