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
| SVC 충돌(F1) | **없음.** SVC 0x00~0x0F가 앱 몫 (§7 F1) |
| SD 예약 인터럽트 우선순위(F2) | **0과 4** (§7 F2) |
| FreeRTOS 포트 | sdk-nrf-bm에 **없음.** 직접 포팅 (§7 F4) |
| 업로드 툴 | **probe-rs** (§3) |

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
| 지원 보드 | **NU54-DK (nRF54L15)** — 1차이자 현재 유일한 variant |
| `boards.txt` 표시명 | `NU54-DK (nRF54L15)` |
| board id / `build.variant` | `nu54dk` |
| architecture | **`nrf54l`** (플랫폼 디렉토리 `nrf54l/`, 코어 `cores/nrf54l/`) |
| 릴리스 아카이브 | `baram-nrf54l-<ver>.tar.bz2` |
| Board Manager 인덱스 | `package_baram_nrf54_index.json` |

인덱스 이름만 `nrf54`로 넓게 잡은 이유: 하나의 index에 platform을 여러 개 넣을 수 있으므로,
훗날 nRF54H를 하게 되면 같은 JSON의 `platforms[]`에 `architecture: "nrf54h"`로 추가하면 된다.
**사용자가 등록할 Board Manager URL은 영구히 하나다.** nRF54H는 SoftDevice 베어메탈 옵션 자체가
없는 다른 칩이므로 architecture를 합치지 않는다 — 합치면 호환되지 않는 두 코어에
라이브러리가 모두 호환된다고 표시된다.

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

**현행 NU54-DK 실측 (회로도 확인 완료 — §4.1)**: 온보드 디버그 프로브가 **없다.**
CP2102N USB-UART 브리지 + 외부 프로브용 SWD 헤더(J3 = ARM 10핀 1.27mm, P2 = 5핀)만 있다.
따라서 M1~M3 개발에는 **외부 CMSIS-DAP 장비를 J3에 연결**한다. 현재 계획에 지장 없다.

**차기 보드 리비전 권장 사항** (현행 보드에는 없음):
- **온보드 CMSIS-DAP 프로브** (SAMD11 / CH552 등) — 케이블 하나로 개발 가능
- 프로브가 UART도 겸하면 더 좋다 ("DAP UART" 구성)
- **부트로더 진입용 GPIO strap** — M4 이후 필요

현행 보드에 이미 있는 것: **UART 브리지**(CP2102N, `Serial` 출력 + M4 이후 DFU 경로), 32.768 kHz LFXO.

### 4.1 NU54-DK 핀맵 (회로도 실측)

모듈 `NCRB54N01VC`(nRF54L15, 62핀). **LED/버튼/UART 핀이 Nordic nRF54L15 DK와 완전히 동일**하다
(sdk-nrf-bm `boards/nordic/bm_nrf54l15dk/include/board-config.h`와 1:1 일치).
→ Nordic 샘플과 보드 설정을 거의 그대로 쓸 수 있다.

| 기능 | 핀 | 주의 |
|---|---|---|
| LED D7 / D8 / D9 / D10 | P2.09 / P1.10 / P2.07 / P1.14 | N-MOSFET 게이트 구동, **active HIGH** (Adafruit 기본값과 반대) |
| SW2 / SW3 / SW4 / SW5 | P1.13 / P1.09 / P1.08 / P0.04 | active LOW, **내부 풀업 필요** (회로도 명기) |
| SW1 | RESET | 10K 풀업 + 1N4148 |
| UART (CP2102N) | TX=P0.00, RX=P0.01, CTS=P0.02, RTS=P0.03 | **UARTE30** |
| LFXO Y1 32.768 kHz | P1.00(XL1) / P1.01(XL2) | 13pF. **GPIO로 쓰면 안 됨** |
| SWD | J3(10핀) / P2(5핀) | **J3-6 = SWO = P2.07 = LED D9 겸용** |
| NFC | P1.02 / P1.03 | |
| AIN0~7 | P1.04~P1.07, P1.11~P1.14 | |
| 헤더 노출 | P0.00~04, P1.00~14, P2.00~10 | P1(25핀) / P3(25핀) |
| 전원 | VIN 5~14V → AZ1117-3.3 → 3V3 | USB-C |

