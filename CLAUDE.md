# nRF54L Arduino Core

nRF54L 시리즈용 Arduino 코어. Nordic SoftDevice + FreeRTOS 기반, Adafruit Bluefruit API 호환.

이 문서는 프로젝트 지침이다. 설계 결정은 이미 확정됐다. 재검토하지 말고 실행하라.

---

## 0. 시작 전 필수 확인

**M0(§10)은 2026-09-05에 수행 완료됐고, 그 결과가 이 문서에 반영돼 있다.**
아래 값들이 조사로 확정된 것이며, 본문 곳곳에 근거와 함께 들어가 있다.

| 항목 | 확정값 |
|---|---|
| sdk-nrf-bm | **v2.0.1** (NCS v3.3.0 기반) |
| SoftDevice | **S115 / S145 v10.0.1** |
| SoftDevice 재배포 | **가능.** 별도 제한 계약 없이 Nordic-5-Clause 그대로 (§9) |
| SVC 충돌(F1) | **있다.** FreeRTOS 11.x 가 SVC 100~105 사용 → 0~5 로 옮김 (§7 F1) |
| SD 예약 인터럽트 우선순위(F2) | **0과 4** (§7 F2) |
| FreeRTOS 포트 | sdk-nrf-bm에 **없음.** 직접 포팅 (§7 F4) |
| 업로드 툴 | **probe-rs**, 타깃 이름 `nRF54L15` (§3). `probe-rs chip list` 에 **nRF54L05 타깃은 없다** — 같은 다이라 L05 도 `nRF54L15` 로 플래시한다 |
| `__NVIC_PRIO_BITS` | **3** (0~7). BASEPRI 는 `prio << 5` |
| GRTC 인터럽트 그룹 | 앱 = **`GRTC_2_IRQn`**, SoftDevice = `GRTC_3_IRQn`. 앱 CC 는 0~6 |
| 리셋 원인 레지스터 | **`NRF_RESET`** (nRF54H 계열의 `NRF_RESETINFO` 아님) |
| WFI 기상과 마스크(F9) | **BASEPRI 는 기상을 막는다. PRIMASK 는 막지 않는다.** 슬립 창에서 BASEPRI 를 걸어 두면 안 된다 (§7 F9) |
| 지원 보드 | `nu54dk` = NU54-DK / **nRF54L05**(500KB/96KB), `nu54vdk` = NU54V-DK / nRF54L15(1.5MB/256KB). 핀맵 동일, variant 공유 |
| 실장 칩 판별 | `FICR INFO.PART` @ `0x00FFC31C` — `0x00054B05` = L05, `0x00054B15` = L15 |

시간이 지나면 위 값도 바뀔 수 있다. 재확인이 필요하면 §10 M0의 절차를 다시 밟고 이 표를 갱신하라.

문서 내용과 실제 저장소 상태가 다르면 **실제 저장소가 정답이다.** 문서를 고쳐라.

---

## 1. 절대 규칙

위반하면 프로젝트 전제가 무너진다. 예외 없음.

| # | 규칙 | 이유 |
|---|---|---|
| R1 | Zephyr RTOS를 도입하지 마라 | 베이스는 `sdk-nrf-bm`(베어메탈). Zephyr은 DTS/Kconfig/빌드시간 부담을 다 끌고 온다 |
| R2 | ArduinoBLE를 지원하려 하지 마라 | ArduinoBLE는 HCI 필요. SoftDevice는 HCI 미노출. 구조적 불가 |
| R3 | SoftDevice 바이너리를 수정·디스어셈블·리버스엔지니어링 하지 마라 | Nordic-5-Clause 5항 위반 |
| R4 | 코드를 GPL로 라이선스하지 마라 | Nordic-5-Clause 4·5항이 GPL의 "추가 제약 금지"와 충돌. MIT 또는 Apache-2.0만 |
| R5 | TrustZone / TF-M / `/ns` 빌드로 가지 마라 | secure-only 고정. 복잡도 대비 실익 없음 |
| R6 | FLPR(RISC-V 코프로세서)을 건드리지 마라 | 범위 밖 |
| R7 | FreeRTOS를 빼지 마라 | Bluefruit API 호환의 필수 조건. §6 참조 |
| R8 | BLE 스택을 직접 구현하지 마라 | SoftDevice(인증됨)만 사용 |
| R9 | 4바이트 정렬을 가정한 NVM 쓰기 코드를 만들지 마라 | RRAM write block = 16바이트 |
| R10 | USB 관련 기능을 nRF54L15에 넣지 마라 | 하드웨어에 USB가 없다 |
| R11 | Bluefruit API와 Arduino API 아래에 별도 HAL/추상화 레이어를 만들지 마라 | 이 두 API 자체가 이미 seam이다. 세 번째 레이어는 추상화를 위한 추상화 |
| R12 | 이식성과 Adafruit 호환이 충돌하면 **호환을 택하라** | 마이그레이션 호환이 이 프로젝트의 존재 이유다 |

---

## 2. 아키텍처

```
사용자 스케치 (.ino)
├─ Bluefruit52Lib 호환 API      ← sd_ble_* 위에 재구현
├─ Arduino API                   ← nrfx 위에 구현
├─ SchedulerRTOS                 ← Adafruit rtos.h와 동일 API
├─ FreeRTOS (tickless, GRTC 틱)  ← MIT
├─ SoftDevice S115 / S145        ← Nordic-5-Clause, BT 인증
├─ nrfx
└─ 부트로더 (UART DFU) — M4에서 추가
```

- 베이스 SDK: `nrfconnect/sdk-nrf-bm` v2.0.1 (nRF Connect SDK Bare Metal)
- 1차 타깃: nRF54L15 / 2차: nRF54LM20A
- 성공 기준: **Adafruit nRF52 사용자의 기존 `.ino`가 최소 수정으로 컴파일·동작**

### 지원 보드 / 네이밍 (확정)

| 항목 | 값 |
|---|---|
| 지원 보드 | **NU54-DK (nRF54L05)** / **NU54V-DK (nRF54L15)** / **XIAO nRF54L15 (Seeed)** |
| board id | `nu54dk` / `nu54vdk` / `xiao_nrf54l15` |
| `build.variant` | `nu54dk` 는 두 DK 가 공유(핀맵 동일), XIAO 는 별도 |
| packager (FQBN 앞) | **`baram-nrf54`** — 인덱스의 `packages[0].name` 이자 개발용 `hardware/` 링크 이름 |
| architecture | **`nrf54l`** (플랫폼 디렉토리 `nrf54l/`, 코어 `cores/nrf54l/`) |
| 릴리스 아카이브 | `baram-nrf54l-<ver>.tar.bz2` |
| Board Manager 인덱스 | `package_baram_nrf54_index.json` |

인덱스 이름만 `nrf54`로 넓게 잡은 이유: 하나의 index에 platform을 여러 개 넣을 수 있으므로,
훗날 nRF54H를 하게 되면 같은 JSON의 `platforms[]`에 `architecture: "nrf54h"`로 추가하면 된다.
**사용자가 등록할 Board Manager URL은 영구히 하나다.** nRF54H는 SoftDevice 베어메탈 옵션 자체가
없는 다른 칩이므로 architecture를 합치지 않는다 — 합치면 호환되지 않는 두 코어에
라이브러리가 모두 호환된다고 표시된다.

### 2.1 왜 베어메탈인가 (재확인)

현재 요구사항 기준으로 베어메탈이 우세하다. **BLE 스택 품질은 근거가 아니다** (§5.1).

- **커스텀 보드 + UART DFU + Bluefruit 이전** — 세 목표 모두 베어메탈이 유리
- **부트로더** — 스케치가 standalone 바이너리라 UART DFU가 자연스럽다.
  NU54DK가 v0.3.0에서 DFU를 넣지 못하고 v0.6.0으로 미룬 것이 구조적 차이를 보여준다
- **설치 크기** — 우리 플랫폼은 **비압축 15MB / tar.bz2 1.4MB** (실측)이고 오프라인 설치가 된다.
  NCS 방식은 첫 설치가 수 GB 다운로드다
- **빌드 결합도** — NCS 빌드 시스템에 강결합되면 NCS 버전이 바뀔 때마다 재검증이 필요하다
- **드라이버 부담은 과대평가돼 있다** — §5.1 두 번째 항목 참조

**Zephyr 쪽이 우세한 영역** (현재 요구사항에 없음, 인지만 할 것):
프로토콜 확장(Matter / Thread / Zigbee / LE Audio / 802.15.4),
인프라 스택(MCUboot / TF-M / PSA / settings / 파일시스템), 신규 칩 upstream 지원.

### 2.2 재검토 트리거

아래 중 하나라도 발생하면 **작업을 멈추고 사람에게 보고하라.** Zephyr 이전을 재검토해야 한다.

- 노르딕이 `sdk-nrf-bm` 릴리스를 1년 이상 중단하거나 maintenance mode를 선언
- 제품 요구사항에 Matter / Thread / Zigbee / LE Audio / 802.15.4가 추가됨
- nRF54L 후속 칩이 Bare Metal에서 지원되지 않음
- 페리페럴 래퍼 유지보수가 감당 불가 수준으로 커짐

> 노르딕은 Bare Metal을 "nRF5 SDK 사용자의 마이그레이션을 돕고 Zephyr RTOS로 가는
> 업그레이드 경로를 제공"하는 것으로 포지셔닝한다. 다리(bridge)로 설계된 제품이라는 뜻이고,
> 다리는 역할이 끝나면 투자가 줄 수 있다. **이것이 이 아키텍처의 최대 장기 리스크다.**

---

## 3. 업로드 경로 — 2단계 전략

**초기 개발(M1~M3)은 CMSIS-DAP + SWD를 사용한다. 부트로더는 M4에서 추가한다.**

| 단계 | 업로드 경로 | 비고 |
|---|---|---|
| M1~M3 | **CMSIS-DAP (probe-rs)** 또는 J-Link, SWD 직결 | 부트로더 없음. 스케치가 0x0에서 직접 실행 |
| M4~ | UART DFU (부트로더 경유) | CMSIS-DAP 경로는 **제거하지 말고 병행 유지** |

이렇게 나누는 이유:
- 부트로더 없이 M1~M3를 끝낼 수 있다 → FreeRTOS 포팅과 BLE를 부트로더 리스크와 분리해 검증 가능
- 부트로더가 깨졌을 때 복구 경로가 항상 필요하다
- 개발자는 SWD, 최종 사용자는 UART DFU — 둘 다 필요하다

### 툴 선정 — probe-rs 확정

조사 결과와 선정 이유:

| 툴 | nRF54L15 | 배포 형태 | 판정 |
|---|---|---|---|
| **probe-rs** | ✅ `nRF54L_Series.yaml` 내장 | **OS별 단일 정적 바이너리** (MIT/Apache-2.0) | **채택** |
| pyOCD 0.39 | ✅ `nrf54l` 타깃 내장 | Python 의존 | 대비책 |
| nrfjprog / J-Link | ✅ | J-Link 프로브 전용 | CMSIS-DAP 불가 |
| OpenOCD 0.12.0 | ❌ nRF54 타깃 없음 | — | 배제 |

**추가 설치가 필요 없어야 한다**는 것이 결정 기준이었다. probe-rs 바이너리를
`nrf54l/tools/probe-rs/{macosx,linux,win}/`에 동봉하면 사용자는 Board Manager 설치만으로 끝난다.
`baram-stm32-arduino`가 업로더를 Go로 만들어 동봉한 것과 같은 이유다 — **IDE는 Python을 동봉하지 않는다.**
probe-rs 하나가 CMSIS-DAP과 J-Link를 모두 커버하므로 툴이 늘지도 않는다.

