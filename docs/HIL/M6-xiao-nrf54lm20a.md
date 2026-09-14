# M6 실기 검증 — XIAO nRF54LM20A Sense

날짜: 2026-09-14
보드: Seeed XIAO nRF54LM20A **Sense** (variant `xiao_nrf54lm20a`)
프로브: 온보드 CMSIS-DAP `2886:0068-1:0862DE5D`, 시리얼 `/dev/cu.usbmodem0862DE5D3`
도구: probe-rs 0.32.0, xPack GCC 14.2.1-1.1, macOS + bleak 1.1.1

---

## 1. 결과 요약

| 항목 | 결과 |
|---|---|
| 잠금 해제 + 전체 소거 + SoftDevice 굽기 (명령 한 줄) | ✅ 6.3 s |
| `arduino-cli upload` | ✅ |
| blink | ✅ 빨강(P1.22)만 토글, 파랑·초록 꺼짐 유지 |
| `Serial` (UARTE20 → SAMD11) | ✅ `millis` 간격 정확히 2000 |
| FICR | RAM `0x200` (512 KB), RRAM `0x7F4` (2036 KB), PACKAGE `"PA"` |
| HFXO / LFXO 내부 캡 | ✅ 계산값 = 레지스터 (HFXO INTCAP 40, LFXO INTCAP 23) |
| LFXO 정확도 | **−49 ppm** (5분 151 샘플, 끝점 −49.0 / 최소제곱 −49.5) |
| SoftDevice | ✅ `begin()` = 1, `g_sd_stage` = 11, 요구 RAM `0x20007F48` |
| BLE (안테나 연결 후) | ✅ RSSI −31 dBm, 연결, MTU 247, NUS 에코 |
| `pmic` | ✅ 응답, USB 전원 감지, 다이 30.1 °C, LDO1 기본 꺼짐 |
| `imu` | ✅ LDO1 꺼짐 → 무응답, 켬 → `0x6A`, 가속도 합 ≈ 1.03 g |
| `flash_id` | ✅ JEDEC `85 20 17`, SFDP rev 1.0 |
| `rgb_led` | ✅ 빨강 → 초록 → 파랑 부드럽게 (육안) |
| `button` | ✅ 3번 누름 → 3번 검출, 누를 때마다 LED 토글 (육안) |
| 예제 컴파일 스윕 | ✅ 이 보드 22건 전부 (4 보드 합 76건) |

---

## 2. 출하 상태가 잠겨 있었다

첫 `probe-rs read` 가 이렇게 실패했다:

```
An operation could not be performed because it lacked the permission to do so: erase_all
```

APPROTECT 로 디버그 포트가 잠긴 상태다. 한 줄로 풀고 굽는다:

```sh
probe-rs download --chip nRF54LM20A --binary-format hex \
  --allow-erase-all --chip-erase --verify s145_nrf54lm20_10.0.1_softdevice.hex
```

이 명령을 `programmers.txt` 의 `Erase all + Burn SoftDevice (probe-rs)`
(`sd_erase_burn`) 로 넣었다. Seeed 기본 펌웨어는 지워진다.

---

## 3. 부팅 첫 명령에서 HardFault — RAM 끝 주소

blink 를 올렸는데 P1 `OUT` 이 계속 `0x00000000` 이었다. `initVariant()` 가 LED 를
끄기만 해도(active LOW 라 HIGH) 비트가 서야 하므로 거기까지 못 간 것이다.

```
ICSR  0x00000803   VECTACTIVE = 3 (HardFault)
HFSR  0x40000000   FORCED
CFSR  0x00009201   STKERR + PRECISERR + BFARVALID, IACCVIOL
BFAR  0x2007FFF8
vector[0] (초기 SP) 0x20080000
```

**초기 SP 에서 첫 push 가 없는 주소에 쓰다 죽었다.** 링커의 RAM 끝을 512 KB 로 보고
`0x20080000` 에 잡았는데, 실제 끝은 **`0x2007FD40`** 이다.

| 출처 | RAM |
|---|---|
| sdk-nrf-bm DTS | `cpuapp_sram` 0x20000080 부터 **511K − 0x80 + 0x140** → 끝 `0x2007FD40` ← 정답 |
| FICR `INFO.RAM` | `0x200` = 512 KB — **KB 단위로 올림된 값** |
| MDK `NRF_MEMORY_RAM_SIZE` | `0x40000` (256 KB) — 틀림 |
| MDK `.ld` | RAM `0x40000` + RAM1 `0x40000` |

DTS 주석이 "total size of SRAM is not 1kB aligned" 라고 이미 적고 있었는데
"대략 512 KB" 로 읽은 것이 원인이다. 수정 후 ICSR/CFSR/HFSR 전부 0, blink 정상.

증상이 "LED 가 안 켜진다" 뿐이라 variant 핀 배정을 먼저 의심하기 쉽다.
**LED 가 안 켜지면 SCB 폴트 레지스터부터 읽는다.**

---

## 4. 클럭

### 내부 캡이 계산대로 써졌는지

| | FICR 트림 | 계산 INTCAP | 레지스터 |
|---|---|---|---|
| HFXO (15000 fF) | `XOSC32MTRIM 0x01F50045` → SLOPE 69, OFFSET 501 | 40 | `0x5012071C` = 40 |
| LFXO (17000 fF) | `XOSC32KTRIM 0x013E0016` → SLOPE 22, OFFSET 318 | 23 | `0x50120904` = 23 |