상세는 `docs/NU54-DK.md`.

### SPI 주의 (L15)
외부 노출 Arduino SPI 핀은 SPIM00 하나만 사용 가능. P2 고속 라우팅, E0/E1 출력 드라이브, HSBIAS slew, SPIM anomaly 8 워크어라운드가 필요하다. `lolren/nrf54-arduino-core`에 구현 사례가 있다.

---

## 5. 검토 완료된 대안 (재검토 금지)

이 결정들을 다시 제안하지 마라. 이미 배제됐다.

| 대안 | 배제 이유 |
|---|---|
| `arduino/ArduinoCore-zephyr` 포크 | BLE가 Zephyr 오픈소스 LL만 가능(미인증, nRF54L 미성숙). llext 모델이 Adafruit 사용자 기대와 불일치 |
| `lolren/nrf54-arduino-core` 사용 | BLE 스택이 자체 구현·미인증·동시 링크 1개. 단일 메인테이너. 부트로더/DFU 없음 |
| `EIDOSDATA/NU54DK_Arduino_Core` 방식 (NCS 전체 빌드) | R1. 매 컴파일이 west build, `prj.conf` 미노출, Windows 전용. §5.1 참조 |
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

**정직하게 인정할 강점**: Zephyr 드라이버 생태계를 그대로 쓴다. 우리는 nrfx 위에 페리페럴을 전부 직접 짜야 하므로 M2 작업량이 우리 쪽이 훨씬 크다.

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
| `NRF_POWER->GPREGRET` | **없음** | `.noinit` RAM (F7 방법 2) |
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

### F1. SVC 핸들러 — **조사 완료. 번호 충돌은 없다**

`s145_API/include/nrf_svc.h`:
> The SVCs with SVC numbers **0x00-0x0F are forwarded to the application**. All other SVCs are handled by the SoftDevice.

`SDM_SVC_BASE = 0x10`, `SOC_SVC_BASE = 0x20`. FreeRTOS `ARM_CM33_NTZ`는 `portSVC_START_SCHEDULER = 0`만 쓴다 → **번호 충돌 없음.**

진짜 함정은 다른 데 있다. **nRF54L은 nRF52와 구조가 반대다** — 애플리케이션이 벡터 테이블을 소유하고
필요한 IRQ를 SoftDevice로 **포워딩**한다 (`nrf_sd_isr.h`의 `NRF_SD_ISR_OFFSET_*`).
sdk-nrf-bm `subsys/softdevice_handler/irq_forward.s`가 그 구현인데,
거기 `SVC_Handler`는 **SVC 번호를 보지 않고 전부 SoftDevice로 넘긴다.**

→ **그 핸들러를 그대로 쓰면 FreeRTOS가 죽는다.** 스택된 PC에서 SVC immediate를 읽어 분기하는
자체 `SVC_Handler`를 작성하라: `< 0x10` → `vPortSVCHandler`, `>= 0x10` → SoftDevice 포워딩.

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

### F7. UART 공유
스케치의 `Serial`과 부트로더 진입이 같은 UART를 공유한다 (M4 이후). 진입 방법:
1. GPIO strap (**기본 채택** — 커스텀 보드이므로)
2. retained RAM(`.noinit`) 플래그 + 리셋 → `reboot_to_bootloader()` API 제공
3. 매직 시퀀스 감지 (오검출 위험, 보조 수단)

### F8. 디버거가 System OFF를 방해한다
Active SWD 연결 상태에서는 System OFF 진입과 reset cause 판정이 정상 동작하지 않을 수 있다.
저전력 관련 테스트는 프로브를 분리하고 수행하라. 모르면 "System OFF가 안 된다"고 잘못 결론낸다.

하드웨어적 근거: nrfx `nrf_regulators_system_off()` 구현 자체에
`/* Solution for simulated System OFF in debug mode */ while (true) { __WFE(); }` 폴백이 들어 있다.