`boards.txt`에 업로드 경로를 모두 정의하고 `Tools → Upload method` 메뉴로 선택하게 한다:
`CMSIS-DAP (probe-rs)` 기본 / `CMSIS-DAP + Probe UID` / `J-Link` / (M4에서) `UART DFU`.

**프로브가 여러 대일 때를 위해 CMSIS-DAP UID 지정 옵션을 처음부터 넣어라.** COM 포트 번호나 DAPLink 드라이브 문자가 아니라 프로브 UID다. 나중에 넣으려면 recipe를 다시 손대야 한다.
arduino-cli의 `tools.<t>.upload.field.<name>` 기법을 쓴다 — IDE가 값을 입력받아 `{upload.field.probe_id}`로 치환한다.
`EIDOSDATA/NU54DK_Arduino_Core/platform.txt`가 이 방식을 쓰고 있으니 참고하라. probe-rs는 `--probe <VID>:<PID>:<Serial>`을 받는다.

일반 업로드는 mass erase나 recover를 자동 실행하지 않는다. `programmers.txt`의 별도 항목으로 분리하라.

---

## 4. 하드웨어 제약

| | nRF54L15 | nRF54LM20A |
|---|---|---|
| CPU | 128MHz Cortex-M33 | 동일 |
| NVM | 1.5MB RRAM | 2MB RRAM |
| RAM | 256KB | 512KB |
| USB | **없음** | High-speed USB |
| 오디오 | I2S, PDM | **TDM**(I2S 아님), PDM |

### RRAM (flash 아님)
- write block **16바이트**
- erase 개념 없음. erase는 0xFF 쓰기로 에뮬레이션됨 (`no_explicit_erase = true`)
- 부트로더는 **single-bank 또는 overwrite-only**. swap 알고리즘 쓰지 말 것

### USB 없음 (L15) — 설계에 직결
- USB DFU / UF2 / 1200bps touch / 네이티브 USB CDC 전부 불가
- 업로드·시리얼은 UART 또는 SWD

### 커스텀 보드 요구사항

**현행 NU54-DK 실측 (회로도 확인 완료 — `docs/boards/NU54-DK.md`)**: 온보드 디버그 프로브가 **없다.**
CP2102N USB-UART 브리지 + 외부 프로브용 SWD 헤더(J3 = ARM 10핀 1.27mm, P2 = 5핀)만 있다.
따라서 M1~M3 개발에는 **외부 CMSIS-DAP 장비를 J3에 연결**한다. 현재 계획에 지장 없다.

**차기 보드 리비전 권장 사항** (현행 보드에는 없음):
- **온보드 CMSIS-DAP 프로브** (SAMD11 / CH552 등) — 케이블 하나로 개발 가능
- 프로브가 UART도 겸하면 더 좋다 ("DAP UART" 구성)
- **부트로더 진입용 GPIO strap** — M4 이후 필요
- **DTR → RESET 자동 리셋 회로** — 아래 참조

#### ESP32식 자동 부트로더 진입 (차기 리비전에서만 가능)

**현행 보드에서는 불가능하다.** 회로도 실측 결과:

- RESET 네트 = `C10, D6, J3-10(SWD nRESET), P2-5, P3-3, R17, SW1, X1-41`.
  **CP2102N 핀이 하나도 없다.** 브리지가 MCU를 리셋시킬 물리 경로가 없다
- RTS(U3.19) / CTS(U3.18)는 P0.02 / P0.03 에 **UART 흐름제어로만** 연결돼 있고 RESET 에 닿지 않는다
- **DTR(U3.23)은 미연결(플로팅)** — 차기 리비전에서 쓸 수 있다.
  DSR·DCD·RI·SUSPEND·GPIO.2·GPIO.3 도 비어 있다

차기 리비전 선택지:

| 안 | 회로 | 비고 |
|---|---|---|
| A. ESP32 그대로 | `Q1: E=DTR, B=RTS, C=RESET` / `Q2: E=RTS, B=DTR, C=STRAP` 교차 결선 | DTR·RTS가 다를 때만 동작해 터미널 열 때의 데드락을 막는다. **RTS를 흐름제어로 못 쓴다** |
| **B. DTR만 리셋 (권장)** | 트랜지스터 1개로 `DTR → RESET`(오픈드레인). 모드 선택은 부트로더 초기 대기창의 매직 바이트로 | RTS/CTS를 흐름제어로 유지. 부품 1개. 부트로더가 진입창을 통제하므로 strap 불필요 |

확인 필요:
- **nRF54L15의 RESET 핀이 기본 활성인지.** nRF52는 `UICR.PSELRESET` 설정이 필요했다.
  필요하면 공장 출하 시 한 번 써넣어야 한다
- 터미널 열 때 리셋되는 문제(Arduino Uno와 같은 동작). 원치 않으면 "RESET EN" 솔더 점퍼를 둘 것
- 기존 RESET 라인의 D6 클램프와 R17 풀업은 오픈드레인 구동과 충돌하지 않는다

CP2102N의 GPIO.2/GPIO.3도 비어 있지만 호스트에서 벤더 특화 USB 제어 요청으로만 만질 수 있어
툴이 libusb를 요구하게 된다. DTR/RTS는 모든 시리얼 API가 노출하므로 **DTR 쪽이 맞다.**

현행 보드에 이미 있는 것: **UART 브리지**(CP2102N, `Serial` 출력 + M4 이후 DFU 경로), 32.768 kHz LFXO.

### 4.1 보드 문서 규칙 — 핀맵의 정본은 `docs/boards/`

**핀 배정표를 이 문서에 두지 마라.** 보드마다 다르고, 여기에 복제하면 보드가 늘 때
갱신이 누락된다. 실제로 그렇게 낡은 표가 한 번 생겼다.

| 보드 | 칩 | variant | 문서 |
|---|---|---|---|
| NU54-DK / NU54V-DK | nRF54L05 / nRF54L15 | `nu54dk` (공유) | `docs/boards/NU54-DK.md` |
| XIAO nRF54L15 / Sense | nRF54L15 | `xiao_nrf54l15` | `docs/boards/XIAO-nRF54L15.md` |

**어디에 쓸지 판단 기준은 "보드 사실이냐 칩 사실이냐" 하나다:**

| 종류 | 예 | 위치 |
|---|---|---|
| **보드** 사실 | 핀 배정, LED 극성, 크리스털 실장, 디버그 경로, 전원 | **`docs/boards/<보드>.md`** — 보드(회로도) 하나당 문서 하나 |
| **칩** 사실 | RRAM/RAM 배치, 페리페럴↔GPIO 도메인 규칙, 레지스터 오프셋 | `docs/MEMORY-MAP.md`, `docs/PERIPHERAL-PINMAP.md` — **보드별로 복제하지 마라** |
| 실기 기록 | 측정값, 원인 규명 과정 | `docs/HIL/<마일스톤>-<보드>.md` |
| 지침·결정 | 규칙, 함정, 마일스톤 | 이 문서 (루트 `CLAUDE.md` 하나) |

회로도가 같고 실장 모듈만 다른 보드는 **문서 하나를 공유한다** (NU54-DK / NU54V-DK).
variant 를 만들거나 고칠 때 반드시 해당 보드 문서를 먼저 읽어라.

### 4.2 nRF54LM20A 조사 결과 (M6 대비)

**베어메탈 지원됨.** sdk-nrf-bm v2.0.1 에 S115/S145 v10.0.1 hex 가 있다 (같은 5-Clause).

| | nRF54L15 | nRF54LM20A |
|---|---|---|
| RRAM / RAM | 1.5MB / 256KB | 2MB / 512KB |
| GPIO 포트 | P0 P1 P2 (3개) | P0 P1 P2 **P3** (4개) |
| SERIAL 인스턴스 | ~22 | **~24** (SPIM/TWIM/UARTE 23·24 추가) |
| USB | 없음 | **USBHS** + USBHSCORE + VREGUSB (High-Speed) |
| 오디오 | I2S20 | **TDM** |

**SoftDevice 파일 — 중요:**
- **hex 는 다르다** (L15 389583 B / LM20 389951 B, sha256 상이). SoC 별 빌드다
- **API 헤더는 완전히 동일하다.** `nrf_sd_def.h` `nrf_sd_isr.h` `ble_gap.h`
  `nrf_soc.h` `nrf_sdm.h` 를 전부 비교했고 차이가 없었다
  → **헤더는 한 벌만 vendoring 한다** (`softdevice/s145_10.0.1_API/`).
    hex 만 SoC 별로 둔다
- `nrf_sd_def.h` 가 같다는 것은 **SD 예약 자원이 동일**하다는 뜻이다.
  §7 F2(우선순위 0·4) / F3(GRTC CC7~11, GRTC_3) 분석이 그대로 유효하다

**구조 변경이 필요 없는 것** (확인함):
- GPIO — `nrf_gpio_*` 가 절대 핀 번호를 받아 포트를 알아서 푼다. 4포트 자동 지원
- `NRF54L_PORT_OF` (>>5) 도 그대로
- FreeRTOS / GRTC 포트, SVC 디스패처, SoftDevice 헤더

**이미 조건부로 바꿔 둔 것:**
- `Uart.cpp` 가 UARTE 인스턴스를 하드코딩하지 않는다.
  variant 의 `SERIAL_UARTE_INSTANCE` 로 고른다
- `nrfx_config.h` 의 UARTE23/24 가 `#if defined(NRF54LM20A_XXAA)` 로 분기

**M6 에서 해야 할 것:**
- variant + 링커 스크립트 + 메모리 맵 (LM20 DTS 에서 뽑는다)
- `nrf54l_domains.h` 에 LM20A 도메인 표 추가.
  ⚠ **규칙이 그대로인지 확인 필요** — P3 가 P1 과 같은 대역(0x500D)에 있고,
  SPIM/TWIM/UARTE **23·24 는 GRTC 와 같은 0x500E 대역**이라 L15 의
  "첫 자리 = 도메인, 도메인당 포트 하나" 규칙이 단순 확장되지 않을 수 있다
- **USB 는 별개 대공사다.** nrfx 에 `nrf_usbhs.h` HAL 은 있으나
  **`nrfx_usbhs` 드라이버가 없다** (`nrfx_usbd` 는 nRF52 용).
  디바이스 스택(TinyUSB 등)의 nRF54L USBHS 포트 유무부터 조사해야 한다.
  **R10 은 "nRF54L15 에 USB 가 없다" 는 사실 그대로 유지한다.** LM20A 는 별개 판단이다

### SPI 주의 (L15)
외부 노출 Arduino SPI 핀은 SPIM00 하나만 사용 가능. P2 고속 라우팅, E0/E1 출력 드라이브, HSBIAS slew, SPIM anomaly 8 워크어라운드가 필요하다. `lolren/nrf54-arduino-core`에 구현 사례가 있다.

---

## 5. 검토 완료된 대안 (재검토 금지)

이 결정들을 다시 제안하지 마라. 이미 배제됐다.