HFXO 캡 설정(`HFXO_LOAD_CAP_FF`)은 이번에 코어에 새로 넣었다. 공식은 Zephyr
`soc/nordic/nrf54l/soc.c` 이고, nrfx 의 `NRF_OSCILLATORS_HFXO_CAP_CALCULATE` 는 식이
달라 쓰지 않았다. 기동 확인: `CLOCK.EVENTS_XOSTARTED = 1`.

### LFXO 정확도

`millis()` 를 2초마다 찍고 호스트 `readline()` 도착 시각과 비교했다 (CLAUDE.md §7 F12 방법).

| 측정 | 결과 |
|---|---|
| 72 초 | −58 ppm |
| 300 초, 151 샘플 | **−49.0 ppm** (최소제곱 −49.5) |

두 측정이 같은 방향이라 잡음이 아니다. 크리스털 사양 ±20 ppm 보다 크고,
SoftDevice 선언 ±250 ppm 안이라 BLE 에는 문제없다. 17000 fF 를 줄이는 튜닝은 하지 않았다.

⚠ 첫 5분 측정은 도중에 **USB 가 뽑혀** `Device not configured` 로 끊겼다.
측정 중 케이블을 건드리지 않게 할 것.

---

## 5. BLE — 원인은 안테나였다

### 증상

시험 스케치(`Bluefruit.begin()` + NUS + 광고)는 시리얼로 전부 정상을 보고했다:

```
REPORT begin=1 adv_start=1 running=1 name=XIAO-LM20A-HIL ram_required=0x20007F48 app_ram_start=0x20008000
```

그런데 bleak 스캔 20초 동안 광고 보고 746건 중 이 보드는 **0건**이었다.

### 가른 순서

| 가설 | 확인 | 결과 |
|---|---|---|
| SoftDevice 활성화 실패 | SWD `g_sd_stage` / `m_last_error` / `g_sd_fault_*` | 11 / 0 / 0 — 정상 |
| IRQ 포워딩 이름이 LM20A 벡터에 없음 | `sd_irq_forward.S` 의 7개 핸들러 ↔ LM20A 스타트업 `.long` | 전부 존재 |
| 라디오가 송신 안 함 | SWD `RADIO.STATE` / `FREQUENCY` / `MODE` 반복 샘플 | STATE 0xB(TX) 관측, 주파수 2402 ↔ 2480, MODE 3(BLE 1M) — **송신 중** |
| HFXO 안 돎 (RC 로 송신) | `CLOCK.EVENTS_XOSTARTED` | 1 — 기동됨 |
| **안테나** | 회로도: RF → u.FL(ANT2) 뿐 | **외부 안테나가 없었다** |

안테나 연결 후:

```
SCAN: found … after 0.4s        (RSSI −31)
CONNECT: ok  mtu=247
ECHO: b'hello-lm20a'
```

참고로 첫 bleak 실행은 macOS 블루투스가 켜져 있는데도 `Bluetooth device is turned off`
를 냈다. 재실행에서는 사라졌다 (일시적).

### SoftDevice RAM

LM20 hex 로도 peripheral 4 + central 1 + 큐 3 의 요구량이 **`0x20007F48`** 로
L15·L05 와 같았다. 32 KB 예약에 184 B 남는다.

---

## 6. 보드 라이브러리

| 예제 | 확인한 것 |
|---|---|
| `pmic` | TWIM24 로 nPM1300 응답. USB present, not charging(배터리 없음), 배터리 14~19 mV, 다이 30.1 °C, LDO1 off |
| `imu` | `IMU.begin()` 이 LDO1 을 켜고 104 Hz 설정. 가속도 (−0.12, −0.07, 1.03) g, 자이로 영점 ≈ 3 dps, 70 mdps 단위로 변함(±2000 dps 감도와 일치) |
| `flash_id` | `SPI1`(SPIM00, P2). JEDEC `85 20 17`, SFDP `SFDP` rev 1.0 |
| `rgb_led` | `analogWriteOk` 세 핀 모두 true, 육안으로 순서·페이드 확인 |
| `button` | `attachInterrupt(FALLING)`, 누름 3 → edges 3 (채터링 없음), LED 토글 |

IMU 레일은 3.3 V 로 켰다. Zephyr 는 1.8 V 로 켜지만 이 보드의 버스 풀업이 레일에
달려 있어 3.3 V 가 맞다고 판단했고, 3.3 V 에서 정상이다 (1.8 V 는 시험하지 않았다).

---

## 7. 컴파일에서 드러난 것

LM20A 로 처음 빌드했을 때 오류가 **`nrfx_i2s.c` 한 파일**에서만 났다 (167줄).
LM20A 에는 I2S 가 없다 — Pin Planner SoC 정의에 `TDM` 만 있고 L15 는 `I2S20`,
최신 nrfx(main, v4.6.0) 헤더에도 LM20A I2S 는 없다. 헤더 지연이 아니라 칩 차이다.
칩 define 가드로 막았다 (`nordic/nrfx/VENDORING.md`).

`nrfx_irqs` · `nrfx_config` 템플릿과 스타트업을 LM20A 용으로 추가한 뒤 링크 결과
`SERIAL20` / `GRTC_2` / `SVC` / `RADIO_0` / `GPIOTE20_0` / `SWI00` 핸들러가 모두 `T` 였다.