### F9. 슬립 크리티컬 섹션에 PRIMASK를 쓰지 마라 — **F2 다음으로 위험**

Adafruit의 `vPortSuppressTicksAndSleep()`은 슬립 구간을
`sd_nvic_critical_region_enter()` (SD 없으면 `__disable_irq()`)로 감싼다.
nRF54L에는 그 SD API가 없고(§6.1), `__disable_irq()`(PRIMASK)로 대체하면
**SoftDevice의 우선순위 0 zero-latency IRQ(RADIO_0/TIMER10/GRTC_3)까지 막혀 라디오 타이밍이 깨진다.**

→ **BASEPRI를 우선순위 1로 올린다. PRIMASK는 쓰지 않는다.**

- 우선순위 0(SD zero-latency)은 계속 서비스됨 → 라디오 안전
- 1~7은 마스크됨 → `eTaskConfirmSleepModeStatus()`와 `WFI` 사이의 레이스 차단
- SD의 우선순위 4 IRQ는 잠시 보류되나 pending으로 CPU를 깨우고 BASEPRI 복구 후 처리됨 — 정상
- 슬립은 `__WFE()` 폴링 루프가 아니라 **`__WFI()`**. BASEPRI로 마스크된 인터럽트도 pending이 되면
  WFI를 깨우므로 `NVIC->ISPR` 폴링이 불필요하고, nRF54L15는 IRQ가 64개를 넘어
  Adafruit의 `ISPR[0]|ISPR[1]` 관용구가 애초에 틀린다

**이 가정은 BLE가 붙는 M3 전까지 검증할 수 없다.** M3 진입 직후
advertising 유지 + tickless 동시 동작을 최우선으로 확인하라.
깨지면 슬립 창에서만 BASEPRI를 0으로 낮추고 레이스는 `eTaskConfirmSleepModeStatus()` 재확인으로 처리한다.

---

## 8. 참조 구현 매핑

`adafruit/Adafruit_nRF52_Arduino`를 구조 참조로 사용한다. 새로 설계하지 말고 이식하라.

| Adafruit (nRF52) | 이 프로젝트 (nRF54L) | 비고 |
|---|---|---|
| `cores/nRF5/` | `cores/nrf54l/` | |
| `cores/nRF5/rtos.h` | 동일 API 유지 | 호환 핵심. 시그니처 바꾸지 말 것 |
| `cores/nRF5/freertos/` | FreeRTOS + `ARM_CM33_NTZ` | 틱을 GRTC로 교체 (§7 F3) |
| `port_cmsis_systick.c` (RTC1) | `port_grtc.c` (GRTC) | tickless 골격은 유지, SD 의존부만 치환 (§6.1, §7 F9) |
| `libraries/Bluefruit52Lib/` | 동일 API, `sd_ble_*` 재구현 | S145 기준 |
| nRF5 SDK 드라이버 | nrfx 4.x | nrfx는 Zephyr 없이 standalone 사용이 정식 지원됨 |
| S132 / S140 | **S145 v10.0.1 고정** (peripheral+central) | S115는 메뉴 항목 추가만으로 확장 가능하게 변수화 |
| `Adafruit_nRF52_Bootloader` | 자체 UART DFU 부트로더 (M4) | 구조 참고 |
| `adafruit-nrfutil` | 호스트 업로드 툴 (M4) | 패키지에 바이너리 동봉하는 방식 참고 |
| `Adafruit_TinyUSB` (Serial) | **이식 대상 아님** | R10 |

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
├── README.md                        # 라이선스 혼재 명시 필수
├── CLAUDE.md                        # 이 문서
├── package_baram_nrf54_index.json   # platforms[0] = nrf54l
├── extras/make_release.sh           # 배포 스크립트 (아카이브에 미포함)
├── docs/                            # 아카이브에 미포함
│   ├── LICENSE-INVENTORY.md
│   ├── MEMORY-MAP.md
│   ├── NU54-DK.md
│   └── HIL/                         # 실기 검증 기록
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
- [x] SVC 충돌 여부 (F1용) → **번호 충돌 없음.** 대신 IRQ 포워딩 구조 대응 필요. §7 F1
- [x] sdk-nrf-bm에 FreeRTOS 포트 포함 여부 → **없음.** 직접 포팅 확정 (§7 F4)
- [x] 업로드 툴 조사 → **probe-rs 채택** (§3). pyOCD 0.39도 `nrf54l` 지원하나 Python 의존이라 대비책
- [x] 메모리 맵 확정 → `docs/MEMORY-MAP.md`. 앱이 0x0, SD가 상단 (**nRF52와 반대**)
- [x] 저전력 API 조사 → SD의 `sd_app_evt_wait`/`sd_nvic_*`/`sd_power_system_off` **전부 없음** (§6.1, §7 F9)
- [x] NU54-DK 회로도 분석 → `docs/NU54-DK.md`, §4.1
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