| 대안 | 배제 이유 |
|---|---|
| `arduino/ArduinoCore-zephyr` 포크 | **mainline** Zephyr 기반이라 SoftDevice Controller를 쓸 수 없고 오픈소스 LL만 가능(미인증, nRF54L 미성숙). llext 모델이 Adafruit 사용자 기대와 불일치 |
| `lolren/nrf54-arduino-core` 사용 | BLE 스택이 자체 구현·미인증·동시 링크 1개. 단일 메인테이너. 부트로더/DFU 없음 |
| `EIDOSDATA/NU54DK_Arduino_Core` 방식 (NCS 전체 빌드) | R1. 매 컴파일이 west build, `prj.conf` 미노출, Windows 전용. **BLE 스택 품질은 배제 사유가 아니다** — §5.1 참조 |
| ArduinoBLE 지원 | R2 |
| SoftDevice Controller(nrfxlib) + 자체 HCI + ArduinoBLE | Zephyr 외 사용이 비공식 경로. ArduinoBLE 호스트도 미인증. 실익 없음 |

**용어 구분** (혼동 주의):
- `SoftDevice` (S115/S145, sdk-nrf-bm) = 호스트+컨트롤러 통합, `sd_ble_*` API, HCI 없음 ← **우리가 쓰는 것**
- `SoftDevice Controller` (nrfxlib, NCS) = 컨트롤러 전용, HCI 노출 ← 다른 물건

### 5.1 NU54DK_Arduino_Core — 배제하되 반드시 읽을 것

같은 칩(nRF54L15)으로 다른 아키텍처를 선택해 배포까지 간 사례. v0.3.0 stable, NCS v3.4.0 / Zephyr 4.4.0, MIT.
**경쟁 대상이 아니라 참조 자료다.**

읽을 것:
- **`platform.txt`** — Arduino 컴파일 recipe를 **소스 그래프 수집기로 전용**하는 기법. `recipe.*.o.pattern`이 실제 컴파일 대신 소스만 record하고 placeholder object를 만들며, `recipe.c.combine.pattern`에서 `sources.cmake`를 생성해 west를 실행한다. 우리는 베어메탈이라 이 기법 자체는 불필요하지만, **업로드 툴 정의 방식(pyOCD/UID/J-Link)은 그대로 참고**하라
- README의 "지원 / 부분 지원 / 미지원" 3단계 범위 표기 → 우리 README도 이 형식 (§11)
- 검증 문서(Fixture 101~103) — 두 보드 통신 기반 UART route 실기 검증. 우리 M2 DoD 참고
- 보드 DTS를 별도 리포(`Nucode01/NU54DK_Zephyr_DTS`)로 분리하고 코어에서 수정 금지하는 규칙
- **SDK 미재배포 전략** — §10 M0의 대안 A 근거

**우리 대비 약점** (= 우리 차별점, 유지할 것):
Windows 10/11 전용 · 부트로더/DFU 없음(v0.6.0 계획, SWD 업로드만) · BLE API가 제3의 자체 API라 마이그레이션 자산 0 · 자사 DK 전용 variant 1개 · `Wire1`/`SPI1`/I2C target 미지원.

**정직하게 인정할 강점**

1. **BLE 스택 품질은 동등하다.** NCS 기반 Zephyr는 SoftDevice Controller(인증됨)를 쓴다.
   우리가 쓰는 SoftDevice S145와 같은 컨트롤러 계열이다.
   → **"우리 방식이 BLE 스택 때문에 우월하다"고 쓰지 마라. 사실이 아니다.**
   (오픈소스 LL 문제는 **mainline** Zephyr 기반인 `ArduinoCore-zephyr`에만 해당한다.
   확인 근거: upstream `hal_nordic`에는 `nrfx`만 있고 `softdevice_controller`가 없다.
   SDC는 `nrfconnect/sdk-nrfxlib`에만 있으며 NCS manifest에서만 끌어온다.
   `ArduinoCore-zephyr`의 `west.yml`은 `arduino/zephyr` 포크를 가리킨다.)

2. **Zephyr 드라이버 생태계를 그대로 쓴다.** 우리는 nrfx 위에 온칩 페리페럴을 직접 짜야 한다.
   다만 이 부담은 과대평가되기 쉽다 — Arduino 사용자는 외부 디바이스에 Zephyr 드라이버가
   아니라 Arduino 라이브러리(`Adafruit_BME280` 등)를 쓴다. 우리가 만들 것은 온칩 래퍼뿐이고
   nrfx가 레지스터 레벨을 이미 다 제공한다. 그래도 M2 작업량은 우리 쪽이 크다.

`lolren/nrf54-arduino-core`도 **참조 자료로 유용하다** (MIT). nRF54L 페리페럴 레지스터 레벨 구현 참고용. 코드를 가져다 쓸 때는 라이선스 고지를 유지하라.

---

## 6. FreeRTOS가 필수인 이유

Adafruit nRF52 코어는 FreeRTOS 위에서 동작하고, 이는 **사용자에게 노출된 API**다.

```cpp
void setup() { Scheduler.startLoop(loop2); }
void loop()  { digitalToggle(LED_RED);  delay(1000); }
void loop2() { digitalToggle(LED_BLUE); delay(500);  }
```

- `cores/nRF5/rtos.h` → `SchedulerRTOS`. `startLoop()`은 스택 크기/우선순위 파라미터 지원
- 스케치에서 `xTaskCreate`, `SemaphoreHandle_t` 직접 사용 흔함
- tickless 모드 + `systemOff()` + `suspendLoop()` → 저전력이 FreeRTOS 구조에 얹혀 있음
- 인터럽트 콜백 지연 처리용 워커 태스크가 loop 태스크와 함께 기동

FreeRTOS를 빼면 `Scheduler` 소실, `delay()`가 busy-wait, 콜백이 ISR 컨텍스트 실행, 저전력 붕괴.

### 6.1 저전력: Adafruit 코드는 그대로 이식되지 않는다

Adafruit의 저전력은 SoftDevice API에 묶여 있는데 **그 API들이 S145에 없다.**
`nrf_soc.h`의 SVCALL을 전수 조사해 확인했다. 앱이 벡터 테이블·NVIC를 직접 소유하게 되면서
SoftDevice가 NVIC를 가상화할 이유가 사라진 결과다 (§7 F1 참조).

| Adafruit가 쓰던 것 | S145(nRF54L) | 대체 |
|---|---|---|
| `sd_app_evt_wait()` | **없음** | `__WFI()` 직접 |
| `sd_nvic_critical_region_enter/exit()` | **없음** | BASEPRI 직접 조작 (§7 F9) |
| `nrf_nvic.h` | **헤더 자체가 없음** | 앱이 NVIC 소유 |
| `sd_power_system_off()` | **없음** | `nrf_regulators_system_off()` |
| `NRF_POWER->RESETREAS` | **없음** | `nrf_resetinfo` / `nrfx_reset_reason_*` |
| `NRF_POWER->GPREGRET` | **있다** — 내가 없다고 적었던 건 오류 (`POWER_GPREGRET[2]` @ 0x500) | 그대로 쓴다. F7 참조 |
| — | `sd_power_mode_set()` 은 **있음** | `NRF_POWER_MODE_LOWPWR` |

**근거로 삼을 소스 3종. 추측하지 말고 이걸 읽고 짜라:**

| 소스 | 무엇을 가져오나 |
|---|---|
| sdk-nrf-bm `samples/bluetooth/ble_pwr_profiling/src/main.c` | Nordic 자신의 nRF54L 저전력 BLE 레퍼런스. idle 루프와 System OFF 시퀀스의 정답 |
| Zephyr `drivers/timer/nrf_grtc_timer.c` | GRTC 틱의 정답. `bm_timer`가 결국 이 드라이버 위에 있다 |
| Adafruit `cores/nRF5/freertos/portable/CMSIS/nrf52/port_cmsis_systick.c` | `vPortSuppressTicksAndSleep()`의 FreeRTOS 쪽 골격과 틱 보정 로직 |

`ble_pwr_profiling`의 idle 루프는 `while (true) { k_cpu_idle(); }` 뿐이다.
**SoftDevice가 켜진 상태에서도 `sd_app_evt_wait()`를 부르지 않는다.** 우리도 평범한 `__WFI()`로 잔다.
같은 샘플의 `prj.conf`는 전력 최적화 명목으로 `CONFIG_CONSOLE=n`을 건다 →
**`Serial`(UARTE30)이 켜져 있으면 바닥 전류가 올라간다.** `Serial.end()` 경로를 만들고 README에 명시하라.

**`systemOff()` 시퀀스** — Nordic `poweroff()`를 그대로 따른다:
1. 출력 정리 → 2. `nrf_gpio_cfg_sense_input()` 기상 핀 설정
→ 3. **`nrfx_ram_ctrl_retention_enable_set(..., false)` 전 RAM 리텐션 해제**
→ 4. `nrfx_reset_reason_clear(UINT32_MAX)` → 5. `nrf_regulators_system_off(NRF_REGULATORS)`

3번은 System OFF 전류를 좌우한다. **Adafruit는 이걸 주석 처리해 둔 채 방치했다** — 따라 하지 마라.
단 M4에서 F7 방법 2(`.noinit` 플래그)를 쓰려면 그 영역만 리텐션을 남겨야 한다.

---

## 7. 알려진 함정

작업 중 반드시 부딪힌다. 해당 작업 시작 전에 이 항목을 다시 읽어라.

### F1. SVC 핸들러 — **번호 충돌이 실제로 있다 (실기 확인)**

`s145_API/include/nrf_svc.h`:
> The SVCs with SVC numbers **0x00-0x0F are forwarded to the application**. All other SVCs are handled by the SoftDevice.

`SDM_SVC_BASE = 0x10`, `SOC_SVC_BASE = 0x20`.

**⚠ 문서 조사만으로는 "충돌 없음"으로 보였으나 실기에서 틀렸다.**
FreeRTOS **11.3.1** 의 `ARM_CM33_NTZ` 포트는 `portSVC_*` 를 **100~105** 로 정의한다
(`portmacrocommon.h`). 구버전은 0~4 였고 11.x 에서 옮겨졌다. 100 이상은 SoftDevice 영역이다.

실제 증상: `vStartFirstTask` 의 `svc 102` 가 디스패처의 SoftDevice 분기를 타고,
`softdevice_vector_forward_address` 가 0 이라 `[0 + 8]` = 벡터[2] = `NMI_Handler` 로
점프해 무한루프. **폴트가 나지 않아 증상만으로는 원인을 알 수 없다.**

→ `portmacrocommon.h` 의 SVC 번호를 **0~5 로 옮겼다** (`freertos/PATCHES.md` §2).
  FreeRTOS 를 올릴 때마다 이 값이 또 바뀌었는지 확인하라.

진짜 함정은 다른 데 있다. **nRF54L은 nRF52와 구조가 반대다** — 애플리케이션이 벡터 테이블을 소유하고
필요한 IRQ를 SoftDevice로 **포워딩**한다 (`nrf_sd_isr.h`의 `NRF_SD_ISR_OFFSET_*`).
sdk-nrf-bm `subsys/softdevice_handler/irq_forward.s`가 그 구현인데,
거기 `SVC_Handler`는 **SVC 번호를 보지 않고 전부 SoftDevice로 넘긴다.**

→ **그 핸들러를 그대로 쓰면 FreeRTOS가 죽는다.** 스택된 PC에서 SVC immediate를 읽어 분기하는
자체 `SVC_Handler`를 작성하라: `< 0x10` → `vPortSVCHandler_C`, `>= 0x10` → SoftDevice 포워딩.
구현은 `cores/nrf54l/nordic/sd_svc_dispatch.S`.

### F2. 인터럽트 우선순위 — **실측값 확보**

근거: sdk-nrf-bm `subsys/softdevice_handler/irq_connect.c`.

| 우선순위 | SoftDevice 점유 | 성격 |
|---|---|---|
| **0** | `RADIO_0`, `TIMER10`, `GRTC_3` | zero-latency. `sd_softdevice_enable()`이 덮어씀 |
| **4** | `AAR00_CCM00`, `CLOCK_POWER`, `ECB00`, `SWI00`, `SVCall` | non-time-critical |

→ FreeRTOS 설정:
- `configMAX_SYSCALL_INTERRUPT_PRIORITY` = **5** (SD의 4보다 낮은 긴급도)
- `configKERNEL_INTERRUPT_PRIORITY` = **7** (최저). PendSV/GRTC 틱
- `...FromISR`을 부르는 앱 ISR은 **5~7만** 허용
- `__NVIC_PRIO_BITS`는 MDK 헤더에서 실측 확인 후 BASEPRI 시프트에 반영

틀리면 **랜덤 크래시**로 나타나 추적이 매우 어렵다. **왜 그 값인지 주석으로 근거를 남겨라.**

### F3. 틱 소스 — GRTC. nRF52보다 오히려 쉽다

SysTick은 저전력 모드에서 정지하므로 못 쓴다. **GRTC SYSCOUNTER는 64비트 / 1 MHz**
(`NRF_GRTC_SYSCOUNTER_MAIN_FREQUENCY_HZ`). nRF52 RTC1의 24비트 래핑 보정 코드가 통째로 사라진다.

- SD가 **CC7~11과 `GRTC_3_IRQn`**을 쓴다 (`nrf_sd_def.h`). 앱은 **CC0~6 + 다른 인터럽트 그룹**을 쓴다
- 평시·tickless 모두 **항상 CC 비교**로 처리한다 (Zephyr `nrf_grtc_timer.c`와 동일 구조).
  tickless가 특수 경로가 아니라 기본 동작의 연장이 된다
- `configTICK_RATE_HZ = 1000` — 1 MHz라 틱당 정확히 1000 µs. `millis()`가 오차 없이 떨어진다
- `micros()`는 틱이 아니라 SYSCOUNTER를 직접 읽는다
- **⚠ `AUTOEN` 을 켜 두어야 한다. SoftDevice 의 요구사항이다 (실기에서 겪음).**
  끄고 명시적 active request 만 걸어 두면 `sd_softdevice_enable()` 이
  **`NRF_ERROR_SDM_INCORRECT_GRTC_CONFIGURATION`(0x1003)** 으로 실패한다.
  `nrf_sdm.h` 원문: *"GRTC is not running with SYSCOUNTER on or AUTOEN is not set"*.
  → `NRFX_GRTC_CONFIG_AUTOEN = 1`. Zephyr 의 `nrf_grtc_timer.c` 도 같은 구성이다.
  M1 타이밍에는 영향이 없다 (실측 −19 ppm, 틱 0.0 ppm)
- **⚠ `CLKSEL` 은 SystemLFCLK 로 둔다.** LFXO 를 직접 지정하지 마라 —
  SoftDevice 가 LFCLK 를 관리한다는 전제와 어긋난다. 정확도 손해는 없다
  (`lfclk_start()` 가 이미 시스템 LFCLK 를 LFXO 로 맞춘다)
- **⚠ 채널 마스크와 개수를 함께 고쳐라 (실기에서 겪음)**:
  `nrfx_grtc_init()` 은 `popcount(NRFX_GRTC_CONFIG_ALLOWED_CC_CHANNELS_MASK)` 와
  `NRFX_GRTC_CONFIG_NUM_OF_CC_CHANNELS` 가 다르면 `-ECANCELED` 로 실패한다.
  마스크만 바꾸면 안 된다. 우리는 `0x7F` / `7`.
  참고로 nrfx 템플릿 기본값 `0x0f0f` / `8` 은 **CC8~11 을 포함해 SoftDevice 영역과 겹치므로**
  절대 그대로 쓰면 안 된다
- **⚠ `safe_setting`**: CC를 현재보다 **뒤로 미룰 때**(tickless 진입이 정확히 이 경우) 직전 CC가 가까우면
  **가짜 COMPARE 이벤트가 뜬다.** Zephyr는 `(int64_t)(prev_cc - now) < LATENCY_THR_TICKS`일 때
  `nrfx_grtc_syscounter_cc_abs_set(..., safe_setting=true)`를 쓴다. **같은 판정을 반드시 넣어라.**
  빠뜨리면 슬립이 즉시 깨지는 형태로 나타난다

### F4. FreeRTOS 포트 선택
`ARM_CM33_NTZ` 사용 (TrustZone 미사용 버전). `ARM_CM33`(TrustZone) 아님. R5 참조.
**sdk-nrf-bm에는 FreeRTOS 포트가 없다** (조사 완료). `lib/bm_scheduler`·`lib/bm_timer` 같은
베어메탈 라이브러리뿐이므로 M1 작업량은 줄지 않는다.

### F5. RRAM 16바이트 정렬
DFU 전송 청크를 16의 배수로. 마지막 잔여분 패딩 필수.

### F6. bit-banging 라이브러리는 동작하지 않는다
NeoPixel, DHT, OneWire, SoftwareSerial 등. SoftDevice가 최상위 인터럽트 우선순위를 점유하고 라디오 이벤트 중 애플리케이션을 블로킹한다. FreeRTOS 컨텍스트 스위칭이 더해진다.

**이건 고치는 게 아니라 문서화 대상이다.** 시간을 쓰지 마라. PWM+EasyDMA 기반 대안 예제를 동봉하고 README에 명시하라.

### F7. UART 공유 / 부트로더 진입
스케치의 `Serial`과 부트로더 진입이 같은 UART를 공유한다 (M4 이후). 진입 방법:
1. GPIO strap (**기본 채택** — 커스텀 보드이므로)
2. **`NRF_POWER->GPREGRET` + 리셋** → `reboot_to_bootloader()` API 제공.
   nRF52 방식이 그대로 통한다 (`POWER_GPREGRET[2]` @ 0x500 실재 확인).
   `.noinit` RAM 보다 낫다 — `systemOff()` 의 RAM 리텐션 해제와 충돌하지 않는다
3. 매직 시퀀스 감지 (오검출 위험, 보조 수단)

**⚠ Arduino 의 1200bps touch 는 여기서 동작하지 않는다.**
USB 네이티브 보드에서는 호스트가 보레이트를 바꾸면 MCU 의 USB 스택이 line-coding 변경을
직접 보지만, UART 브리지에서는 CP2102N 이 자기 쪽 클럭만 바꾸고 **MCU 는 그 사실을 알 수 없다.**
`boards.txt` 에 `use_1200bps_touch=false` 를 둔 이유다. 방법 3 은 in-band 매직 바이트여야 한다.

방법 2·3 의 공통 한계: **스케치가 살아 있어야 한다.** 크래시했거나 인터럽트를 막고 도는
루프에 빠지면 DFU 진입이 불가능하다. 물리 strap 이나 SWD 복구 경로를 반드시 남겨라.

### F8. 디버거가 System OFF를 방해한다
Active SWD 연결 상태에서는 System OFF 진입과 reset cause 판정이 정상 동작하지 않을 수 있다.
저전력 관련 테스트는 프로브를 분리하고 수행하라. 모르면 "System OFF가 안 된다"고 잘못 결론낸다.

하드웨어적 근거: nrfx `nrf_regulators_system_off()` 구현 자체에
`/* Solution for simulated System OFF in debug mode */ while (true) { __WFE(); }` 폴백이 들어 있다.

### F9. 슬립 창에서 BASEPRI를 걸어 두면 WFI가 깨지 않는다 — **실기에서 잡음**