업로드는 **CMSIS-DAP/probe-rs 또는 J-Link SWD**. 부트로더 없음 (§3). 보드는 **NU54-DK (nRF54L15)**.

- [ ] FreeRTOS 포팅: `ARM_CM33_NTZ`, GRTC 틱, SVC 조정(F1), 우선순위 설정(F2)
- [ ] `setup()`/`loop()` 태스크 기동
- [ ] GPIO: `pinMode` / `digitalWrite` / `digitalRead`
- [ ] `millis` / `micros` / `delay` (tickless)
- [ ] UART `Serial`
- [ ] `boards.txt` + `platform.txt`에 SWD 업로드 recipe (**프로브 UID 지정 옵션 포함**)

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

- [ ] SoftDevice S145 활성화 + 이벤트 처리 태스크
- [ ] `Bluefruit.begin()`, advertising
- [ ] `BLEService` / `BLECharacteristic`
- [ ] `BLEUart` (NUS)
- [ ] 페어링 / 본딩

**DoD**: Adafruit `Bluefruit52Lib/examples/Peripheral/bleuart` 원본 스케치가 수정 없이 컴파일·동작하고, 폰에서 연결·송수신된다.

### M4 — 부트로더 / DFU

CMSIS-DAP 업로드 경로는 **제거하지 말고 병행 유지**한다 (§3).

- [ ] RRAM 레이아웃 확정 (부트로더 / SoftDevice / 앱 / 스토리지) → `docs/MEMORY-MAP.md`
- [ ] UART DFU (또는 sdk-nrf-bm DFU 활용 — M0 결과에 따름)
- [ ] 부트로더 진입: GPIO strap + `reboot_to_bootloader()`
- [ ] 호스트 업로드 툴
- [ ] `boards.txt` 업로드 방식 메뉴: `CMSIS-DAP (probe-rs)` / `CMSIS-DAP + Probe UID` / `J-Link` / `UART DFU`

**DoD**: UART DFU로 20회 연속 업로드 실패 0회. 업로드 중 전원 차단 후에도 부트로더가 살아 복구 가능. CMSIS-DAP 경로도 여전히 동작.

### M5 — 패키징 / 배포

- [ ] variant: **NU54-DK (nRF54L15)** 우선. 이후 Nordic nRF54L15 DK / generic 모듈
- [ ] `boards.txt`, `platform.txt`, `package_*_index.json`
- [ ] 라이선스 파일 정리 (§9), README
- [ ] 예제 스케치 (Adafruit 예제 포팅)
- [ ] 라이브러리 호환성 컴파일 테스트 결과 → `docs/LIBRARY-COMPAT.md`

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

**대안 아키텍처 사례 (배제, 참조용)**
- NU54DK Arduino Core (NCS 전체 빌드 방식, nRF54L15, MIT): https://github.com/EIDOSDATA/NU54DK_Arduino_Core
  - `platform.txt` (업로드 툴 정의 + 소스 그래프 수집 기법): https://github.com/EIDOSDATA/NU54DK_Arduino_Core/blob/main/platform.txt
- ArduinoCore-zephyr: https://github.com/arduino/ArduinoCore-zephyr