> ⚠ 이 항목은 M1 초기에 **정반대로 적혀 있었다**("PRIMASK를 쓰지 마라, BASEPRI를
> 우선순위 1로 올려라, BASEPRI로 마스크된 인터럽트도 WFI를 깨운다").
> 마지막 문장이 **틀렸다.** 실기에서 tickless가 통째로 죽는 원인이었다.

**ARM 사양**: WFI 기상 조건은 **PRIMASK를 무시하지만 BASEPRI는 무시하지 않는다.**
ARMv7-M ARM B1.5.19 (ARMv8-M 동일):

> "the assertion of an asynchronous exception that has sufficient priority to cause
> exception entry **when the value of PRIMASK is 0**. This means the value of PRIMASK
> does not affect whether an asynchronous exception is a WFI wake-up event, **but the
> values of FAULTMASK, BASEPRI, and the exception enables do affect this.**"

틱 CC는 커널 우선순위 7이라 `BASEPRI = 1 << (8 - PRIO_BITS)` 에 가려진다.
그 상태로 `__WFI()` 하면 **영영 깨지 않는다.**
FreeRTOS의 표준 ARM_CM 포트가 `vPortSuppressTicksAndSleep()` 에서만
`cpsid i`(PRIMASK)를 쓰는 이유가 정확히 이것이다.

**실측 증상** (원인 파악에 가장 오래 걸린 항목):

- 틱이 25~250 tick/s 로 떨어진다 (목표 1000). 값이 잴 때마다 다르다
- `Serial` 이 완전히 죽는다. 폴트도 assert 도 없다
- 계측하면 WFI 한 번이 **수 초** 지속된다 (max 8.1 s, `xExpectedIdleTime` 은 500 ms).
  CC 만료로 깬 횟수는 20초에 1회
- **실제로 깨우는 것은 디버거 attach 뿐이다.** 그래서 SWD 로 들여다보면
  "지금은 정상"으로 보인다 — CC 는 미래에 정확히 무장돼 있고, CCEN=1,
  INTEN 비트 켜짐, NVIC ISER 켜짐, pending 없음. 레지스터만 봐서는 못 찾는다
- 격리 시험으로 함수 본문을 `__WFI()` 하나로 줄이면 **정확히 1000 tick/s** 로 돈다.
  이 시험이 결정적이었다

**→ 두 마스크를 나눠 쓴다** (`port_grtc.c` 구현):

| 구간 | 마스크 | 이유 |
|---|---|---|
| `eTaskConfirmSleepModeStatus()` ~ CC 장전 | **BASEPRI**(우선순위 1) | 앱 IRQ만 막아 레이스 차단. SD의 우선순위 0 zero-latency IRQ(RADIO_0/TIMER10/GRTC_3)는 계속 서비스 |
| WFI 전후 몇 명령어 | **PRIMASK** + `BASEPRI = 0` | BASEPRI를 내려야 CC가 깨울 수 있다. 그 사이 레이스는 PRIMASK가 막고, PRIMASK는 기상을 막지 않는다 |
| 기상 후 틱 보정 | **BASEPRI** 복귀 후 PRIMASK 해제 | 이 순서라야 SD 우선순위 0 핸들러가 곧바로 실행된다 |

```c
__set_BASEPRI_MAX(SLEEP_BASEPRI);          /* 앱 IRQ 차단 */
if (eTaskConfirmSleepModeStatus() == eAbortSleep) { 복구; return; }
/* ... CC 장전 ... */
__disable_irq();  __set_BASEPRI(0U);       /* 순서 중요 */
__DSB(); __ISB();
__WFI();
__set_BASEPRI_MAX(SLEEP_BASEPRI);  __DSB(); __ISB();
__enable_irq();                            /* SD prio-0 즉시 실행 */
/* ... 틱 보정 ... */
__set_BASEPRI(prev_basepri);
```

SD 우선순위 0 IRQ가 지연되는 구간은 WFI 기상 직후 PRIMASK를 푸는 몇 명령어뿐이다.
Adafruit의 `sd_nvic_critical_region_enter()` 는 S145에 없고(§6.1), nRF52의 SD가
NVIC를 가상화해서 앱 인터럽트만 가릴 수 있었던 것이라 nRF54L에는 그 방법 자체가 없다.

`NVIC->ISPR` 폴링은 하지 않는다. nRF54L은 IRQ가 269번까지 있어
Adafruit의 `ISPR[0]|ISPR[1]` 관용구는 애초에 쓸 수 없다.

**✅ 검증됐다 (2026-09-06, `docs/HIL/M3-softdevice.md`).**
advertising 60초 연속 + 실제 연결 성립 상태에서 틱 vs SYSCOUNTER **0.0 ppm**,
Δmillis 전부 정확히 2000. **이 구조를 바꿀 이유가 없다.**
단 라디오의 zero-latency IRQ 가 `loop()` 을 선점해 Δmicros 에 **±29 µs 지터**가
생긴다 (SD 없을 때는 ±1 µs). 누적되지 않으므로 정확도 문제는 아니다.

### F9b. tickless 틱 보정은 기상 시각이 아니라 **틱 그리드** 기준으로

`completed = elapsed / CYCLES_PER_TICK` 후 다음 CC를 `exit_cnt + CYCLES_PER_TICK`
으로 잡으면, 매 슬립마다 1틱 미만의 나머지가 버려져 `millis()` 가 조금씩 느려진다.

**실측 +253 ppm.** LFXO로 어렵게 잡아 둔 +25 ppm(§7 F12)을 통째로 날린다.
크래시도 없고 `millis()`/`micros()` 를 각각 보면 정상이라, **둘의 차이를 봐야 드러난다.**

→ "다음 틱의 절대 시각"(`m_last_cc`)을 1 ms 그리드 위에 유지하고,
기상 후에도 `grid_next + completed * CYCLES_PER_TICK` 로 재장전한다.
`xExpectedIdleTime` 틱을 자려면 CC는 `grid_next + (n-1) * CYCLES_PER_TICK` 에 건다
(마지막 1틱은 `vTaskStepTick()` 이 센다).
수정 후 틱 vs SYSCOUNTER 편차 **0 ppm**.

### F10. nrfx 4.x 사용 규칙 — **M2 착수 전에 반드시 읽어라**

M1 에서 UARTE 와 GRTC 를 붙이며 세 시간을 태운 함정들이다.
**M2 에서 SPIM / TWIM / PWM / SAADC / GPIOTE 를 붙일 때 똑같이 반복된다.**
nrfx 3.x 예제나 인터넷 코드를 그대로 옮기면 전부 걸린다.

#### ① 반환값은 POSIX errno 다

```c
int err = nrfx_grtc_init(...);
if (err == 0 || err == -EALREADY) { /* ok */ }
```

`nrfx_err_t` / `NRFX_SUCCESS` / `NRFX_ERROR_ALREADY` 는 **nrfx 3.x 것이다.**
4.x 는 `int` 를 돌려주고 0 이 성공, 음수가 `-EALREADY` `-ECANCELED` `-EINVAL` `-ENOMEM` 등이다.

#### ② `NRFX_xxx_INSTANCE()` 는 번호가 아니라 **베이스 포인터**를 받는다

```c
static nrfx_uarte_t u = NRFX_UARTE_INSTANCE(NRF_UARTE30);   /* O */
static nrfx_uarte_t u = NRFX_UARTE_INSTANCE(30);            /* X */
```

매크로가 `{ .p_reg = (NRF_UARTE_Type *)reg, ... }` 라서 숫자를 넘기면 **그 숫자가 곧 주소**가 된다.
실제 증상: `p_reg = 30` → 레지스터 접근에서 BusFault, `BFAR = 0x21E`.
컴파일도 경고 없이 통과한다.

#### ③ IRQ 벡터 연결 방식이 페리페럴마다 다르다 — **양방향으로 틀릴 수 있다**

| 유형 | 예 | 해야 할 일 |
|---|---|---|
| 단일 인스턴스 | GRTC | **아무것도 하지 마라.** nrfx 가 정의하는 함수의 심볼 이름이 매크로 체인으로 벡터 이름(`GRTC_2_IRQHandler`)까지 치환된다. 직접 정의하면 **무한 재귀** (`-Winfinite-recursion` 으로 잡힌다) |
| 다중 인스턴스 | UARTE, SPIM, TWIM, PWM | **직접 연결하라.** nrfx 가 `NRFX_INSTANCE_IRQ_HANDLERS` 를 호출하지 않아 벡터 심볼이 MDK 의 weak `Default_Handler` 로 남는다. 인터럽트가 뜨는 순간 무한루프 |

다중 인스턴스는 이렇게 잇는다:

```c
extern "C" void SERIAL30_IRQHandler(void) { nrfx_uarte_irq_handler(&_uarte30); }
```

> nRF54L 에서 UARTE/SPIM/TWIM 은 같은 SERIAL 인스턴스를 공유하므로 벡터 이름이
> `UARTE30_IRQHandler` 가 아니라 **`SERIAL30_IRQHandler`** 다 (`UARTE30_IRQn == SERIAL30_IRQn`).

**확인 방법**: 링크 후 `arm-none-eabi-nm` 으로 심볼 타입을 본다.
`T` 면 연결됨, **`W`(weak) 면 미연결**이다.

```
00001384 T SERIAL30_IRQHandler   <- OK
00003294 W SERIAL30_IRQHandler   <- Default_Handler. 인터럽트 뜨면 죽는다
```

#### ④ 페리페럴 추가 체크리스트 (M2)

- [ ] `nrfx_config.h` 에 `NRFX_xxx_ENABLED` 와 `NRFX_xxxNN_ENABLED` **둘 다**
- [ ] 채널/인스턴스 마스크를 바꿨다면 개수 매크로도 같이 (F3 ①)
- [ ] IRQ 우선순위는 **5~7** (F2). `...FromISR` 을 부르면 필수
- [ ] `NRFX_xxx_INSTANCE(NRF_xxxNN)` — 포인터로
- [ ] 다중 인스턴스면 `SERIALnn_IRQHandler` 등을 직접 연결
- [ ] 링크 후 `nm | grep IRQHandler` 로 `T` 인지 확인
- [ ] SoftDevice 예약 자원(`nrf_sd_def.h`)과 겹치지 않는지 (§7 F2 표, DPPI/PPIB 포함)

### F11. 부트로더를 넣으면 앱이 0x0 에서 밀려난다 — **nRF52 와 정반대**

M4 착수 전에 반드시 읽어라. 실측으로 확정된 제약이다.

| | nRF52 | **nRF54L15** |
|---|---|---|
| 0x0 에 있는 것 | MBR | **애플리케이션** (부트로더 생기면 부트로더) |
| 부트로더 위치 | 상단. MBR 이 `UICR.BOOTLOADERADDR` 로 찾아감 | **0x0** (CPU 가 거기서 부팅) |
| MBR | 있음 | **없음** (MDK 에 심볼 자체가 없다) |
| `UICR.BOOTLOADERADDR` | 있음 | **없음** |
| SoftDevice | 하단 (앱 아래) | **상단, `0x0015A800` 고정** |

**SoftDevice 주소는 hex 에 절대 주소로 박혀 있다** (실측: `0x0015A800` ~ `0x0017C4F8`,
135.2 KB). 옮길 수 없다.

M4 의 레이아웃은 이렇게 된다:

```
0x00000000  Bootloader              <- CPU 가 여기서 부팅
0x000?????  Application             <- 부트로더가 점프. VTOR 재배치 필요
0x00158800  peer_manager  4 KB      <- 앱 파티션 밖. 앱 업데이트해도 본딩 유지
0x00159800  storage0      4 KB
0x0015A800  SoftDevice  137 KB      <- 고정
```

**이미 처리해 둔 것:**
- `peer_manager` / `storage0` 가 앱 파티션 **밖**에 있다. single-bank 업데이트로
  본딩이 날아가지 않는다 (Nordic DTS 를 따른 결과)
- `cores/nrf54l/wiring.c` 의 `init()` 이 `SCB->VTOR` 을 링커 심볼
  `__vectors_start` 로 설정한다. **앱 시작 주소가 바뀌어도 자동으로 따라간다.**
  MDK 스타트업은 VTOR 을 건드리지 않으므로, 없으면 앱이 밀리는 순간
  인터럽트가 부트로더 벡터로 간다

**M4 에서 정하면 되는 것** (지금 정할 근거가 없다):
- 부트로더 크기 → 앱 시작 주소. 실제로 만들어 봐야 안다
- dual-bank 여부. RRAM 은 erase 가 없어 swap 특성이 flash 와 다르다 (R9)

바뀌는 것은 링커 스크립트의 `FLASH ORIGIN` 한 줄과 `boards.txt` 의
`upload.maximum_size` 한 줄뿐이다. 미리 숫자를 찍어두는 것보다 제약을 아는 게 중요하다.

### F12. LFCLK 소스를 설정하지 않으면 조용히 0.9% 틀린다

**실측으로 잡은 문제다. 크래시도 없고 로그도 정상으로 보인다.**

리셋 직후 GRTC 의 `CLKCFG.CLKSEL`(bit **16**, bit0 아님)은 SystemLFCLK 이고,
시스템 LFCLK 는 아무 설정도 안 하면 내부 RC 로 돈다.

| 설정 | 실측 오차 |
|---|---|
| 미설정 (내부 RC) | **+9000 ppm** (0.9%) |
| LFXO 선택 후 | **+25 ppm** (측정 잡음 ±3 ppm) |

10분 시험에서 `millis` 델타는 전부 정확히 2000 이었고 `micros` 와도 완벽히 일치했다.
**타깃 내부에서는 아무 이상이 없다.** 호스트 시계와 비교해야만 드러난다.

BLE 는 보통 ±250 ppm 이하를 요구하므로 **RC 로는 M3 에서 연결이 끊긴다.**

구현: `cores/nrf54l/freertos/port_grtc.c` 의 `lfclk_start()`.
`variant.h` 의 `USE_LFXO` / `USE_LFRC` 로 분기한다.

> **크리스털 없는 보드**: `USE_LFRC` 로 빌드된다. 다만 위 수치대로 정확도가
> 두 자릿수 나빠지므로 **BLE(M3)에는 쓸 수 없다.** LED/UART 수준의 용도만 가능하다.
> variant 를 만들 때 이 트레이드오프를 README 에 명시하라.

**측정 방법 주의**: 시리얼로 정확도를 잴 때 `read(n)` 은 타임아웃까지 블록해서
도착 시각이 양자화된다. `readline()` 으로 받고 즉시 타임스탬프를 찍어라.
처음에 이걸 틀려서 -3432 ppm 이라는 무의미한 값을 얻었다.

---

### F13. Arduino 빌드 시스템 함정 — **수동 빌드로는 절대 안 드러난다**

`arduino-cli` 로 실제 업로드해 보고서야 잡은 것들이다.
`.o` 를 직접 링크하는 수동 빌드에서는 전부 정상이었다.

#### ① weak 심볼 오버라이드가 아카이브 경계를 넘지 못한다 — **가장 위험**

Arduino 는 `cores/` 전체를 **`core.a` 아카이브**로 묶는다.
MDK 스타트업은 벡터 테이블과 **weak 기본 핸들러를 같은 오브젝트에** 정의한다.
그 오브젝트가 링크되는 순간 `SVC_Handler` 등이 이미 정의된 상태가 되고,
**아카이브 멤버는 "미해결 심볼"이 있을 때만 추출**되므로 우리의 강한 정의는
영영 링크되지 않는다.

증상: **빌드는 성공하는데** SVC 가 `Default_Handler` 로 가서 FreeRTOS 가
시작조차 못 한다. 폴트도 안 나고 로그도 없고 LED 도 안 켜진다.
`nm` 으로 보면 우리 심볼(`g_fault` 등)이 ELF 에 아예 없다.

→ `platform.txt` 의 링크 recipe 에서 **`-Wl,--whole-archive`** 로 core.a 를 감쌌다.
  `-ffunction-sections` + `--gc-sections` 이 있어 크기 손해는 거의 없다.

**진단법**: 링크 후 `nm <elf> | grep <핸들러>`.
없거나 `W` 면 미링크다. `T` 여야 한다.

#### ② `cores/` 아래 모든 소스가 무조건 컴파일된다

파일을 빌드에서 빼는 방법이 없다. 그래서:
- **가드 없는 nrfx 소스**가 빌드를 깨뜨린다 (`nrfx_adc.c` 등 21개 제거)
- **서로 배타적인 대안 구현**이 중복 정의를 낸다
  (스타트업 2벌, GPPI 3벌, COMP/LPCOMP 공유 IRQ)

내역과 판정 방법은 `cores/nrf54l/nordic/nrfx/VENDORING.md`.

#### ③ `-T"path"` 가 아니라 `"-Tpath"` 로 써야 한다

arduino-cli 는 recipe 를 셸 없이 토큰 단위로 실행한다.
`-T"{path}"` 는 **따옴표가 파일명에 포함**되어 `cannot open linker script file` 이 난다.
`-L` `-I` `-Wl,-Map,` 도 마찬가지다. **인자 전체를 감싸라.**

#### ④ `compiler.libraries.ldflags` 기본값을 선언해야 한다

선언하지 않으면 라이브러리가 없을 때 치환되지 않고 문자열 그대로 링커에 넘어간다.

---

## 8. 참조 구현 매핑

`adafruit/Adafruit_nRF52_Arduino`를 구조 참조로 사용한다. 새로 설계하지 말고 이식하라.

| Adafruit (nRF52) | 이 프로젝트 (nRF54L) | 비고 |
|---|---|---|
| `cores/nRF5/` | `cores/nrf54l/` | |
| `cores/nRF5/rtos.h` | 동일 API 유지 | 호환 핵심. 시그니처 바꾸지 말 것 |
| `cores/nRF5/freertos/` | FreeRTOS + `ARM_CM33_NTZ` | 틱을 GRTC로 교체 (§7 F3) |
| `port_cmsis_systick.c` (RTC1) | `port_grtc.c` (GRTC) | tickless 골격은 유지, SD 의존부만 치환 (§6.1, §7 F9) |
| `libraries/Bluefruit52Lib/` | **`libraries/Bluefruit54Lib/`** — 동일 API, `sd_ble_*` 재구현 | S145 기준. 이름을 54 로 둔 이유는 §8.2 |
| nRF5 SDK 드라이버 | nrfx 4.x | nrfx는 Zephyr 없이 standalone 사용이 정식 지원됨 |
| S132 / S140 | **S145 v10.0.1 고정** (peripheral+central) | S115는 메뉴 항목 추가만으로 확장 가능하게 변수화 |
| `Adafruit_nRF52_Bootloader` | 자체 UART DFU 부트로더 (M4) | 구조 참고 |
| `adafruit-nrfutil` | 호스트 업로드 툴 (M4) | 패키지에 바이너리 동봉하는 방식 참고 |
| `Adafruit_TinyUSB` (Serial) | **이식 대상 아님** | R10 |
| `Adafruit_LittleFS` + `InternalFileSystem` | **부분 이식.** §8.1 참조 | 본딩 저장에만 필요하다. RRAM 에는 erase 가 없어 그대로 못 올린다 (R9) |

### 8.3 Adafruit 대비 달라진 점

**BLE 기능이나 성능이 더 낫다는 뜻이 아니다.** 아직 기능은 한참 모자란다
(docs/STATUS.md 의 B 단계 표). 여기 적는 것은 구조와 진단 방식의 차이이고,
nRF54L 로 옮기면서 **다르게 하기로 한 것**이다.

**1. SoftDevice 계층을 한 파일에 격리했다** — `cores/nrf54l/ble/sd_event_pump.c`

Adafruit 은 SoftDevice enable / 이벤트 디스패치 / 인터럽트 우선순위를
`bluefruit.cpp` 안에 API 구현과 섞어 둔다. 우리는 분리했다.
백엔드(SoftDevice)를 갈아치우면 통째로 버려질 코드이므로 흩뿌리지 않는다.
Bluefruit API 쪽은 관찰자로 붙기만 한다 (`sdBleObserverAdd`).
**추상화 레이어가 아니라 코드 배치 규칙이다** — R11 위반이 아니다.

**2. 진단 상태를 SWD 로 읽을 수 있게 남긴다**

Adafruit 은 `VERIFY_STATUS` 매크로 + `Serial` 로그에 의존한다. 그런데
**`Serial` 이 죽은 상태에서는 아무것도 못 본다.** 실제로 A 단계에서
"출력이 하나도 없다" 로 시작해 오진했다.

그래서 전역에 흔적을 남긴다. probe-rs 가 halt 를 유지하지 못하므로
(docs/HIL/M1-nu54dk.md §5) 이 방식이 더 확실하다.

| 전역 | 무엇 |
|---|---|
| `g_sd_stage` | `sdEnable()` 이 어디까지 갔는지 |
| `m_last_error` | 마지막으로 실패한 SoftDevice API 의 오류 코드 |
| `g_sd_ram_required` | `sd_ble_enable()` 이 요구한 최소 앱 RAM 시작 주소 |
| `g_sd_evt_buf_too_small` | 이벤트가 버퍼보다 커서 못 꺼냈을 때 |
| `g_sd_fault_id/pc/info` | SoftDevice 치명적 오류 |
| `g_fault`, `g_assert_file/line` | 예외 프레임 / assert 위치 (M1 부터) |

**3. HardFault 를 SoftDevice 로 포워딩하지 않는다**

sdk-nrf-bm 은 넘긴다. 우리는 `fault_handler.c` 의 기록기를 살린다.
개발 중에는 폴트가 났을 때 스택 프레임과 CFSR 을 남기는 쪽이 낫다.
근거와 되돌리는 법은 `nordic/sd_irq_forward.S` 주석에 있다.

**4. 본딩 저장 위치를 앱 파티션 밖으로 잡아 뒀다**

`docs/MEMORY-MAP.md` 가 `peer_manager` 4 KB 를 앱 파티션 **밖**에 두었다
(Nordic DTS 를 따른 결과). 앱을 갱신해도 본딩이 날아가지 않는 것이
**구조적으로 보장된다.** 파일시스템 없이 고정 레코드로 넣을 수 있다는 뜻이기도
하다 (§8.1).

### 8.2 왜 `Bluefruit54Lib` 인가

폴더 이름을 원본과 같은 `Bluefruit52Lib` 로 두지 않았다.

**호환성에는 영향이 없다.** Arduino 는 `src/` 안의 **헤더 이름**으로 라이브러리를
찾고 스케치가 쓰는 것은 `#include <bluefruit.h>` 다. 폴더 이름은 사용자가
타이핑하지 않는다. R12 를 해치지 않는다.

이름의 독자는 셋이다 — 저장소를 읽는 사람, 빌드 로그, Library Manager.
셋 모두에게 `Bluefruit52Lib` 는 **"Adafruit 것을 vendoring 했다"** 로 읽히는데
사실이 아니다. 이 저장소는 `nrfx/VENDORING.md` · `freertos/PATCHES.md` 처럼
**가져온 것과 새로 쓴 것을 구분해 적는** 방식을 쓰므로 이름도 그래야 한다.
Adafruit nRF52 코어를 함께 설치한 사용자의 빌드 로그에서 구분되는 이점도 있다.

⚠ 잃는 것: 제3자 라이브러리가 `depends=Bluefruit52Lib` 를 선언하면 못 찾는다.
그런 라이브러리는 nRF52 용이라 어차피 이 코어에서 돌지 않을 가능성이 크다.

### 8.1 파일시스템 — 무엇 때문에 필요한가

Adafruit 예제들이 `#include <Adafruit_LittleFS.h>` / `<InternalFileSystem.h>` 를
달고 있는데 **정작 스케치에서 쓰지는 않는다.** 이유는 두 가지다.

1. `Bluefruit.begin()` 이 `bond_init()` 을 부르고, 그게 `InternalFS.begin()` 으로
   `/adafruit/bond_prph` · `/adafruit/bond_cntr` 디렉토리를 만든다.
   **본딩 키와 피어 이름이 LittleFS 파일로 저장된다** (`utility/bonding.cpp`).
2. Arduino 는 **스케치가 `#include` 한 라이브러리만 링크한다.** 그래서 직접 쓰지
   않아도 include 가 있어야 링크가 된다. 예제의 저 두 줄은 그 때문이다.

즉 **BLE 본딩을 하기 전까지는 필요 없다.** advertising 과 무본딩 연결에는 안 쓴다.

**RRAM 위에 LittleFS 를 그대로 올릴 수 없다** (R9 / §7 F5):

| | flash (nRF52) | **RRAM (nRF54L)** |
|---|---|---|
| erase | 페이지 단위 필수 | **개념이 없다.** 0xFF 쓰기로 에뮬레이션 |
| write 단위 | 4 B | **16 B** |

올리려면 블록 디바이스 계층에서 `erase()` 를 "0xFF 채우기" 로 구현하고
`prog_size` 를 16 으로 잡아야 한다.

**권장 경로 — 본딩 저장만 먼저, 범용 FS 는 나중에:**

- 본딩 키는 `docs/MEMORY-MAP.md` 가 이미 잡아 둔 **`peer_manager` 4 KB 파티션**
  (`0x00158800`)에 고정 레코드로 넣는다. 앱 파티션 밖이라 앱을 갱신해도 남는다.
  키 하나가 100 바이트 남짓이라 4 KB 면 충분하고, LittleFS 를 no-erase 매체로
  이식하는 것보다 훨씬 간단하다
- `Adafruit_LittleFS.h` / `InternalFileSystem.h` 는 **링크만 되게 하는 호환 헤더**로
  둔다. 예제가 직접 쓰지 않으므로 M3 DoD 에는 이것으로 충분하다
- ⚠ 사용자 스케치가 `InternalFS` 로 **자기 파일을 저장**하려 하면 이 방식으로는
  안 된다. 그때 범용 FS 가 필요해지고, 그건 별도 판단이다 (M5 이후).
  **호환 헤더가 조용히 실패하게 만들지 마라** — 미지원이면 명확히 실패시켜라

### R11 / R12 배경 — Bluefruit은 이미 seam이지만 깨끗하지 않다

Bluefruit52Lib은 사용자 스케치와 백엔드 사이에 이미 앉아 있다. 백엔드를 바꿔야 할 때
Bluefruit 내부 구현을 교체하면 되고 사용자 스케치는 그대로다. 이것이 seam이다.
그래서 **그 아래 또 한 겹을 만들 이유가 없다** (R11).

단, 깨끗한 seam은 아니다. SoftDevice 타입이 공개 헤더로 새어나온다:

- `err_t` 반환값의 실체가 `NRF_ERROR_*`
- `BLEConnection`이 `ble_gap_conn_params_t` 노출
- Advertising API가 `BLE_GAP_ADV_*` 상수 사용
- `SecureMode_t`가 `ble_gap_conn_sec_mode_t` 래퍼
- `Bluefruit.begin(prph, central)`이 SoftDevice `ble_cfg` 연결 수 설정에 직결

**이 누수를 감싸서 없애려 하지 마라.** 그 타입을 직접 쓰는 Adafruit 스케치가 실제로 존재하고,
감싸는 순간 호환이 깨진다. R12가 이 경우를 위한 규칙이다.

읽어야 할 구체적 경로:
- `Adafruit_nRF52_Arduino/cores/nRF5/rtos.h` — SchedulerRTOS API 시그니처
- `Adafruit_nRF52_Arduino/cores/nRF5/freertos/` — SoftDevice 공존 포트 구조
- `Adafruit_nRF52_Arduino/libraries/Bluefruit52Lib/src/` — 재현할 API 표면
- `Adafruit_nRF52_Arduino/libraries/Bluefruit52Lib/examples/` — 호환성 검증용 스케치
- `EIDOSDATA/NU54DK_Arduino_Core/platform.txt` — pyOCD / J-Link 업로드 툴 정의 (§3)
- `lolren/nrf54-arduino-core` — nRF54L 페리페럴 레지스터 레벨 구현

---

## 9. 라이선스 / 배포

### 디렉토리 구조 (이대로 만들 것)

```
baram-nrf54-arduino/                 # 저장소 루트
├── LICENSE                          # MIT (자체 코드)
├── README.md                        # 영문(기본). 라이선스 혼재 명시 필수
├── README.ko.md                     # 한국어. README.md 와 내용을 맞춘다
├── CLAUDE.md                        # 이 문서
├── package_baram_nrf54_index.json   # platforms[0] = nrf54l
├── extras/make_release.sh           # 배포 스크립트 (아카이브에 미포함)
├── docs/                            # 아카이브에 미포함
│   ├── STATUS.md                    # 진행 상황 / 인수인계
│   ├── LICENSE-INVENTORY.md
│   ├── MEMORY-MAP.md                # 칩별 (L05 / L15) RRAM·RAM 배치
│   ├── PERIPHERAL-PINMAP.md         # 칩별 페리페럴↔GPIO 도메인 규칙
│   ├── boards/                      # ★ 보드별 회로도 실측 (보드 하나당 문서 하나)
│   │   └── NU54-DK.md               #   NU54-DK + NU54V-DK (회로도 동일)
│   └── HIL/                         # 실기 검증 기록 (마일스톤 × 보드)
└── nrf54l/                          # ★ 아카이브되는 플랫폼 루트
    ├── platform.txt  boards.txt  programmers.txt  keywords.txt
    ├── cores/nrf54l/
    │   ├── avr/                     # pgmspace 셰임
    │   ├── freertos/                # MIT, 원본 고지 유지
    │   ├── nordic/                  # nrfx + MDK + CMSIS + softdevice_handler 이식본
    │   └── linker/
    ├── libraries/
    ├── variants/nu54dk/
    ├── tools/probe-rs/{macosx,linux,win}/
    ├── bootloader/                  # M4
    └── softdevice/
        ├── LICENSE-Nordic           # Nordic-5-Clause 전문
        ├── LICENSE-attribution      # ARM BSD-3-Clause
        └── s145_nrf54l15_10.0.1_softdevice.hex   # 수정 금지 (R3)
```

`nrf54l/`만 tar로 묶어 배포한다. `docs/`·`extras/`는 저장소에만 둔다.

### Nordic-5-Clause 요약

| 조항 | 내용 |
|---|---|
| 1 | 소스 재배포 시 고지·조건·면책 유지 |
| 2 | **바이너리 재배포 허용.** 동봉 문서에 고지 재현 필요 → SoftDevice hex 번들 가능 |
| 3 | Nordic 이름으로 홍보 금지 → 코어 이름/설명에 "Nordic" 사용 금지 |
| 4 | Nordic IC에서만 사용 가능 |
| 5 | 바이너리 리버스엔지니어링·수정·디스어셈블 금지 |

### README에 반드시 쓸 것
- 코어 자체 코드는 MIT
- 번들된 SoftDevice는 Nordic-5-Clause, Nordic IC 전용
- **"오픈소스"라고 단정하지 말 것** — 4·5항 때문에 OSI 정의를 충족하지 않는다. "혼재 라이선스"로 표기

### 전례 (문제없이 운영 중)
`adafruit/Adafruit_nRF52_Arduino`, Seeed·smartme.io·CAMI 포크들이 SoftDevice를 번들해 Board Manager로 배포 중.

---

## 10. 마일스톤

각 마일스톤은 **Definition of Done을 만족해야 다음으로 넘어간다.**

### M0 — 사전 조사 ✅ **완료 (2026-09-05)**

- [x] `nrfconnect/sdk-nrf-bm` 최신 릴리스 → **v2.0.1** (NCS v3.3.0, SoftDevice **v10.0.1**, S115/S145).
      지원 DK: nRF54L15 / nRF54LM20 / nRF54LS05 / nRF54LV10
- [x] `LICENSE` 및 `components/softdevice/` 라이선스 확인 → **별도 제한 계약 없음.**
      `s145_10.0.1_license-agreement.txt`는 루트 `LICENSE`와 **동일한 Nordic-5-Clause 전문**이고
      `NCS-SBOM-Apply-To-File: ./*.hex`로 hex에 적용됨을 명시한다. 2항이 바이너리 재배포를 명시 허용.
      → **번들 가능. 아래 분기표 첫 행 채택.** `s145_10.0.1_license-attribution.txt`(ARM BSD-3-Clause)도 함께 동봉할 것
- [x] SoftDevice 예약 인터럽트 우선순위 (F2용) → **0과 4.** §7 F2에 근거와 함께 기록
- [x] SVC 충돌 여부 (F1용) → **충돌 있음** (조사 단계의 "충돌 없음" 판단이 틀렸다). FreeRTOS SVC 100~105 를 0~5 로 옮겼다. §7 F1
- [x] sdk-nrf-bm에 FreeRTOS 포트 포함 여부 → **없음.** 직접 포팅 확정 (§7 F4)
- [x] 업로드 툴 조사 → **probe-rs 채택** (§3). pyOCD 0.39도 `nrf54l` 지원하나 Python 의존이라 대비책
- [x] 메모리 맵 확정 → `docs/MEMORY-MAP.md`. 앱이 0x0, SD가 상단 (**nRF52와 반대**)
- [x] 저전력 API 조사 → SD의 `sd_app_evt_wait`/`sd_nvic_*`/`sd_power_system_off` **전부 없음** (§6.1, §7 F9)
- [x] NU54-DK 회로도 분석 → `docs/boards/NU54-DK.md`
- [ ] `reuse` 또는 `scancode-toolkit`으로 전체 라이선스 인벤토리 생성 → `docs/LICENSE-INVENTORY.md`
- [ ] sdk-nrf-bm의 single-bank DFU가 UART 경유를 지원하는지 확인 (지원하면 M4 대폭 축소) — **M4 직전으로 연기**
- [ ] nRF54L의 부트로더 진입 주소 메커니즘 확인 — **M4 직전으로 연기**
- [ ] **NU54-DK로 sdk-nrf-bm 샘플 빌드·플래시 성공** ← 실기 필요. 남은 유일한 M1 선행 조건
- [ ] Nordic `ble_pwr_profiling` 샘플로 **저전력 기준선 전류 측정** (§7 F8 — 프로브 분리)

**DoD**: 위 결과로 이 문서의 §0·§3·§4·§6·§7·§10을 갱신했고(완료), **DK에서 노르딕 샘플이 동작한다**(미완).

**라이선스 확인 결과별 분기** — 조사 결과 **첫 행이 확정**됐다:

| 결과 | 대응 |
|---|---|
| **SoftDevice 바이너리 재배포 가능** ← **확정** | 계획대로. `softdevice/`에 번들 |
| 재배포 불가 | (해당 없음) 대안 A — `post_install`이 Nordic 공식 배포에서 다운로드. `EIDOSDATA/NU54DK_Arduino_Core` 방식 |
| 판단 불가 | (해당 없음) 사람에게 보고하고 대기 |

### M1 — 최소 동작

업로드는 **CMSIS-DAP/probe-rs 또는 J-Link SWD**. 부트로더 없음 (§3).
보드는 **NU54-DK (nRF54L05)** 와 **NU54V-DK (nRF54L15)** 2종이다.

- [x] FreeRTOS 포팅: `ARM_CM33_NTZ`, GRTC 틱, SVC 조정(F1), 우선순위 설정(F2)
- [x] `setup()`/`loop()` 태스크 기동 — `Scheduler.startLoop()` 두 번째 태스크까지 실기 확인
- [x] GPIO: `pinMode` / `digitalWrite` / `digitalRead` — LED 4 / 버튼 4 확인
- [x] `millis` / `micros` / `delay` — 실기에서 델타 정확 (tickless 는 아래 별도)
- [x] UART `Serial` — UARTE30, CP2102N 경유 수신 확인
- [x] `boards.txt` + `platform.txt`에 SWD 업로드 recipe (프로브 UID 지정 옵션 포함)
- [x] **tickless idle 켜기** — `configUSE_TICKLESS_IDLE 1`. 실기에서 millis 델타 = 호스트 델타,
      틱 vs SYSCOUNTER 편차 **0 ppm**. 함정 2건(§7 F9 / F9b)은 `docs/HIL/M1-tickless.md` 에 기록
- [x] **nRF54L05 보드 추가** — 실장 칩이 L05 였다. `nu54dk`(L05) / `nu54vdk`(L15) 두 보드,
      링커 스크립트·SoftDevice hex 분리, variant 공유. `docs/MEMORY-MAP.md`
- [x] **arduino-cli 로 컴파일·업로드** — FQBN `baram-nrf54:nrf54l:nu54dk`.
      실기 확인 완료. 이 과정에서 §7 F13 의 함정 네 개를 잡았다
- [x] 장시간 연속 실행 무크래시 — tickless on 으로 5분 소크, 151샘플 이상 0건,
      틱 vs SYSCOUNTER 0 ppm, 호스트 대비 +38 ppm (tickless off 10분 소크는 이미 통과)

실기 검증 기록은 `docs/HIL/M1-nu54dk.md`.

**DoD**: Arduino IDE Upload 버튼으로 blink + `Serial.println()` 업로드·동작. 10분 연속 실행 시 크래시 없음.
tickless는 **틱이 안정된 뒤에 켠다.** 둘을 동시에 켜면 틱 버그와 슬립 버그가 섞여 원인 분리가 안 된다.
저전력 DoD: tickless on/off 양쪽에서 `millis()` 드리프트 없음, `systemOff()` 후 지정 핀으로 기상,
**프로브 분리 상태**에서 전류 측정 (§7 F8) — 기준선은 Nordic `ble_pwr_profiling`을 같은 보드에 구워서 잡는다.

> M1이 이 프로젝트에서 가장 미끄러운 구간이다. 여기가 안정되기 전에 BLE로 넘어가면 원인 추적이 불가능해진다. 서두르지 마라.

### M2 — Arduino API

- [ ] `analogRead` (SAADC), `analogWrite` (PWM)
- [ ] `Wire` / `Wire1` (TWIM)
- [ ] `SPI` (SPIM00, §4 주의사항 반영)
- [ ] `attachInterrupt` (GPIOTE)
- [ ] `SchedulerRTOS` — Adafruit `rtos.h`와 동일 시그니처
- [ ] AVR 호환 셰임: `avr/pgmspace.h` (`PROGMEM` 빈 매크로, `pgm_read_byte()` 역참조)

**DoD**: Adafruit `rtos_scheduler.ino`가 수정 없이 동작. I2C 센서 라이브러리 1종 동작.
UART/SPI/I2C는 두 보드 간 통신 또는 루프백으로 양방향 데이터를 실기 검증하고 결과를 `docs/HIL/`에 기록.

### M3 — BLE

> **`BLEDfu` 를 빠뜨리지 마라.** Adafruit 예제 대부분이 `bledfu.begin()` 을 포함하므로
> 클래스가 없으면 **컴파일이 실패한다** (R12: 호환 우선).

- [x] **SoftDevice S145 활성화 + 이벤트 처리 태스크** — `cores/nrf54l/ble/sd_event_pump.c`.
      IRQ 포워딩(`nordic/sd_irq_forward.S`), SoC 이벤트, CRACEN TRNG 로 RNG 시딩까지.
      실기 확인: `docs/HIL/M3-softdevice.md`
- [x] **raw advertising 동작** — 공중에서 `BARAM-nRF54L` 탐지, 연결 성립.
      **§7 F9 공존 검증 통과**
- [ ] `Bluefruit.begin()`, advertising (Bluefruit API 계층)
- [ ] `BLEService` / `BLECharacteristic`
- [ ] `BLEUart` (NUS)
- [ ] 페어링 / 본딩
- [ ] **이벤트 펌프 격리** — `sd_ble_evt_get()` 루프, 이벤트 태스크, SoftDevice
      enable/disable, 인터럽트 우선순위 처리를 `cores/nrf54l/ble/sd_event_pump.c`
      한 파일에 모은다. 백엔드를 바꾸면 통째로 버려질 코드이므로 흩뿌리지 마라.
      **추상화가 아니라 코드 배치 규칙이다** (R11 위반 아님)
- [ ] **`BLEDfu` 클래스 제공** — M3 시점에는 부트로더가 없다. `begin()` 이 서비스를
      등록하되 실제 DFU 트리거는 M4 에서 연결한다.
      **명확한 에러를 반환하고 로그를 남겨라. 조용히 성공한 척하지 마라.**
      (`lolren/nrf54-arduino-core` 가 `ERROR_NOT_SUPPORTED` 를 반환하는 방식을 쓴다)
- [ ] `enterOTADfu()` 상당 API 의 시그니처 확보
      (Adafruit `examples/Hardware/dfu_ota/dfu_ota.ino` 참조)

**DoD**: Adafruit `Bluefruit52Lib/examples/Peripheral/bleuart` 원본 스케치가 수정 없이 컴파일·동작하고, 폰에서 연결·송수신된다.

> 목표로만 둘 것: `<bluefruit.h>` 를 include 한 사용자 스케치에서 `ble_gap.h` 가 직접 노출되지
> 않으면 좋다. 다만 **그림자 타입 헤더를 만들어 달성하려 하지 마라** — Bluefruit 내부는 진짜
> 헤더가 필요한데 사용자 쪽에 같은 이름을 다시 정의하면 ODR 위반이 되고, SoftDevice 버전이
> 오를 때마다 전 구조체 레이아웃을 재검증해야 한다. R11·R12 와도 충돌한다.
> 달성 방법은 실제 이식하면서 정한다. 안 되면 포기해도 되는 항목이다.

### M4 — 부트로더 / DFU

CMSIS-DAP 업로드 경로는 **제거하지 말고 병행 유지**한다 (§3).

- [ ] **`caveman99/nRF54_Bootloader` 를 DK 에서 빌드·플래시하고 serial/OTA DFU 동작 확인
      → 채택 여부 결정.** 밑바닥부터 만들기 전에 이것부터 본다.
      nRF54L 에서 MBR 없이 부트로더를 기동하는 유일한 공개 선례다 (MIT, S145).
      단 스타 1개 단일 메인테이너이므로 그대로 의존하지 말고 구조만 참고할 수도 있다
- [ ] RRAM 레이아웃 확정 → `docs/MEMORY-MAP.md`. **제약은 §7 F11 에 이미 정리돼 있다**
      (부트로더가 0x0, SD 는 0x0015A800 고정, VTOR 은 코어가 이미 처리)
- [ ] dual-bank 여부 결정. RRAM 은 erase 가 없어 swap 특성이 flash 와 다르다 (R9).
      실기 검증 후 결정
- [ ] **부트로더 전송 계층을 UART / BLE 교체 가능하게 분리** — UART 하나로 하드코딩하지 마라.
      USB 가 없는 보드에서 최종 사용자에게는 폰 OTA 가 더 자연스러운 경로다
- [ ] UART DFU
- [ ] **BLE OTA DFU** — nRF Connect / nRF Toolbox 로 업로드 가능
- [ ] 부트로더 진입: GPIO strap + `reboot_to_bootloader()`(GPREGRET, F7) + 더블 리셋
- [ ] `BLEDfu` 서비스를 실제 DFU 트리거에 연결 (M3 에서 만든 것)
- [ ] 호스트 업로드 툴 (`adafruit-nrfutil` zip 포맷 호환 검토)
- [ ] `boards.txt` 업로드 방식 메뉴: `CMSIS-DAP (probe-rs)` / `CMSIS-DAP + Probe UID` / `J-Link` / `UART DFU`

**DoD**:
- UART DFU 로 20회 연속 업로드 실패 0회
- **nRF Connect 앱으로 BLE OTA 업로드 성공**
- 업로드 중 전원 차단 후에도 부트로더가 살아 복구 가능
- CMSIS-DAP 경로도 여전히 동작
- **본딩 키가 앱 업데이트 후에도 유지됨** (peer_manager 가 앱 파티션 밖이므로 구조적으로 보장되나 실측할 것)

OTA 제약으로 문서화할 것: Adafruit 부트로더 기준 **Packet Receipt Notification(PRN)이 8 이하**여야 한다.
초과하면 부트로더가 메모리 부족에 빠진다. nRF54L 포팅에서도 유효한지 확인하고 README 에 명시하라.

### M5 — 패키징 / 배포

- [ ] variant: **NU54-DK / NU54V-DK** (`nu54dk` 공유) 우선. 이후 Nordic nRF54L15 DK / generic 모듈
- [ ] `boards.txt`, `platform.txt`, `package_*_index.json`
- [ ] 라이선스 파일 정리 (§9), README
- [ ] 예제 스케치 (Adafruit 예제 포팅)
- [ ] 라이브러리 호환성 컴파일 테스트 결과 → `docs/LIBRARY-COMPAT.md`
- [ ] README에 **지원 범위 밖**을 명시: "이 코어는 BLE 애플리케이션용이다.
      Matter / Thread / Zigbee / LE Audio / 802.15.4는 지원 범위 밖이며 계획에도 없다."
      범위를 넓게 선언하면 나중에 요구가 들어왔을 때 방어선이 없다. NU54DK가 로드맵에
      v0.8.0 Matter를 걸어둔 채 v0.3.0에서 `Wire`/`SPI`가 "부분 지원"인 것이 그 예다

**DoD**: 깨끗한 환경에서 Board Manager URL로 설치 → blink 업로드 성공. Linux/Windows 양쪽에서 확인.

### M6 — LM20A 확장 (선택)

- [ ] nRF54LM20A variant. I2S→TDM 차이 반영. USB 지원 여부 별도 판단

---

## 11. 라이브러리 호환성 정책

- **`architectures=` 불일치는 무시하라.** arduino-cli/IDE는 경고만 띄우고 컴파일은 진행한다. 하드 블록이 아니다
- **AVR 셰임은 반드시 제공하라.** 이것만으로 호환 라이브러리가 크게 늘어난다
- **`ARDUINO_ARCH_NRF52` 정의 여부는 별도 결정 사항.** 정의하면 Bluefruit 생태계 라이브러리 호환성이 오르지만 잘못된 레지스터 접근 위험이 있다. M2에서 결정하고 근거를 문서화하라
- **bit-banging은 지원하지 않는다** (F6). README에 명시하고 대안 예제를 제공하라
- 검증: 실제 사용할 라이브러리 목록을 **컴파일 테스트**하라. 추측하지 말 것

### README 범위 표기 형식
`지원` / `부분 지원` / `미지원` 3단계만 쓰고 각 단계의 정의를 명시한다. 못 하는 것을 구체적으로 적어라
(예: "Wire는 master만. target/slave 미지원"). `NU54DK_Arduino_Core` README가 좋은 예시다.

---

## 12. 작업 규칙

- 커밋은 마일스톤 단위가 아니라 기능 단위로 쪼개라
- F1·F2 관련 코드에는 **왜 그 값인지 근거를 주석으로 남겨라.** 나중에 추적 불가능해진다
- 참조 구현에서 코드를 가져오면 출처와 라이선스 고지를 파일 상단에 남겨라
- 문서와 실제가 다르면 문서를 고쳐라 (§0)
- 하드웨어 없이 추측으로 진행하지 마라. DK에서 검증 후 다음 단계로
- 저전력 관련 테스트는 SWD 프로브를 분리하고 수행하라 (F8)

---

## 13. 참고 링크

**Nordic**
- sdk-nrf-bm: https://github.com/nrfconnect/sdk-nrf-bm
- Bare Metal 문서: https://nrfconnectdocs.nordicsemi.com/ncs-bm/latest/nrf-bm/index.html
- Bare Metal 제품 페이지: https://www.nordicsemi.com/Products/Development-software/nRF-Connect-SDK/Bare-Metal-option-for-nRF54L-Series
- Nordic-5-Clause 전문: https://github.com/nrfconnect/sdk-nrf/blob/main/LICENSE
- 라이선스 스킴 설명: https://devzone.nordicsemi.com/nordic/nordic-blog/b/blog/posts/introducing-nordics-new-software-licensing-schemes
- nRF54L 개발 옵션 비교: https://academy.nordicsemi.com/courses/nrf54l-series-express-course/lessons/lesson-5-development-choices-and-demo/topic/nrf54l-development-options/

**참조 구현**
- Adafruit nRF52 코어: https://github.com/adafruit/Adafruit_nRF52_Arduino
- `rtos.h`: https://github.com/adafruit/Adafruit_nRF52_Arduino/blob/master/cores/nRF5/rtos.h
- Scheduler 예제: https://github.com/adafruit/Adafruit_nRF52_Arduino/blob/master/libraries/Bluefruit52Lib/examples/Hardware/rtos_scheduler/rtos_scheduler.ino
- Bluefruit FAQ (FreeRTOS 근거): https://learn.adafruit.com/bluefruit-nrf52-feather-learning-guide/faqs
- Adafruit 부트로더: https://github.com/adafruit/Adafruit_nRF52_Bootloader
- lolren nRF54L 코어 (페리페럴 참조): https://github.com/lolren/nrf54-arduino-core
- nRF54L 부트로더 (BLE+Serial DFU, S145, MIT): https://github.com/caveman99/nRF54_Bootloader
  - M4 착수 전 반드시 검증. nRF54L 에서 MBR 없이 부트로더를 기동하는 유일한 공개 선례

**대안 아키텍처 사례 (배제, 참조용)**
- NU54DK Arduino Core (NCS 전체 빌드 방식, nRF54L15, MIT): https://github.com/EIDOSDATA/NU54DK_Arduino_Core
  - `platform.txt` (업로드 툴 정의 + 소스 그래프 수집 기법): https://github.com/EIDOSDATA/NU54DK_Arduino_Core/blob/main/platform.txt
- ArduinoCore-zephyr: https://github.com/arduino/ArduinoCore-zephyr
