# 진행 상황 / 다음 세션 인수인계

최종 갱신: 2026-09-08 · 릴리스 `0.2.0` + `BLEHidGamepad`

프로젝트 지침과 설계 결정은 [CLAUDE.md](../CLAUDE.md) 가 정본이다.
이 문서는 **"지금 어디까지 됐고 다음에 뭘 하면 되는지"** 만 짧게 적는다.

---

## 1. 지금 동작하는 것 (M1 거의 완료 · M3 DoD 달성 — peripheral + central)

실기 보드 **NU54-DK / nRF54L05** 에서 확인:

| 항목 | 상태 |
|---|---|
| FreeRTOS (ARM_CM33_NTZ) + GRTC 틱 | ✅ 1000 tick/s |
| `setup()`/`loop()` + `Scheduler.startLoop()` 두 번째 태스크 | ✅ |
| GPIO (LED 4 / 버튼 4, 내부 풀업) | ✅ |
| `millis()` / `micros()` / `delay()` | ✅ 델타 정확 |
| `Serial` (UARTE30 → CP2102N) | ✅ 흐름제어 없음 |
| **tickless idle** | ✅ 틱 vs SYSCOUNTER 0 ppm, 5분 소크 이상 0건 |
| LFXO 클럭 정확도 | ✅ 호스트 대비 +25~38 ppm |
| arduino-cli / Arduino IDE 컴파일·업로드 | ✅ probe-rs, CMSIS-DAP |
| 보드 3종 | ✅ NU54-DK / NU54V-DK / XIAO nRF54L15 |

**XIAO nRF54L15 실기 확인 (2026-09-06)** — `docs/HIL/M1-xiao.md`:
온보드 CMSIS-DAP 업로드 1.9초, LED 점멸(active LOW) 및 `Serial`(UARTE20) 정상,
틱 vs SYSCOUNTER **0.0 ppm**, 호스트 대비 **-14 ppm**, 180초 90샘플 이상 0건.
LFXO 내부 로드 캡을 잡기 전에는 **+805 ppm** 이었다 (§ 아래 4-7).

```
FQBN  baram-nrf54:nrf54l:nu54dk          NU54-DK    (nRF54L05, 500KB/96KB)
      baram-nrf54:nrf54l:nu54vdk         NU54V-DK   (nRF54L15, 1.5MB/256KB)
      baram-nrf54:nrf54l:xiao_nrf54l15   XIAO       (nRF54L15, 온보드 CMSIS-DAP)
```

빌드 크기(blink + Serial + 2태스크): Flash 38004 B, RAM 3856 B.

### BLE (M3) — 실기 확인

XIAO nRF54L15 + Mac(bleak) / 폰(nRF Connect) / NU54-DK 로 확인한 것:

| 항목 | 상태 |
|---|---|
| SoftDevice S145 활성화 + 이벤트 펌프 | ✅ |
| advertising / GATT 서버 / 커스텀 서비스 | ✅ |
| `BLEUart`(NUS) · `BLEDis` · `BLEBas` | ✅ |
| ATT MTU 협상 | ✅ 247 |
| **동시 연결** | ✅ L15 5링크 / L05 3링크분 RAM, 폰+Mac 2링크 양방향 실증 |
| `BLEBeacon`(iBeacon) · `EddyStoneUrl` | ✅ 광고 바이트 단위 검증 |
| `getPeerName()` (GATT 클라이언트) | ✅ `Connected to Mac` |
| **central — 스캔 / 필터 / 연결** | ✅ 두 보드 간 |
| **central — GATT 탐색 + `BLEClientUart`** | ✅ MTU 247, 양방향 |
| `BLEClientService` / `BLEClientCharacteristic` | ✅ 상류 `central_bleuart` 컴파일 |
| 본딩 키 RRAM 저장 | ✅ 저장·재부팅 유지·IRK 주소 해석 |
| **페어링 / 본딩** | ✅ Just Works, 재연결 무페어링 암호화, CCCD 복원 |
| **LESC** (LE Secure Connections) | ✅ micro-ecc P-256, Mac 과 `LESC=1` |
| **HID 키보드** | ✅ 호스트 페어링 후 버튼 -> 키 입력 |
| **HID 게임패드** | ✅ Mac 이 Usage 1/5 로 열거, 테스터에서 축·버튼 반응 |
| **처리량 실측** | ✅ 2M PHY / MTU 247, 맥 상대 양방향 26~28 KB/s (§2.6) |
| 역할 배분 런타임 지정 | ✅ `begin(4,0)` `(0,4)` `(2,2)` `(1,1)` 전부 |
| tickless idle 과 BLE 동시 동작 | ✅ 틱 vs SYSCOUNTER 0.0 ppm |

**없는 것:** `BLEClientHidAdafruit`, `BLEMidi`,
`BLEAncs` / `BLEClientCts`, 실제 DFU(M4). 그리고 **M2(Arduino API)가 통째로 비어 있다** —
`Wire` / `SPI` / `analogRead` / `analogWrite` / `attachInterrupt` 는 아직 없다.
예제 호환 현황은 `docs/EXAMPLE-COMPAT.md` (71개 중 **28개** 통과).

---

## 2. 바로 다음에 할 일

### ⭐ 다음 세션은 여기서 시작한다 (2026-09-08 갱신)

**결정: BLE 를 먼저 끝낸다.** M2(Arduino API)가 예제 기준으로는 더 큰 병목이지만
(미통과 45개 중 약 20개), **BLE 를 M4 를 뺀 범위에서 닫고 나서 M2 로 넘어간다.**
사용자가 그렇게 정했다. M2 로 방향을 되돌리지 마라.

**우선순위 — B13b 를 순서대로**

~~1. **`BLEMidi`**~~ — ✅ **끝났다 (§2.7).** 양방향 실기 확인.
   상류 원본 `blemidi.ino` 가 include 3줄 삭제만으로 컴파일된다
~~2. **`BLEClientCts`**~~ — ✅ **끝났다 (§2.8).** iPhone 상대로 시각·시간대 확인
~~3. **`BLEAncs`**~~ — ✅ **끝났다 (§2.9).** iPhone 알림을 앱 이름·제목·본문까지 수신
~~4. **`BLEClientHidAdafruit`**~~ — ✅ **끝났다 (§2.10).** 보드 2대로 실기 확인

**→ B13b 가 끝났다. M4(부트로더)를 뺀 BLE 는 여기서 닫혔다. 지금은 M2 다.**

**M2 진행 상황**

| | |
|---|---|
| ~~`Wire`(TWIM)~~ | ✅ **끝났다 (§2.11).** XIAO 온보드 IMU 로 실기 확인 |
| ~~`SPI`(SPIM00)~~ | ✅ **끝났다 (§2.12).** SPI 플래시·SD 카드로 실기 확인 |
| ~~`attachInterrupt`(GPIOTE)~~ | ✅ **끝났다 (§2.13).** NU54-DK 버튼으로 P1·P0 양쪽 확인 |
| **`analogWrite`(PWM)** | ⭐ **다음.** 온보드 LED 로 검증된다. **PWM20/21/22 는 P1 전용** |
| `analogRead`(SAADC) | 외부 계측 필요. **AIN0~7 = P1.04~07 / P1.11~14 고정** |
| 전류 측정 | M1 을 닫는 마지막 항목. 프로브 분리 필수 (§7 F8) |

⚠ **M2 DoD 의 "I2C 센서 라이브러리 1종 동작" 이 아직 안 끝났다.** SPI 가 생겨
`Seeed_Arduino_LSM6DS3` 는 컴파일되지만 **값이 0 으로만 나온다** — 그 라이브러리가
`Wire`->`Wire1` 치환을 **특정 Seeed 보드 매크로**(`TARGET_SEEED_XIAO_NRF52840_SENSE`
등)에만 걸어 두어서, 우리 보드에서는 헤더 쪽 `Wire` 를 쓴다. 거기엔 아무것도 없다.
**우리 버그가 아니다** (I2C 자체는 §2.11 에서 확인됐다).
→ 버스를 인자로 받는 라이브러리를 쓰거나, 센서를 `Wire`(헤더) 쪽에 붙여 재시험하라.

⚠ **`Adafruit_BusIO` 는 아직 못 쓴다.** `digitalPinToPort` / `portOutputRegister` /
`digitalPinToBitMask` 를 요구하는데 우리 코어에 없다. **Adafruit 센서 라이브러리
대부분이 BusIO 를 거치므로** 이 셰임을 넣을지는 별도 판단이다 (AVR 식 고속 GPIO
매크로라 §11 의 AVR 셰임과 같은 성격이다).
~~5. **처리량 실측**~~ — ✅ **끝났다 (§2.6).** 맥 상대 양방향 26~28 KB/s.
   notify 큐 깊이는 병목이 아니었다. 남은 것은 iOS 쪽 수치뿐이다

그 다음이 M2 다 (`Wire`(TWIM) -> `attachInterrupt`/`analogWrite` -> `SPI`/`analogRead`),
그리고 전류 측정으로 M1 을 닫는다. **M2 착수 전에 CLAUDE.md §7 F10 ④ 체크리스트를
반드시 읽어라** — 같은 번호대 SERIAL 블록 충돌(`docs/PERIPHERAL-PINMAP.md` §0)과
다중 인스턴스 IRQ 직접 연결이 그대로 재발한다.

**별건으로 남겨 둔 것 두 개** (오늘 게임패드 작업에서 드러났다)

- **Service Changed 켜기** — GATT 지문(`0a20d0c`)은 원인을 **보이게만** 하고 자동
  복구는 못 한다. 호스트는 본딩이 살아 있으면 CCCD 를 다시 쓰지 않는다.
  `sd_ble_cfg_set(BLE_GATTS_CFG_SERVICE_CHANGED, …)` 로 SD 구성을 바꿔야 하는데
  **앱 RAM 요구량이 달라질 수 있다** (L05 여유 184 B). 실측 없이 켜지 마라
- **HOGP 는 Battery Service 를 필수로 요구한다** — 우리 HID 예제 3종 모두
  `BLEBas` 가 없다. macOS 는 문제없이 동작했지만 다른 호스트에서 걸릴 수 있다.
  예제에 한 줄 추가하는 정도의 일이다

⚠ **구현 전에 레퍼런스부터 확인한다.** 상류(Adafruit), Nordic DevZone, Zephyr
드라이버를 먼저 본다. 두 번 이게 방향을 바꿨다 — UARTE FRAMETIMEOUT 은
nRF54L 에서 쓰면 안 되는 것이었고(§2.5), 연결 핸들은 배열 인덱스가 아니었다(B5).

**저장소는 `origin/main` 과 동기화돼 있다.** 미푸시 없음.
릴리스는 `0.2.0` 이 최신이고, 그 뒤로 게임패드·GATT 지문·처리량 API(`d7f95b8`)·문서가
들어갔다 —
**다음 릴리스를 낼 때 `0.3.0` 으로 올려라** (`extras/make_release.sh`, §(c)).

**이 PC 개발 환경** (2026-09-07 에 맞춰 뒀다):
`~/Documents/Arduino/hardware/baram-nrf54` 심링크, xPack GCC 14.2.1-1.1 은
`~/opt/xpack-arm-none-eabi-gcc-14.2.1-1.1`, probe-rs 0.32.0 은
`~/opt/probe-rs-0.32.0/bin` (저장소에서 뺐다 — `platform.local.txt.example` 참조).

### 실기 환경 메모

| | |
|---|---|
| XIAO nRF54L15 | probe `2886:0066:5784477E`, 시리얼 `/dev/cu.usbmodem5784477E3` |
| NU54-DK (L05) | probe `0d28:0204:1070…2820`, 시리얼 `/dev/cu.usbserial-2110` |

⚠ **프로브가 둘 붙어 있으면 `arduino-cli upload` 가 실패한다** — 어느 쪽인지 못 고른다.
`probe-rs download --probe <VID:PID:serial>` 로 직접 굽는다.
그때 **SoftDevice 영역이 지워질 수 있다.** 증상은 `begin=0 err=0 need=0`
(cfg_set 이 아예 안 불린 상태). `nrf54l/softdevice/s145_nrf54l05_*.hex` 를 다시 구우면 된다.

⚠ 시리얼 포트 번호는 다시 꽂을 때마다 바뀐다 (`usbserial-110` -> `usbserial-2110`).

**central 시험 조합**: XIAO 에 `Central/central_bleuart`, NU54-DK 에
`Peripheral/bleuart`. 다만 두 예제 모두 시리얼 입력을 쓰는데 XIAO VCOM 이
약하므로(§2.5), 주기 송신 스케치로 시험하는 편이 낫다.

---


### (a) 전류 측정 — M1 을 닫으려면 이게 남았다

tickless 를 켠 목적이 전력인데 아직 재지 못했다. **SWD 프로브를 물리적으로
분리하고** 재야 한다 (§7 F8 — 붙어 있으면 수치가 안 나온다).

측정 4종 (§4.6):
1. `delay(1000)` 루프 + `Serial` 켠 상태
2. 같은 조건에서 `Serial.end()` 후 (UARTE 가 바닥 전류를 올리는지)
3. `systemOff()` 후 — `systemOff()` 는 구현됐다 (`cores/nrf54l/wiring.c`)
4. (M3) advertising 중

기준선은 Nordic `ble_pwr_profiling` 샘플을 같은 보드에 구워서 잡는다.
이 비교 없이는 "이 정도면 괜찮은가"를 판단할 근거가 없다.

같이 판단할 것: `port_grtc.c` 의 `nrfx_grtc_active_request_set(true)` 를 빼도
되는지. `MODE.AUTOEN` 이 이미 0 이라 중복일 가능성이 높고, 뺐을 때 전력이
내려가면 빼면 된다. 기능적으로는 없어도 틱이 정확하다 (확인됨).

### (b) 앱 레벨 저전력 API (§4.5) — ✅ **구현됨 (`cores/nrf54l/wiring.c`)**

`waitForEvent()`, `systemOff(pin, wake_logic)`, `readResetReason()`.
**전류로는 아직 검증 안 됐다** — 위 (a) 가 그 항목이다.
`readResetReason()` 은 `NRF_RESET` 이다 (`NRF_POWER->RESETREAS` 아님, `NRF_RESETINFO` 도 아님).

### (c) 릴리스 파이프라인 — ✅ **0.2.0 배포 완료 (2026-09-07)**

깨끗한 환경에서 Board Manager 로 설치 → 컴파일 → **실기 업로드·동작까지 확인했다**
(M5 DoD 중 macOS/arm64 부분). Linux / Windows 는 자산은 올라갔지만 미검증이다.

재배포할 때는 아래 순서다. **저장소의 인덱스에는 업로드 안 한 것을 넣지 않는다** —
아카이브 바이트가 매번 달라 체크섬과 파일이 같은 실행에서 나와야 한다.

```sh
extras/make_tools.sh              # probe-rs 재포장 + 업로드 (버전 바뀔 때만)
extras/make_release.sh 0.1.0      # 플랫폼 아카이브 + 업로드
git add package_baram_nrf54_index.json nrf54l/platform.txt && git commit && git push
```

| 항목 | 값 |
|---|---|
| packager (FQBN 앞부분) | **`baram-nrf54`** |
| Board Manager URL | `https://raw.githubusercontent.com/chcbaram/baram-nrf54-arduino/main/package_baram_nrf54_index.json` |
| 플랫폼 아카이브 | 1.7 MB (`tools/` 제외) |
| 릴리스 태그 | `0.1.0` (플랫폼) / `probe-rs-0.32.0` (툴) |
| 툴 — GCC | xPack `14.2.1-1.1`. **업스트림 URL 을 그대로 가리킨다** (재호스팅 안 함) |
| 툴 — probe-rs | `0.32.0`. 업스트림 레이아웃이 `platform.txt` 의 `{...}/bin` 과 안 맞아 **재포장**해서 이 저장소 릴리스에 올린다 |

주의할 것 두 가지:

- **`boards` 목록은 `boards.txt` 에서 자동으로 읽는다.** 보드를 추가해도 스크립트를
  고칠 필요가 없다
- **Linux probe-rs 는 스트립이 필요하다.** 업스트림이 비스트립본(136 MB, 그중 94 MB 가
  DWARF)을 배포한다. `make_tools.sh` 는 ELF 를 다룰 수 있는 strip 이 있을 때만
  스트립하고 없으면 경고한다 → **macOS 에서 돌리지 말고 Linux 나 `brew install llvm`
  환경에서 돌려라.** 스트립하면 136 MB → 42 MB 다

### (d) M3 A단계 — ✅ **끝났다. F9 가 풀렸다 (2026-09-06)**

SoftDevice S145 가 뜨고 advertising 이 공중에서 잡히며 연결까지 성립한다.
**그 상태에서 틱 vs SYSCOUNTER 0.0 ppm** — §7 F9(BASEPRI/PRIMASK 분리)가
라디오와 공존한다는 것이 실측으로 확인됐다. 이 프로젝트 최대의 미지수였다.
기록: `docs/HIL/M3-softdevice.md`.

이 과정에서 코어가 두 군데 바뀌었다. **둘 다 M1 회귀 없음을 확인했다.**

| 바뀐 것 | 왜 |
|---|---|
| `NRFX_GRTC_CONFIG_AUTOEN` 0 → **1** | SoftDevice 요구사항. 안 켜면 `0x1003` 으로 거부당한다 |
| GRTC `CLKSEL` LFXO → **SystemLFCLK** | SoftDevice 가 LFCLK 를 관리한다는 전제와 맞춘다 |

### B 단계 — Bluefruit API 계층 — ✅ **B13a·B13c 완료, B13b 만 남음**

**목표는 M3 DoD**: Adafruit `Bluefruit52Lib/examples/Peripheral/bleuart` 원본이
**무수정으로** 컴파일·동작하는 것.

규모를 먼저 알아 둘 것: Bluefruit52Lib 본체만 **약 250 KB / 30여 파일**이고
`services/`(BLEUart·BLEDis·BLEBas·BLEDfu)와 파일시스템 2종이 더 붙는다.
한 번에 끝나지 않으므로 아래처럼 쪼갠다. **각 단계마다 bleak 으로 실증한다.**

| 단계 | 내용 | 검증 |
|---|---|---|
| ~~**B1**~~ ✅ | `BLEUuid` / `BLEService` / `BLECharacteristic` + 최소 `Bluefruit` 싱글턴 | **완료.** 탐색·읽기·알림·쓰기 전부 실증 |
| ~~**B2**~~ ✅ | `BLEUart` (NUS) | **완료.** 18바이트 에코 왕복 일치 |
| ~~**B3**~~ ✅ | MTU 협상(247), `BLEConnection`, `BLEDis`, `BLEBas`, `autoConnLed` 등 | **완료.** Adafruit `bleuart` 예제가 API 호출 그대로 동작 |
| ~~**B4**~~ ✅ | `BLEDfu` 스텁, 파일시스템 안내 헤더, `bluefruit.h` 가 서비스 포함 | **완료. Adafruit 원본 `bleuart.ino` 가 include 2줄 삭제만으로 동작 = M3 DoD** |
| ~~**B5**~~ ✅ | **다중 연결** (nRF54L15 4개) + notify 큐 설정 | **완료.** 폰 + Mac 동시 2링크 실증 |
| ~~**B6**~~ ✅ | **Beacon** — `BLEBeacon`(iBeacon) + `EddyStoneUrl` | **완료.** 광고 페이로드 실측 검증 |
| ~~**B7**~~ ✅ | **GATT 클라이언트** + 콜백 지연 실행 -> `getPeerName()` | **완료.** `Connected to Mac` 실증 |
| ~~**B8**~~ ✅ | **central 역할** — 스캔 / 연결 / GATT 탐색 / `BLEClientUart` | **완료.** 두 보드 간 양방향 실증 |
| ~~**B9**~~ ✅ | `BLEClientService`/`BLEClientCharacteristic` 일반화 + `BLEClientBas`/`BLEClientDis` | **완료.** 상류 `central_bleuart` 컴파일 |
| ~~**B10**~~ ✅ | **본딩 / `BLESecurity`** (레거시 페어링) | **완료.** Mac 으로 4가지 실증 |
| ~~**B11**~~ ✅ | **LESC** (micro-ecc P-256) | **완료.** Mac 과 `LESC=1` 로 페어링 |
| ~~**B12**~~ ✅ | **HID** — 키보드/마우스/미디어 키 | **완료.** 호스트 페어링 후 버튼 -> 키 입력 확인 |
| ~~**B13a**~~ ✅ | `BLEHidGamepad` | **완료.** Mac 에서 리포트 수신 확인 (§B13a) |
| ~~**B13c**~~ ✅ | **처리량 API** — `requestPHY` / DLE / MTU 협상 + 실측 | **완료.** 맥 상대 26~28 KB/s (§2.6) |
| ~~**B13b-1**~~ ✅ | **`BLEMidi`** — BLE-MIDI 1.0 | **완료.** 양방향 실기 확인 (§2.7) |
| ~~**B13b-2**~~ ✅ | **`BLEClientCts`** — 폰의 시계를 읽는다 | **완료.** iPhone 상대 실기 확인 (§2.8) |
| ~~**B13b-3**~~ ✅ | **`BLEAncs`** — iPhone 알림 | **완료.** 실기 확인 (§2.9) |
| ~~**B13b-4**~~ ✅ | **`BLEClientHidAdafruit`** — 남의 HID 를 읽는다 | **완료.** 보드 2대 실기 확인 (§2.10) |

지금 위치: **M3 DoD 달성.** Adafruit 원본 `bleuart.ino` 가
`#include <Adafruit_LittleFS.h>` / `<InternalFileSystem.h>` **두 줄 삭제만으로**
컴파일·동작한다 (`~/Documents/Arduino/bleuart_orig/`).
서비스 4종·DIS·배터리·UART·MTU 247 전부 확인.
DoD 문구를 그렇게 바꾼 근거(예제 71개 전수 조사)는 CLAUDE.md §8.1.

**아직 없는 것:**

- **실제 DFU** — `BLEDfu` 는 서비스만 등록하고 명확히 거절한다. M4 에서 연결
- 클라이언트 쪽 **게임패드**는 구현돼 있으나 실기 미확인 (부트 프로토콜에 게임패드가
  없어 일반 Report 를 본다)

> 아래 세 줄은 B10~B12 이전에 적힌 것이라 지웠다. 본딩 · central · HID 는
> 모두 완료됐다 (§1 표와 B8·B10·B11·B12 절 참조).

시험 스케치는 `~/Documents/Arduino/nrf54_ble_gatt/` (GATT),
`~/Documents/Arduino/nrf54_bleuart/` (NUS).

⚠ MTU 를 키울 때 걸린 것 네 가지는 `docs/HIL/M3-softdevice.md` §3.7 에 있다.
특히 `ble_gap_cfg_role_count_t.adv_set_count` 는 구조체 첫 필드라
`memset` 뒤에 빠뜨리기 쉽고, 증상이 "BLE 를 못 켠다" 로만 보인다.

⚠ B1 에서 잡은 것: **`DATA_LENGTH_UPDATE_REQUEST` 에 답하지 않으면 연결은
유지되는데 ATT 가 전혀 흐르지 않는다.** 자세한 건 `docs/HIL/M3-softdevice.md` §3.5.

**파일시스템은 B4 전까지 필요 없다.** 예제의 `#include <Adafruit_LittleFS.h>` 는
본딩 저장 때문이고, RRAM 에는 erase 가 없어 그대로 못 올린다.
배경과 권장 경로는 CLAUDE.md §8.1.

### B5 — 다중 연결 ✅ (2026-09-06 완료)

**실기 결과** (XIAO nRF54L15, 폰(nRF Connect) + Mac(bleak) 동시 연결):

```
Connected, handle 4, total 2        <- 폰 handle 1, Mac handle 4
Keep advertising                     <- 2/4 라 광고 계속
[1] test2                            <- 폰 -> 보드
[1] test3
Disconnected, handle 4, reason 0x13, left 1   <- 폰 링크는 살아 있다
```
```
1.5s  mac link up, mtu 247
5.5s  *** mac RX <- b'test2'         <- 보드 -> Mac (링크 간 전달)
```

확인된 것: 동시 2링크, 링크별 핸들 태깅, **양방향 링크 간 전달**,
**연결 중 광고 유지**(연결을 쥔 채 다시 스캔해서 잡히는 것으로 확인),
한쪽만 끊어도 나머지 생존, MTU 247.

#### ⚠ 연결 핸들은 슬롯 번호가 아니다 — 여기서 한 번 물렸다

**링크 수를 4 로 두고도 SoftDevice 가 핸들 4 를 줬다.** 핸들은 슬롯 인덱스가 아니라
연결마다 새로 매기는 번호에 가깝고, 설정한 링크 수보다 커질 수 있다.

처음엔 Adafruit 처럼 `_connection[conn_hdl]` 로 핸들을 배열 인덱스로 썼다.
Adafruit 이 이걸로 안 터지는 이유는 `BLE_MAX_CONNECTION` 을 설정된 링크 수가 아니라
**20**(SoftDevice 최대)으로 고정해 배열을 넉넉히 두기 때문이다. 그 상수를
"동시 연결 수" 로 읽은 게 잘못이었다.

증상: Mac 이 붙자마자 `reason 0x16`(LOCAL_HOST_TERMINATED)으로 튕겼다.
범위 밖 핸들을 **무시하지 않고 끊도록** 짜 뒀기 때문에 즉시 드러났다.
무시했으면 "가끔 연결은 되는데 데이터가 안 온다" 로 나타났을 것이다.
링크를 5 -> 4 로 줄이면서 드러났고, 5 였으면 핸들 4 가 우연히 통과해 더 오래 숨었다.

**고친 방식:** 핸들 -> 슬롯 매핑(`_slotOf()`, 선형 탐색). 슬롯은 링크 수만큼만 둔다.
공개 API 는 그대로 핸들을 받으므로 스케치 쪽은 차이가 없다.

⚠ 그래서 **연결을 훑을 때 핸들 값으로 반복하면 안 된다.** `connHandleAt(i)` 로
슬롯을 돈다. Adafruit `bleuart_multi` 의 `for (conn_hdl = 0; conn_hdl < MAX; ...)` 는
같은 결함이 있다 — 핸들이 낮게 유지되는 동안만 우연히 동작한다.

**그때 고른 구성 (nRF54L15): 링크 4개 + notify 큐 3, SD 예약 28 KB.**
링크 5개도 RAM 은 되지만 그러면 큐를 1 에서 못 올리고, 큐 1 은 연결 이벤트당
notify 1건이라 처리량이 크게 깎인다. 연결 수보다 처리량을 골랐다 —
Adafruit `BANDWIDTH_MAX` 와 같은 조합이다.

⚠ **이 값은 이후 B8 에서 바뀌었다.** central 을 넣으면서 예약을 32 KB 로 키워
지금은 `peripheral 4 + central 1 + 큐 3` 이다. 현재 값은 `docs/MEMORY-MAP.md` 가 기준.

**nRF54L05 도 실측했다 (2026-09-06, NU54-DK 실기): 링크 2개 + 큐 3.**
필요량 `0x20004AC8`, 예약을 `0x4780` → `0x4B80` 으로 올려 여유 184 B.

⚠ **이 값도 이후 바뀌었다.** central 을 넣으려고 예약을 `0x5D00` 으로 키워
지금은 `peripheral 2 + central 1 + 큐 3` (`0x20005C38`), 앱 RAM 74,496 B 다.

⚠ **L05 와 L15 의 SoftDevice RAM 요구량이 완전히 같았다.** SoC 별로 재배치된
별도 SD 빌드인데도 그렇다 — 요구량은 설정(링크 수 · MTU · 큐 깊이)만 따른다.
그래서 한쪽에서 잰 값을 다른 쪽에 그대로 쓸 수 있다. 다만 그걸 **모르는 상태에서
추정으로 넘어가지는 않았고**, 여유가 168 B 뿐이라 실측 전에는 올리지 않았다.

**남은 것:** 없다. 링크 3개 이상 동시 연결은 호스트가 모자라 못 해 봤지만,
슬롯 관리가 개수와 무관하므로 2개에서 검증된 경로와 같다.

---

#### (아래는 착수 전에 정리한 설계 메모 — 결과와 함께 남겨 둔다)

**끝난 것: RAM.** nRF54L15 의 SoftDevice 예약을 26 KB → **30 KB(`0x20007800`)** 로
넓혔다 (커밋 `7e4e2d5`). 실기에서 `SD_BLE_PERIPH_LINK_COUNT=5` + MTU 247 로
`sd_ble_enable()` 이 성공하고 요구치가 `0x20007590` (여유 624 B) 인 것을 확인했다.
연결당 실측 ~3980 B. 측정 표는 `docs/MEMORY-MAP.md`.

**아직 안 된 것: 라이브러리.** 그래서 `SD_BLE_PERIPH_LINK_COUNT` 는 **1 로 되돌려 뒀다.**
`AdafruitBluefruit` 이 연결 하나만 추적하므로 링크 수만 올리면 두 번째 연결이
SoftDevice 에서는 맺어지는데 라이브러리가 관리하지 못한다.
`begin()` 은 지금도 `prph_count != 1` 이면 **false 를 돌려준다** (조용히 깎지 않는다).

#### 상류(Adafruit) 실제 동작 — 추측하지 말 것

구현 전에 upstream 소스를 받아 확인한 결과다. 세 가지가 처음 예상과 달랐다:

1. **`BLEUart` 의 RX FIFO 는 연결별이 아니라 하나를 공유한다.**
   `available()` / `read()` 는 어느 연결에서 왔는지 구분하지 않는다.
   `bleuart_multi.ino` 도 받은 것을 모든 연결에 그대로 되뿌린다.
   → **우리도 공유 FIFO 로 간다.** 연결별로 나누면 무핸들 `available()` 이
   어느 쪽을 봐야 할지 정의할 수 없고, 상류 예제의 동작이 달라진다 (R12).
   섞이는 문제는 상류의 한계 그대로이므로 주석으로 남긴다.
2. **핸들 없는 `write()` / `notify()` 는 "모든 연결" 이 아니라 `connHandle()` 한 곳으로 간다.**
   `BLEUart::write(buf,len)` = `write(Bluefruit.connHandle(), buf, len)`.
   전체에 보내는 것은 **스케치가** `for` 로 돈다.
3. **연결 유지 중 advertising 재시작도 라이브러리가 아니라 스케치가 한다.**
   `connect_callback` 안에서 `if (count < MAX) Advertising.start(0)`.
   → 라이브러리에서 자동 재시작을 넣지 마라. 상류와 동작이 달라진다.

또 하나: 상류는 `_connection[conn_hdl]` 로 **핸들을 배열 인덱스로 그대로 쓴다.**
SoftDevice 가 핸들을 `[0, 링크수)` 로 준다는 전제다. 우리도 같이 가되
`conn_hdl >= BLE_MAX_CONNECTION` 이면 NULL 을 돌려주는 경계 검사는 넣는다.

받아 둔 상류 소스: `bluefruit.h`, `BLEPeriph.cpp`, `services/BLEUart.{h,cpp}`,
`BLECharacteristic.h`, `examples/Peripheral/bleuart_multi/bleuart_multi.ino`.
(스크래치패드에만 있다 — 저장소에는 넣지 않는다.)

#### 구현 순서

1. **`SD_BLE_PERIPH_LINK_COUNT` 를 보드별 설정으로 뺀다.**
   `sd_event_pump.c` 의 `#ifndef` 블록을 **`sd_event_pump.h` 로 옮겨** 라이브러리에서도
   보이게 하고, `boards.txt` 의 `build.extra_flags` 에 `-DSD_BLE_PERIPH_LINK_COUNT=N` 을 준다.
   `build.extra_flags` 는 core/라이브러리/스케치 recipe 에 모두 들어가므로 한 곳에서 통한다.
   값: `nu54vdk`/`xiao_nrf54l15` = **5**, `nu54dk`(L05) = **2**
   (L05 는 예약이 `0x4780`=18,304 B 라 실측상 2개까지다), 헤더 기본값 = 1.
2. **`bluefruit.h/.cpp`** — `BLE_MAX_CONNECTION` = `SD_BLE_PERIPH_LINK_COUNT`,
   `BLEConnection _connection[BLE_MAX_CONNECTION]` (**고정 배열 — `new` 금지**,
   상류는 `new` 를 쓰지만 전역 규칙 위반), `_prph_count` 보관,
   `uint8_t connected(void)`(개수) + `bool connected(uint16_t)`,
   `connHandle()` = 마지막 연결, `attMtu(conn_hdl)` / `maxPayload(conn_hdl)`.
   `_att_mtu` 는 **연결별**로 옮겨야 한다 (지금은 싱글턴 멤버 하나다).
3. **TX 세마포어를 연결별 배열로.** 지금 `_tx_sem` 이 하나라 A 링크의
   `HVN_TX_COMPLETE` 가 B 링크 대기자를 깨운다. `_waitTxComplete(conn_hdl, ms)` 로 바꾼다.
4. **`BLECharacteristic::notify(conn_hdl, ...)`** 추가, 무핸들판은 `connHandle()` 위임.
5. **`BLEUart`** — `write(conn_hdl,...)` 를 실제 연결별 notify 로, 청크 크기는
   그 연결의 MTU 를 쓴다. `notifyEnabled(conn_hdl)` 추가.
6. **`BLEPeriph::connected()` / `connected(conn_hdl)`** (상류 호환).
7. `Advertising::_restartIfNeeded()` 는 `_running` 이면 건너뛴다
   (연결 중에도 광고를 켜 두는 경우가 생기므로 이중 start 를 막는다).
8. 예제 `examples/Peripheral/bleuart_multi` 추가 → 링크 수를 5 로 올리고 실기 확인.

#### 검증

호스트 2대가 동시에 붙어야 한다. Mac(bleak) + 폰 조합을 쓴다.
확인할 것: 연결 2개 동시 유지, 각 링크의 MTU 가 따로 협상되는지,
한쪽만 끊었을 때 남은 링크가 살아 있는지, 광고가 다시 도는지.

⚠ 링크가 늘면 라디오 시간을 나눠 쓰므로 **연결당 처리량이 떨어진다.**
`SD_BLE_EVENT_LENGTH`(현재 6 = 7.5 ms) 를 같이 봐야 할 수 있다. 실측 대상이다.

### B7 — GATT 클라이언트 + 콜백 지연 실행 ✅ (2026-09-06)

`getPeerName()` 이 빈 문자열 대신 실제 이름을 준다. 실기에서 `Connected to Mac`.

#### 핵심은 GATT 가 아니라 **콜백을 어디서 도느냐** 였다

`sd_ble_gattc_char_value_by_uuid_read()` 한 번이면 탐색과 읽기가 같이 끝나서
GATT 쪽은 간단했다. 문제는 그게 **블로킹**이고, 상류 예제가 그걸
**연결 콜백 안에서** 부른다는 것이었다.

우리는 콜백을 BLE 이벤트 태스크에서 직접 불렀다. 거기서 블로킹하면
기다리는 응답 이벤트를 처리할 주체가 자기 자신이라 **영영 안 온다.**
Adafruit 이 `ada_callback()` 으로 콜백을 다른 태스크에 넘기는 이유가 그것이다.

그래서 콜백 태스크(`ble_cb`, 우선순위 2 — 이벤트 태스크 3보다 낮게)와 큐를 뒀다.
연결/해제 콜백이 거기서 돈다. 부수 효과로 **콜백이 오래 걸려도 이벤트 펌프가
막히지 않는다.**

⚠ 슬롯 반납(`_end()`)도 콜백 태스크로 옮겼다. 이벤트 핸들러에서 바로 반납하면
콜백이 도는 시점엔 이미 다른 연결이 그 슬롯에 들어와 있을 수 있다.
대신 링크가 꽉 찬 상태에서 끊고 곧바로 다시 붙으면 슬롯이 아직 안 비어
거절될 수 있다 — 콜백 태스크가 금방 돌므로 실제로는 좁은 창이다.
(Adafruit 은 반대로 즉시 free 해서, 그쪽은 해제 콜백에서 `Connection()` 이 NULL 이다.)

#### GATTC 는 RAM 을 더 안 먹었다

착수 전에 "`BLE_CONN_CFG_GATTC` 때문에 RAM 이 더 필요할 것" 이라고 봤는데 **틀렸다.**
그 설정은 write command 큐 깊이만 조정하는 것이고, GATT 클라이언트 절차 자체는
기본 구성으로 동작한다. cfg_set 을 추가하지 않았고 요구량은 `0x20006DD8` 그대로다.

#### 남은 것

`getPeerName()` 은 **상대가 이름을 공개할 때만** 값이 온다. iOS 는 본딩 전에
GAP Device Name 을 주지 않는 경우가 많아 0 이 정상일 수 있다.
반환형은 상류에 맞춰 `bool` -> `uint16_t`(길이) 로 바꿨다.

### B8 — central 역할 (2026-09-06: RAM 배분 확정)

**nRF54L15 는 `peripheral 4 + central 1`, 예약 32 KB 다** (`0x20007F48` 실측, 여유 184 B).
앱 RAM 은 233,472 -> 229,376 B 로 4 KB 준다 (256 KB 중 1.6%).

비용은 역할이 아니라 **링크 수**에 붙는다 — `3+1`(`0x20006DC0`) 이 `4+0`(`0x20006DD8`)
보다 24 B 싸다. 28 KB 를 유지하려면 `3+1` 로 가면 되지만, 다중 peripheral 을
4 개로 유지하려고 예약을 키웠다.

#### ⚠ 역할별로 다른 MTU 를 줄 수 없다

`sd_ble_cfg_set()` 으로 두 번째 `conn_cfg_tag` 를 만들려 하면
**`NRF_ERROR_NOT_SUPPORTED`(6)** 이 온다. S145 v10.0.1 은 **연결 구성을 하나만**
허용한다. Adafruit 은 nRF52 에서 `CONN_CFG_PERIPHERAL` / `CONN_CFG_CENTRAL` 을
나눠 역할마다 다른 MTU·큐를 주는데, **그 설계를 그대로 옮길 수 없다.**

결과: 두 역할이 MTU 247 · 이벤트 길이 · notify 큐 3 을 공유한다. central 에만
작은 MTU 를 줘서 RAM 을 아끼는 길은 없다. 실측표는 `docs/MEMORY-MAP.md`.

착수 전 추측은 "태그를 나누는 게 유리할 것" 이었는데 **애초에 선택지가 아니었다.**

#### 역할 배분을 런타임으로 옮겼다 (2026-09-07)

`begin(prph, central)` 인자가 실제로 `sd_ble_cfg_set()` 까지 간다.
그전에는 boards.txt 의 `-D` 로 **컴파일 타임에 고정**돼 있어서 `begin(0, 4)` 가
무조건 false 였다.

계기는 예제 전수 조사였다. 72개의 `Bluefruit.begin()` 호출을 훑어 보니
`central_bleuart_multi` 가 **`begin(0, 4)`**, `dual_bleuart` 와
`rssi_proximity_central` 이 `begin(1, 1)` 이다. 컴파일 타임 고정으로는
`(4,0)` 과 `(0,4)` 를 같이 지원하려면 링크 8개분 RAM 을 잡아야 하는데,
런타임이면 **어느 쪽이든 4개분만** 쓴다.

실측으로 확인 (예약 32 KB): `(4,0)`=`0x6DD8`, `(0,4)`=`0x6868`,
`(2,2)`=`0x6C00`, `(0,5)`=`0x7828` 전부 성공. `(8,0)`/`(6,2)` 는
`begin()`=false, `err=0x04`(NO_MEM), 필요량 `0x2000B3F8`/`0x2000B220` 을 보고한다.

같은 이유로 `configPrphConn(mtu, event_len, hvn_qsize, ...)` 과
`configPrphBandwidth()` 도 이제 **실제로 동작한다** (begin() 전에 부를 것).

⚠ MTU 는 `SD_BLE_ATT_MTU`(컴파일 타임) 를 넘길 수 없다. 이벤트 버퍼가 그 값
기준으로 잡혀 있어서, 넘기면 이벤트가 잘린다. 넘기면 `sdEnable()` 이 거절한다.

#### 스캐너 ✅ (2026-09-06)

`BLEScanner` 가 동작한다. 실기에서 주변 광고 수신, 주소 역순 출력, RSSI,
AD 타입 파싱(이름), active scan(스캔 응답), 16비트 UUID 필터까지 확인했다.
예제는 `examples/Central/central_scan`, 상류 `central_scan.ino` 도 그대로 컴파일된다.

⚠ **보고 하나마다 스캔이 멈춘다** (SoftDevice v6 이후). 콜백 안에서 `resume()` 을
불러야 이어진다. 안 부르면 "처음 하나만 잡히고 조용하다" 가 된다.
필터에 걸러진 보고도 라이브러리가 대신 `resume()` 해 준다 — 안 그러면 필터에
처음 걸린 순간 스캔이 멈춘다.

⚠ 스캔 콜백은 **연결 콜백과 달리 BLE 이벤트 태스크에서 직접** 불린다.
`report` 가 이벤트 버퍼를 가리켜서 미루면 덮이기 때문이다.
그래서 **스캔 콜백 안에서 블로킹 호출을 하면 안 된다** (`getPeerName()` 등).
출력·필터·연결 시작은 괜찮다.

#### central 연결 + 클라이언트 UART ✅ (2026-09-07)

두 보드로 실증했다. XIAO(central) ↔ NU54-DK(peripheral), MTU 247, 양방향:

```
XIAO:     Found E8:FA:C5:C4:FD:A5  rssi -64, connecting
          Connected, discovering UART service ... ready, MTU 247
          [peer] tick-0 ... tick-7
NU54-DK:  [from-central] hello-0 ... hello-6
```

경로 전체가 동작한다: UUID 필터 스캔 -> 연결 -> MTU 교환 -> 서비스/characteristic/
CCCD 탐색 -> 알림 켜기 -> 양방향 데이터.

⚠ **central 은 MTU 교환을 우리가 먼저 걸어야 한다.** peripheral 일 때는 상대가
걸어 주지만 central 일 때는 걸어 주는 쪽이 없다. 넣기 전에는 MTU 가 23 이었다.
콜백 태스크에서 **스케치 콜백보다 먼저** 끝낸다 — 콜백 안의 `discover()` 도
GATT 절차라 겹치면 `NRF_ERROR_BUSY` 가 난다.

⚠ **characteristic 은 UUID 로 가려야 한다.** 탐색 응답의 순서로 고르면 안 된다 —
규격이 순서를 정해 두지 않아 구현마다 다르다.

⚠ **characteristic 탐색은 한 번에 안 끝난다.** 마지막 핸들 다음부터 다시 물어
범위가 끝날 때까지 반복해야 한다. 한 번만 부르면 뒤쪽이 조용히 빠진다.

#### 클라이언트 서비스 일반화 ✅ (2026-09-07)

`BLEClientService` / `BLEClientCharacteristic` 가 생겼고 `BLEClientUart` /
`BLEClientBas` / `BLEClientDis` 가 그 위에 얹혔다. **상류 `central_bleuart` 가
이제 컴파일된다** (배터리·장치정보 클라이언트가 없어서 막혀 있었다).

⚠ **서비스가 한 번의 탐색으로 등록된 characteristic 을 모두 채운다.**
characteristic 마다 따로 훑으면 DIS(특성 6개)에서 6번 왕복한다.
UUID 로 나눠 주는데, **응답 순서로 고르면 안 된다** — 규격이 순서를 안 정한다.

⚠ `BLEClientDis` 만 예외로 **부를 때마다 찾아 읽는다.** 특성이 여섯인데 보통
한둘만 쓰므로 상주시키는 것보다 낫다 (Adafruit 도 같은 이유).

⚠ **indication 은 `sd_ble_gattc_hv_confirm()` 으로 확인 응답을 보내야 한다.**
안 보내면 상대가 다음 것을 안 보내고 결국 절차 타임아웃으로 링크가 끊긴다.

#### PC 를 peripheral 로 쓸 수 있다 — `extras/mac_peripheral.py`

보드가 한 대뿐일 때 macOS 를 상대 peripheral 로 띄운다 (CoreBluetooth
`CBPeripheralManager`, pyobjc). NUS 를 광고·제공하므로 central 시험에 충분하다.

⚠ **macOS 는 DIS(0x180A) / 배터리(0x180F) 같은 예약 서비스의 게시를 거부한다**
(`CBError 8 "UUID is not allowed"`). 그래서 `BLEClientBas` / `BLEClientDis` 는
이걸로 못 재고 보드가 필요하다.

**실기 결과** (XIAO central ↔ Mac peripheral): 스캔 필터 -> 연결 -> MTU 247 ->
서비스/characteristic/CCCD 탐색 -> 알림 구독 -> 양방향 데이터.

```
XIAO: Found 80:A9:97:3F:E5:18  rssi -44, connecting
      Connected, discovering UART service ... ready, MTU 247
      [peer] mac-tick-1 ... mac-tick-8
Mac:  central subscribed to TX
      <- from central: b'xiao-1' ... b'xiao-5'
```

**남은 것:** `BLEClientBas` / `BLEClientDis` 는 컴파일만 됐고 실기 확인 전이다.
NU54-DK 에 `Peripheral/bleuart`(DIS·배터리 포함)를 올리고 상류
`central_bleuart` 를 XIAO 에 올리면 한 번에 확인된다.

nRF54L05 도 실측해서 **peripheral 2 + central 1** 로 올렸다 (`0x20005C38`).
둘 다 가지려고 예약을 `0x4B80` -> `0x5D00` 으로 키웠고, 앱 RAM 은
78,976 -> 74,496 B 다.

⚠ **프로브가 두 개 붙어 있으면 arduino-cli 업로드가 실패한다.**
어느 프로브인지 못 고른다. `probe-rs ... --probe <VID:PID:serial>` 로 직접 굽되,
**그 과정에서 SoftDevice 영역이 지워질 수 있다** — 실제로 겪었고 증상은
`begin=0 err=0 need=0` (cfg_set 이 아예 안 불린 상태) 이었다.
SoftDevice hex 를 다시 구우면 복구된다.

### B10 — 페어링 / 본딩 ✅ (2026-09-07)

Mac 을 상대로 4가지를 실증했다. bleak 은 macOS 에서 `pair()` 를 못 하지만,
**암호화가 필요한 characteristic 을 읽으면 CoreBluetooth 가 자동으로 페어링을 건다.**
그래서 사람 손 없이 반복 시험이 된다 (PIN 흐름만 폰이 필요하다).

```
[connect] handle=0 notifyEnabled=0
[secured] handle=0 ...            <- 암호화됨
[pair done] status=0x00 bonds=1   <- 본딩 저장
...끊었다 다시 연결...
[connect] handle=1 notifyEnabled=1  <- 재페어링 없이 암호화 + CCCD 복원
```

#### 걸린 것 셋 — 전부 조용히 실패하는 종류였다

**1. `sec_mode_set()` 의 니블 순서가 뒤집혀 있었다.**
`SECMODE_*` 상수는 `ble_gap_conn_sec_mode_t` 바이트를 그대로 옮기도록 만든 값이라
**0x<lv><sm>** 다. 뒤집어 쓰면 `SECMODE_ENC_NO_MITM`(0x21) 이 sm=2 lv=1(서명)이 되어
`characteristic_add` 가 `NRF_ERROR_INVALID_PARAM` 을 낸다.
⚠ **OPEN(0x11)과 NO_ACCESS(0x00)가 좌우대칭이라 그동안 안 드러났다.**
암호화 권한을 처음 쓰는 순간 터졌다.

**2. CCCD 를 시스템 서비스만 저장했다.**
`sd_ble_gatts_sys_attr_get()` 에 `SYS_SRVCS` 만 줬더니 8바이트가 저장되는데
**NUS 처럼 우리가 만든 서비스의 CCCD 는 안 들어간다.** `USR_SRVCS` 를 함께 줘야 한다.
증상은 "저장은 되는데 재연결하면 알림이 꺼져 있다" 였다.

**3. `SYS_ATTR_MISSING` 을 기다리면 안 된다.**
그 이벤트는 상대가 CCCD 가 걸린 속성을 **건드려야** 온다. 재연결한 상대는 이미
구독했다고 믿고 아무것도 안 건드리므로 영영 안 온다. **연결 즉시** 복원해야 한다.

### B11 — LESC ✅ (2026-09-07)

**micro-ecc 소프트웨어 P-256** 으로 했다. Mac 과 페어링해 `LESC=1` 확인.

nRF54L 의 CRACEN 하드웨어 가속기는 **못 쓴다** — `nrfx_cracen.h` 가 난수만 내주고,
공개키 엔진은 NCS 의 `nrf_security`(PSA Crypto)를 거쳐야 닿는데 Arduino 코어에
끌어오기엔 너무 크다. Adafruit 이 nRF52840 에서 쓰는 CryptoCell 도 이 칩엔 없다.
Nordic 자신이 CryptoCell 없는 nRF52832 에서 쓴 방법이 micro-ecc 다.

**실측**: 키쌍 생성 **202 ms**(부팅 때 한 번), 플래시 **약 4 KB** 증가.

⚠ **바이트 순서가 유일한 함정이다.** BLE 는 공개키를 {X,Y} 각각 **리틀엔디안**,
DHKey 도 리틀엔디안으로 다루는데 micro-ecc 는 **빅엔디안** 배열을 쓴다.
32바이트 덩어리마다 뒤집는다. 안 뒤집으면 그냥 페어링이 실패하고, 증상은
"LESC 만 안 된다" 로만 보인다.

⚠ `uECC_VLI_NATIVE_LITTLE_ENDIAN` 으로 해결하려 하지 마라 — Nordic 문서가 그 매크로를
바꾸면 라이브러리가 제대로 동작하지 않을 수 있다고 경고한다.

⚠ S145 API 가 nRF52 와 또 달랐다:
- `sd_rand_application_bytes_available_get()` 이 **없다.** 풀이 비면
  `sd_rand_application_vector_get()` 이 오류를 내므로 그때 잠깐 쉬었다 재시도한다
- `sd_ble_gap_lesc_dhkey_reply()` 가 **sec_status 인자를 하나 더** 받는다

#### 시험 요령 — 상대의 옛 본딩이 걸림돌이다

우리 본딩만 지우면 Mac 에는 옛 키가 남아, 재연결 때 우리가 `SEC_INFO_REQUEST` 에
NULL 로 답하고 **macOS 가 그냥 끊어 버린다.** 시스템 설정에서 기기를 잊게 하는 대신
`Bluefruit.setAddr()` 로 **주소를 새로 잡으면** 새 기기로 보고 새로 페어링한다.
자동 반복 시험에는 이 쪽이 훨씬 낫다.

### B12 — HID ✅ (2026-09-07)

`BLEHidGeneric` / `BLEHidAdafruit` 으로 키보드·마우스·미디어 키를 만들었다.
상류 `blehid_keyboard` · `blehid_mouse` · `blehid_camerashutter` · `blehid_keyscan`
이 컴파일된다.

#### TinyUSB 를 안 쓴다

Adafruit 은 HID 정의를 **TinyUSB 에서 빌려 쓴다** (`class/hid/hid.h` 의
`hid_keyboard_report_t`, `HID_KEY_*`, `TUD_HID_REPORT_DESC_*`). nRF52840 은 USB
하드웨어가 있어 TinyUSB 를 어차피 싣기 때문이다.

우리는 그럴 수 없다 — **nRF54L15 에 USB 하드웨어가 없고**, TinyUSB 의 hid.h 는
165 KB 에 `common/tusb_common.h` 를 끌고 온다. 필요한 것만 `ble_hid_defs.h` 에
직접 적었다: 리포트 구조체, 키코드, ASCII 표, 합본 리포트 맵(171 바이트).
이름은 상류와 같게 둬서 예제가 그대로 컴파일된다.

⚠ **Report Reference 디스크립터(0x2908)가 필수다.** 없으면 호스트가 어느 리포트가
어느 ID 인지 몰라 HID 가 통째로 동작하지 않는다 — 연결은 되는데 키가 안 먹는다.
`BLECharacteristic::addDescriptor()` 를 새로 만들어 붙였다. SoftDevice 는
디스크립터를 **직전에 추가한 characteristic** 에 붙이므로 순서를 지켜야 한다.

⚠ **HID 는 암호화된 링크를 요구한다.** 리포트 characteristic 의 권한이 암호화
이상이어야 한다. 본딩(B10)이 먼저 된 뒤에야 의미가 있다.

#### ⚠ Mac 으로는 GATT 계층조차 확인할 수 없다

**Apple 이 HID 서비스(0x1812)를 앱에 안 보여준다.** 시스템이 직접 처리하는
프로파일이라 CoreBluetooth 가 걸러낸다. 실제로 `blehid.begin()` 이 0 을 돌려주고
광고에 0x1812 가 실려 있는데도, bleak 은 DIS 만 보고 HID 는 못 봤다.
(같은 이유로 iOS/macOS 는 앱이 0x1812 서비스를 **게시**하는 것도 막는다.)

그래서 확인은 **호스트 Bluetooth 설정에서 페어링하고 키를 눌러 보는 것**뿐이다.
✅ 그렇게 확인했다 — 페어링 후 버튼을 누르니 키가 입력됐다.
`examples/Peripheral/blehid_button` 이 그 용도다 — 버튼 하나에 키 하나만 매핑해서,
전체 키보드 예제처럼 아무 창에나 타이핑하는 사고를 막는다.

우리 보드를 central 로 써서 0x1812 를 읽는 길도 있다 (`BLEClientService` 로).
보드가 두 대 붙어 있을 때 해 볼 만하다.

### B13a — 게임패드 ✅ (2026-09-08)

`BLEHidGamepad` + `hid_gamepad_report_t` / `GAMEPAD_HAT_*` / `GAMEPAD_BUTTON_*`.
상류 원본 `blehid_gamepad.ino` 가 include 3줄 삭제만으로 컴파일된다.

실기: XIAO nRF54L15 + Mac. `hidutil list` 가 **UsagePage 1 / Usage 5(Gamepad)** 로
잡고, `ioreg` 로 꺼낸 리포트 맵이 우리가 넣은 바이트열과 **완전히 일치**했다.
브라우저 게임패드 테스터에서 버튼 32개 / 축 6개가 잡히고 값이 움직인다.
macOS 리포트 카운터(`ReportAvailableCalls`)가 0 -> 23 으로 올라간다.

TinyUSB 를 안 쓰므로 `TUD_HID_REPORT_DESC_GAMEPAD()` 가 펼쳐진 바이트열을
직접 적었다 (`BLEHidGamepad.cpp`). 구조체와 맵이 어긋나면 값이 엉뚱한 자리에서
읽히므로 `static_assert(sizeof(...) == 11)` 로 빌드에서 막았다.

#### ⚠ 상류를 그대로 옮기면 리포트가 하나도 안 나간다 — 하루의 절반을 썼다

상류의 단일 연결 API 는 `BLE_CONN_HANDLE_INVALID` 를 아래로 내려보내고
**Adafruit 의 `BLECharacteristic::notify()` 가 그것을 실제 핸들로 치환**한다.
우리 `notify()` 는 그 치환을 하지 않고 `Bluefruit.connected(conn)` 에서 바로
false 를 돌려준다. 그대로 옮겼더니 모든 리포트가 첫 줄에서 버려졌다.

→ `Bluefruit.connHandle()` 을 넘긴다. `BLEHidAdafruit` 이 이미 그렇게 하고 있었다.
  **상류 파일을 옮길 때 이 치환 관례가 다른지 먼저 확인하라.**

증상이 고약한 이유: 연결·페어링·암호화·MTU·리포트 맵이 전부 정상이라
**호스트 문제로 보인다.** 실제로 낡은 본딩을 의심해 양쪽 본딩을 다 지웠는데
그건 원인이 아니었다. macOS 는 HID 서비스를 앱에 감추므로(§B12) bleak 으로도
못 본다.

**원인을 가른 계측**: GATTS 쓰기를 전부 찍었다. macOS 가 `h=26` 에 `01 00`
(notify 켜기)을 **정상적으로 쓰고 있었다**는 것이 드러나면서 범위가 우리 쪽으로
좁혀졌다. 그 다음은 `notify()` 를 따라 내려가면 바로 나온다.
호스트가 무엇을 하는지 모를 때는 **추측하지 말고 쓰기를 덤프하라.**

#### macOS 에서 HID 를 확인하는 법 (bleak 이 막힐 때)

| 무엇 | 명령 |
|---|---|
| 열거·Usage 확인 | `hidutil list \| grep -i <이름>` |
| 호스트가 받은 리포트 맵 | `ioreg -l -r -c IOHIDResourceDeviceUserClient` 의 `ReportDescriptor` |
| 리포트가 실제로 도착하는지 | 같은 출력의 `DebugState.ReportAvailableCalls` |

`ReportAvailableCalls` 가 0 이면 호스트에 아무것도 안 오는 것이다.
이 세 가지가 있으면 GUI 없이도 대부분 판정된다.

### 이어서 작업할 때 알아 둘 것

- **SoftDevice hex 를 먼저 구워야 한다.** 앱만 구우면 `sdEnable()` 이 SVC 를
  널 포인터로 포워딩한다. `docs/HIL/M3-softdevice.md` §5 에 명령이 있다
- **`g_sd_stage` 로 어디까지 갔는지 읽는다.** SoftDevice API 는 실패해도 원인이
  안 보이므로 단계 마커 + `m_last_error` 를 SWD 로 읽는 게 가장 빠르다
- **오류 코드는 헤더 retval 주석을 끝까지 읽어라.** A 단계에서 막힌 네 건이
  전부 거기 답이 있었다. 특히 `sd_ble_enable()` 의 `INVALID_STATE` 는
  "이미 초기화됨" 이 아니라 **RNG 미시딩**이었다
- **진단 출력을 부팅 때 한 번만 찍지 마라.** USB CDC 는 포트를 여는 사이에
  놓친다. 실패해도 주기적으로 찍게 해라
- 시험 스케치: `~/Documents/Arduino/nrf54_ble_adv/` (advertising 스파이크)

### (e) M2 — Arduino API

`analogRead`/`analogWrite`/`Wire`/`SPI`/`attachInterrupt`.
**착수 전에 CLAUDE.md §7 F10(nrfx 4.x 규칙)의 체크리스트를 반드시 읽어라.**
M1 에서 UARTE/GRTC 로 태운 함정이 그대로 반복된다.

---

## 2.5 `Serial` 수신이 32바이트 단위였다 — ✅ 고침 (2026-09-07)

**증상:** 짧은 문자열을 보내면 스케치가 못 읽었다. 32바이트가 찰 때까지 DMA 버퍼에
갇혀 있다가 나중 데이터가 청크를 채우면 **한꺼번에** 올라왔다.

**실기 재현 (NU54-DK):** 2바이트·5바이트 → 0개. 이어서 31바이트 → 32개가 쏟아짐.

**원인:** `RX_CHUNK = 32` 짜리 EasyDMA 버퍼를 걸어 두고, **버퍼가 다 차야** 오는
완료 이벤트로만 링버퍼에 옮겼다. 유휴 감지가 없었다.

**고친 방법:** `RX_CHUNK = 1`, 그리고 `NRFX_UARTE_RX_ENABLE_CONT` **해제**.
1·2·5·64·1024 바이트 모두 손실 없이 수신 확인.

#### 왜 이렇게 골랐나 — 다른 길들

| 방법 | 판단 |
|---|---|
| UARTE **FRAMETIMEOUT**(유휴 감지)로 부분 버퍼 끊기 | ❌ **nRF54L 에서 그 이벤트가 수신 도중 걸리면 뒤따르는 데이터의 비트가 깨진다**고 Nordic 이 확인 |
| RXDRDY → DPPI → **TIMER 카운터**로 바이트 세기 | 정석이고 Nordic 이 NCS 3.1 에서 택한 길(`UARTE_NRFX_UARTE_COUNT_BYTES_WITH_TIMER`). 다만 TIMER 인스턴스와 DPPI 채널을 선점한다 |
| **1바이트 DMA** (채택) | Adafruit nRF52 코어와 같은 구조(`RXD.MAXCNT = 1`). 자원을 안 먹고, 115200 에서 초당 약 11.5k 인터럽트로 감당된다 |

아주 높은 보드레이트가 필요해지면 TIMER + DPPI 로 옮긴다.

⚠ `NRFX_UARTE_RX_ENABLE_CONT` 는 ENDRX -> STARTRX 하드웨어 short 인데,
nrfx 문서가 **짧은 버퍼와 함께 쓰지 말라**고 못박는다. 끄면 드라이버가 ENDRX
인터럽트에서 다음 전송을 걸고, 그 틈의 바이트는 UARTE 하드웨어 FIFO 가 받는다.

#### ⚠ XIAO 의 VCOM 은 큰 입력 버스트에 멎는다 (보드 문제, 코어 아님)

XIAO 도 USB 를 다시 꽂으면 1·2·5·64 바이트가 정확히 수신된다. 그런데
**한 번에 1024 바이트를 밀어 넣으면 그 뒤로 수신이 완전히 죽는다** — 5바이트도
안 들어온다. 송신(보드 -> PC)은 그동안에도 멀쩡하다.

**우리 코어 문제가 아니라는 근거:**
- 같은 펌웨어로 NU54-DK(CP2102N)는 1024 바이트를 손실 없이 받는다
- **타깃만 리셋해도 안 살아난다.** 리셋하면 `Uart::begin()` 이 다시 도니
  우리 쪽 상태였다면 복구돼야 한다
- USB 를 뽑았다 꽂아야만 살아난다

즉 XIAO 온보드 CMSIS-DAP 의 **VCOM 이 호스트 -> 타깃 방향에서 멎는다.**
nrfx 드라이버도 오류 이벤트에서 수신을 멈추지 않는다(보고만 한다)는 것을
소스로 확인했다.

**사용자 안내:** XIAO 에서 시리얼 입력이 갑자기 안 되면 USB 를 다시 꽂는다.
큰 덩어리를 한 번에 붙여 넣지 말고 나눠 보낸다.

---

## 2.6 처리량 실측 — ✅ (2026-09-08)

**병목은 우리 쪽이 아니었다.** 이게 결론이다.

### 무엇을 추가했나 (커밋 `d7f95b8`)

상류 `throughput` 이 딱 네 가지 때문에 막혀 있었다.

| 추가 | 하는 일 |
|---|---|
| `BLEConnection::requestPHY(phy = AUTO)` | 2M PHY 요청 |
| `BLEConnection::requestDataLengthUpdate()` | 링크 계층 DLE. NULL 이면 스택이 최대치 |
| `BLEConnection::requestMtuExchange(mtu)` | **우리가 먼저** 거는 MTU 협상 (GATT 클라이언트) |
| `BLEUart::setNotifyCallback()` | 상대가 알림을 켜는 순간 — `BLECharacteristic::setCccdWriteCallback` 위에 얹었다 |

**거는 것만큼 받아 적는 것이 중요하다.** 협상된 MTU 를 아무도 저장하지 않으면
244 로 합의해 놓고도 `BLEUart` 가 계속 20바이트씩 쪼갠다. 그래서
`BLE_GATTC_EVT_EXCHANGE_MTU_RSP` / `BLE_GAP_EVT_PHY_UPDATE` /
`BLE_GAP_EVT_CONN_PARAM_UPDATE` 를 링크별로 기록하게 했다
(`getPHY()` / `getMtu()` / `getConnectionInterval()`).

⚠ `BLEGatt` 의 블로킹 MTU 절차와는 **별개 경로**다. 거기서만 받으면 스케치가 직접
건 요청의 결과가 아무 데도 반영되지 않는다.

⚠ **`BLEUart::flush()` 는 수신 FIFO 를 비운다.** Stream 의 통상적인 의미가 아니지만
Adafruit 이 그렇게 정의했고 상류 예제가 그 전제로 돈다 (R12). 송신은 notify 라
애초에 비울 버퍼가 없다.

### 실측 (XIAO nRF54L15 ↔ macOS, `extras/mac_throughput.py`)

협상 결과: **2M PHY / ATT MTU 247 / 페이로드 244 B / 연결 간격 30 ms**

| 방향 | 속도 |
|---|---|
| 보드 → 맥 | 26~28 KB/s = **211~226 kbps** |
| 맥 → 보드 | 27~30 KB/s = **217~245 kbps** |

**왜 이 수준인가** — 연결 간격 **7.5 ms 를 요청했는데 맥이 30 ms 를 줬다.**
정하는 쪽은 central 이다. 30 ms 에 26.4 KB/s 면 연결 이벤트당 약 **790 바이트**이고,
이벤트당 처리량이 같다면 간격만 7.5 ms 가 될 때 산술적으로 **100 KB/s 대**가 된다.
즉 병목은 PHY 도 MTU 도 칩도 아니라 **호스트가 주는 연결 간격**이다.

이것이 "1M PHY 로 210 kbps 나오는데 그 이상 되냐" 는 질문의 답이다 —
**2M 에 MTU 247 을 다 붙여도 애플 호스트에서는 같은 자리다.**

### 알림 큐 깊이는 병목이 아니다 — 키우지 마라

`SD_BLE_HVN_TX_QUEUE_SIZE` 를 바꿔 가며 같은 시험을 돌렸다.

| 큐 깊이 | 다운링크 |
|---|---|
| 1 | 27.5 KB/s |
| **3 (현재)** | 26.4~28.2 KB/s |
| 6 | 26.7 KB/s |

**전부 측정 잡음 안이다.** RAM 만 먹으므로 3 을 유지한다.
이유는 위와 같다 — 큐가 아니라 연결 이벤트가 병목이라, 큐를 키워도 실을 자리가 없다.

### 재현 방법

1. 보드에 `examples/Peripheral/throughput` 을 굽는다
2. `python3 extras/mac_throughput.py 30`
3. **PHY / MTU / 연결 간격은 보드 쪽 시리얼에만 나온다.** CoreBluetooth 는 노출하지 않는다

⚠ **호스트 GATT 캐시에 걸리면 측정이 통째로 틀린다** (§4 의 7번).
처음 시도에서 PHY 1M / MTU 23 으로 붙어 "스택이 느리다" 로 오독할 뻔했다.
측정용 빌드에서 `setAddr()` 로 주소를 흔들어야 뚫렸다.

---

## 2.7 `BLEMidi` — ✅ (2026-09-12)

BLE-MIDI 1.0. 상류 원본 `blemidi.ino` 가 **include 3줄 삭제만으로 컴파일**되고,
우리 예제는 `examples/Peripheral/blemidi` 다. MIDI 라이브러리(Francois Best,
Library Manager 의 "MIDI Library" 5.0.2)가 메시지 인코딩을 하고 `BLEMidi` 는
그 라이브러리가 쓰는 `Stream` 이 된다.

**실기 (XIAO + Mac, bleak):**

| | |
|---|---|
| 서비스 | ✅ `03b80e5a-…` + IO characteristic 하나 (read/write/wwr/notify) |
| MTU | ✅ 247 |
| 보드 -> 호스트 | ✅ 5초에 20패킷. `88 8B 90 3C 64` = Note On 60 vel 100 |
| 타임스탬프 | ✅ 250 ms 간격이 13비트 필드에 정확히 실린다 |
| 호스트 -> 보드 | ✅ 보낸 `90 45 7F` 가 `note on ch=1 pitch=69 vel=127` 로 디코드 |

#### ⚠ 값 쓰기는 암호화된 링크가 필요하다 — 미페어링이면 조용히 안 온다

IO characteristic 의 권한이 `SECMODE_ENC_NO_MITM` 이라 **호스트 -> 보드 쓰기는
페어링 전에는 거부된다.** 그런데 **알림은 그대로 나간다** — CCCD 쓰기 권한은
열려 있고 notify 자체는 권한 검사를 받지 않기 때문이다.

그래서 증상이 "보내는 건 되는데 받는 게 안 된다" 로 나타나고, 파서를 의심하게 된다.
실제로 그렇게 한 번 헛짚었다. bleak 으로 시험할 때는 **암호화가 필요한
characteristic 을 먼저 읽어** CoreBluetooth 가 페어링을 걸게 하면 된다
(B10 에서 쓴 것과 같은 방법).

#### 상류와 다르게 한 것 셋

| | 왜 |
|---|---|
| `Adafruit_FIFO` 대신 자체 링버퍼 | 그 유틸리티가 우리 저장소에 없다. `BLEUart` 과 같은 구조로 맞췄다. 힙은 `begin()` 에서 잡는다 — 전역 객체 생성자가 힙보다 먼저 돌 수 있다 |
| 송신 조립 버퍼를 멤버로 | 상류는 `write()` 안의 `static` 이라 인스턴스가 여럿이면 섞인다 |
| 수신 파서의 범위 검사 | 상류는 `data[1]` 을 조건에 넣는데 두 분기가 하는 일이 같고, `len == 1` 이면 **버퍼 밖을 읽는다.** 조건을 하나로 줄이고 길이를 먼저 본다 |

#### 덤 — GATT 지문이 실전에서 걸렸다

게임패드에서 MIDI 로 스케치를 바꾸자 재연결에서 이렇게 떴다:

```
bond: stored CCCD is for a different GATT layout (71C1E6B5 != 8AA54C59).
Dropped - re-pair to get notifications back.
```

**이게 정확히 `f1c6fe0` 을 넣은 이유다.** B13a 때는 같은 상황을 만나고도 원인을
찾는 데 오래 걸렸다. 해결은 양쪽을 지우는 것이다 — 보드는 `clearbonds`,
호스트는 Bluetooth 설정에서 기기 삭제. **한쪽만 지우면**
`Peer removed pairing information` 으로 연결 자체가 거부된다 (실제로 겪었다).

---

## 2.8 `BLEClientCts` — ✅ (2026-09-12)

폰의 시계를 읽는다. 보드가 **peripheral 인데 GATT 클라이언트**인 구성이라
`BLEClientService`(B9) 위에 얹었다. 예제는 `examples/Peripheral/client_cts` 다 —
상류도 Central 이 아니라 Peripheral 아래 둔다.

**실기 (XIAO + iPhone, nRF Connect 로 연결):**

```
Connected. Asking to pair.
Link secured, looking for the Current Time service
  found
time: 2026-09-12 10:29:48  weekday=6  adjust=0x02
timezone: 36 quarter-hours, dst offset 0
```

호스트 시계와 초까지 일치. `weekday=6` = 토요일, `timezone 36` × 15분 = **+9시간(KST)**.

#### ⚠ 라이브러리 버그를 찾았다 — 보안 콜백이 이벤트 태스크에서 돌고 있었다

**이것이 이번 작업의 본체다.** 상류 `client_cts` 는 `secured` 콜백 안에서
`discover()` 를 부르는데, 그 콜백만 `_cb_queue` 를 안 타고 **BLE 이벤트 태스크에서
직접** 불리고 있었다. 탐색은 응답 이벤트를 기다리는 블로킹 절차라, 기다리는 이벤트를
처리할 주체가 자기 자신이 되어 영영 오지 않는다. B7 에서 연결 콜백을 옮긴 것과 같은
문제인데 보안 콜백만 빠져 있었다.

진단이 결정적이었다 — **같은 코드를 두 컨텍스트에서 돌려 비교했다**:

| 어디서 | 결과 |
|---|---|
| `secured` 콜백 안 | `service=-` / `discover=FAIL` |
| `loop()` 안 | `service=FOUND (29..34)` / `discover=OK` / 시각 정상 |

→ `BLE_CB_SECURED` 를 추가해 연결·해제와 같은 큐로 보냈다
(`bluefruit.cpp`, `BLESecurity::_invokeSecuredCallback`).
**안 고쳤으면 상류 스케치가 우리 코어에서 조용히 실패한다** (R12).

⚠ 증상이 사람을 엉뚱한 곳으로 보낸다. 폰은 멀쩡하고, 광고도 정상이고, 같은 코드가
`loop()` 에서는 된다. 게다가 내가 예제에 쓴 실패 메시지가
`not found - the peer does not publish one` 이라 **원인이 상대에게 있는 것처럼
읽혔다.** 실패 메시지에 원인을 단정해 적으면 안 된다 — 그 문구도 고쳤다.

#### 그 밖에 없어서 새로 만든 것

- **`Advertising.addService(BLEClientService&)`** — 상류 예제가 부르는데 우리에겐
  `BLEService&` 판만 있었다. 상류를 보니 일반 서비스 목록이 아니라
  **Solicitation UUID**(`0x14`/`0x15`)로 싣는다. "내가 제공한다" 가 아니라
  **"당신이 가졌다면 붙어 달라"** 는 정반대 의미다. iOS 는 CTS·ANCS 를 이 방식으로만
  열어 준다. **`BLEAncs` 에도 그대로 필요하다**
- `UUID16_CHR_LOCAL_TIME_INFORMATION`(0x2A0F) 이 표에 없어 추가
- 수신 알림의 길이 검사 — 상류는 받은 길이를 그대로 `memcpy` 해서, 상대가 규격보다
  긴 값을 보내면 구조체 뒤를 넘어 쓴다

#### 실기 시험에서 알아 둘 것

- **iOS 설정 → Bluetooth 목록에 안 뜬다.** 요즘 iOS 는 일반 BLE 주변기기를 거기
  잘 안 올린다 (상류 주석의 "Accessory 로 보인다" 는 옛 이야기다).
  **nRF Connect 로 연결**하면 된다
- 그런데 nRF Connect 는 **페어링을 걸지 않는다.** 그러면 링크가 암호화되지 않아
  iOS 가 CTS 를 안 준다. → 예제가 연결 콜백에서 `conn->requestPairing()` 을 먼저
  부른다. 상류에는 없는 부분이고, 없으면 아무 일도 안 일어난다
- Local Time Information 은 **규격상 선택**이다. 없다고 실패시키면 안 된다
  (iPhone 은 준다)

---

## 2.9 `BLEAncs` — ✅ (2026-09-12)

iPhone 의 알림을 받는다. B13b 중 가장 큰 작업이었다. 예제는
`examples/Peripheral/ancs`, 상류 원본 `ancs.ino` 는 include 3줄 삭제만으로 컴파일된다.

**실기 (XIAO + iPhone):** 알림 이벤트 -> 앱 이름·제목·본문 조회 -> 조각 응답 재조립까지
전부 동작한다. 한국어 본문 정상.

```
added   [other] 지갑: Tmoney
         사용자의 새로운 잔액은 ₩82,900입니다.
added   [social] 메시지: +82 1544-7200
```

응답은 조각으로 온다 — 한 건에 `DS hvx len=26 / 29 / 14 / 108` 처럼 네 번 나뉜다.

#### 콜백 컨텍스트가 **정반대로 두 개** 필요하다

이 서비스의 핵심 구조다. 잘못 놓으면 교착한다.

| 콜백 | 어디서 | 왜 |
|---|---|---|
| Notification Source | **콜백 태스크** | 이 안에서 스케치가 제목·본문을 가져오는데 그게 블로킹이다 |
| Data Source | **이벤트 태스크** | 위가 기다리는 동안 이쪽이 채워 줘야 한다. 같은 태스크면 막힌다 |

⚠ 우리 `BLEClientCharacteristic::setNotifyCallback()` 은 상류의
`use_ada_callback` 인자를 **받기만 하고 무시한다** — 항상 이벤트 태스크에서 직접 부른다.
그래서 범용 `Bluefruit._deferCallback()` 을 만들어 알림 쪽만 콜백 태스크로 넘겼다
(§2.8 의 보안 콜백과 같은 큐). **`use_ada_callback` 을 믿는 상류 코드를 옮길 때
이 점을 먼저 확인하라.**

상류의 `AdaMsg` 유틸리티는 우리 저장소에 없어, 필요한 만큼(버퍼 + 세마포어)만 직접 뒀다.
조각마다 깨우고 "머리에 적힌 길이만큼 다 왔는지" 는 부르는 쪽이 판단한다.

#### ⚠ `Serial` 은 태스크 안전하지 않다 — 여기서 드러났다

계측 중 출력이 이렇게 깨졌다:

```
a  e=3ans  v n11added   [other] 롯데O
```

`[ancs] NS hvx len=11` 과 `added [other] …` 가 **글자 단위로 섞였다.** 이벤트 태스크와
콜백 태스크가 동시에 `Serial` 에 쓴 결과다. 우리 `Serial` 에는 잠금이 없다.

**사용자도 겪는다** — `Scheduler.startLoop()` + BLE 콜백 조합이 흔하기 때문이다.
뮤텍스를 넣을지는 별도 판단이다 (모든 쓰기에 비용이 붙고, Adafruit 도 안 한다).
당장은 알아 두고 쓰는 쪽이 맞다.

#### ⚠ UTF-8 경계 — ASCII 로만 시험했으면 못 잡는다

폰은 버퍼 크기만큼 보내고 끊으므로 **한글 중간에서 잘린다**(글자당 3바이트).
예제에 문자 경계로 되돌리는 처리를 넣었는데, **첫 판이 틀렸다** — 끝의 continuation
바이트를 무조건 벗겨서 **온전한 마지막 글자까지 지웠다** (`메시지` -> `메시`).
온전한 다바이트 글자도 continuation 으로 끝난다. **바이트가 실제로 모자랄 때만**
버려야 한다.

#### 실기 시험 요령

- 연결하는 순간 **폰에 쌓여 있던 알림이 한꺼번에 쏟아진다.** 새 알림을 기다릴 필요가 없다
- **타이머 알람은 안 온다.** 알림 센터에 남지 않는 종류라서다. 문자·카드 결제처럼
  센터에 쌓이는 것으로 시험하라
- 캡처 창을 넉넉히 잡아라. 이번에 세 번을 창이 어긋나 "안 온다" 로 오판했다

---

## 2.10 `BLEClientHidAdafruit` — ✅ (2026-09-12)

남의 HID 기기를 읽는다. `BLEHidAdafruit` 의 반대쪽이다.
예제는 `examples/Central/central_hid`, 상류 원본도 include 3줄 삭제만으로 컴파일된다.

**실기 (보드 2대):** NU54-DK 가 키보드, XIAO 가 central.

```
KBD     boot 'a' press=1 release=1
CENTRAL keys 0x04('a')
CENTRAL keys                        <- 뗀 리포트
```

키코드 `0x04` 를 받아 ASCII `'a'` 로 옮기는 것까지 확인했다.

#### ⚠ 라이브러리 버그 — characteristic 이 9개를 넘으면 뒤쪽이 조용히 빠진다

**이번 작업에서 가장 중요한 발견이다.** `BLEClientService::discoverCharacteristics()` 가
`BLE_CLIENT_CHAR_MAX`(8) 짜리 배열로 **한 번만** 훑고 있었다. HID 서비스는
characteristic 이 11개라 **9번째부터가 통째로 빠졌고**, 하필 그게 부트 리포트였다.

증상은 `discover()` 실패 -> 연결 즉시 해제 반복이다. 서비스는 찾는데 그 안의
characteristic 이 없다고 나오니 상대(우리 키보드)를 의심하게 된다.

→ **범위를 나눠 여러 번 훑도록** 고쳤다. 배열을 키우지 않은 이유는 스택에 들고 있는
  크기가 서비스 크기를 따라 늘지 않게 하려는 것이다.

#### 없어서 새로 만든 것 둘

- **`BLEHidGeneric::bootKeyboardReport()` / `bootMouseReport()`** — 상류에는 있는데
  우리에겐 없었다. 부트 프로토콜은 **리포트 프로토콜과 다른 characteristic**
  (0x2A22 / 0x2A33)으로 나가므로, 이게 없으면 부트 모드 호스트에 아무것도 안 간다.
  characteristic 자체는 이미 만들고 있었다 — 보내는 길만 없었다
- **`hid_keycode_to_ascii[128][2]`** — 상류는 TinyUSB 매크로를 쓴다.
  손으로 옮기지 않고 **우리 `hid_ascii_to_keycode` 를 뒤집어 생성했다.**
  두 표가 어긋나면 "어떤 키만 이상하다" 로 나타나 찾기 어렵다.
  Enter 는 두 ASCII 가 같은 키코드로 가므로 줄바꿈 쪽을 남겼다

#### 보드 2대 시험 요령

- **프로브가 둘이면 `arduino-cli upload` 가 실패한다** (어느 쪽인지 못 고른다).
  `probe-rs download --probe <VID:PID:serial>` 로 직접 굽는다
- ⚠ 그렇게 구우면 **SoftDevice 가 지워진다.** 앱을 구운 뒤 SoftDevice hex 를
  다시 구워야 한다. 안 그러면 `Bluefruit.begin()` 이 조용히 죽는다
- 부트 모드는 **central 이 켜 줘야 한다** — `setBootMode(true)`.
  안 켜면 상대가 리포트 프로토콜로 보내고 부트 characteristic 은 조용하다
- 미디어 키(Consumer Control)는 **부트 프로토콜에 없어 받을 수 없다.** 상류도 같다

---

## 2.11 `Wire` (I2C) — ✅ (2026-09-12)

M2 의 첫 항목. `libraries/Wire/`, 예제는 `libraries/Wire/examples/i2c_scanner`.

#### ⚠ 코어가 아니라 **라이브러리**여야 한다 — 실측으로 확인

처음에 `cores/nrf54l/Wire.{h,cpp}` 에 넣었는데 **모든 스케치가 1 KB 씩 무거워졌다.**
`rtos_scheduler` 가 24,088 -> 25,112 B 로 늘었고, `nm` 으로 보니 I2C 를 쓰지 않는
`blinky` 의 ELF 에 `TwoWire` 와 `nrfx_twim` 이 통째로 들어 있었다.

이유는 §7 F13 ① 이다. Arduino 는 **스케치가 include 한 라이브러리만 링크**하지만,
`cores/` 는 `core.a` 로 묶이고 우리는 그것을 `-Wl,--whole-archive` 로 링크한다.
즉 **코어에 넣은 것은 안 쓰는 스케치에도 다 들어간다.**

→ `libraries/Wire/` 로 옮겼다. `rtos_scheduler` 가 24,088 B 로 정확히 되돌아왔다.
  **Adafruit 이 `Wire` 와 `SPI` 를 라이브러리에 두는 이유가 이것이다** — 따라야 했다.

부수 효과도 있다. 라이브러리에는 예제를 같이 넣을 수 있어
**IDE 의 파일 -> 예제 -> Wire** 에 바로 뜬다. 코어에 두면 그 자리가 없다.

⚠ **`SPI` 도 라이브러리로 만들어라.** 같은 이유다.

**실기 (XIAO):**

```
Wire : nothing                      <- 헤더 D4/D5, 아무것도 안 붙어 있다
Wire1: 0x6A
       IMU WHO_AM_I = 0x6A (LSM6DS3TR-C)
```

온보드 IMU 를 주소로 찾고, 레지스터 읽기(쓰기 -> repeated start -> 읽기)까지 됐다.
`Serial`(UARTE20)이 `Wire`(TWIM22)·`Wire1`(TWIM30)과 함께 살아 있다 — 블록 배정이
맞다는 뜻이다.

#### 체크리스트(§7 F10 ④)를 그대로 밟았고, 그래서 막힌 데가 없었다

M1 에서 UARTE·GRTC 로 태운 함정들이 이번엔 하나도 재발하지 않았다. 특히:

- **인스턴스 배정은 variant 가 이미 해 뒀다.** `WIRE_TWIM_INSTANCE` = TWIM22,
  `WIRE1_TWIM_INSTANCE` = TWIM30. 블록 충돌(§0)이 검토된 상태였다
- `nrfx_config.h` 에 `NRFX_TWIM_ENABLED` 와 인스턴스별 `NRFX_TWIMnn_ENABLED` 를
  **다 켰다.** variant 마다 고르는 번호가 달라서, 하나만 켜면 다른 보드에서
  링크가 깨진다
- **벡터를 직접 이었고 `nm` 으로 확인했다** — `T SERIAL22_IRQHandler` /
  `T SERIAL30_IRQHandler`. `W` 였으면 인터럽트가 뜨는 순간 죽는다
- IRQ 우선순위는 6 (`NRFX_DEFAULT_IRQ_PRIORITY`) — F2 의 5~7 범위 안

#### 만든 방식

- **인터럽트 + 세마포어**로 기다린다. 폴링으로 돌면 tickless 가 잠들지 못하고
  CPU 도 그만큼 태운다. 전송 중에는 다른 태스크가 돈다
- `endTransmission()` 의 반환값을 Arduino 규약대로 구분한다
  (0 성공 / 1 버퍼 초과 / 2 주소 NACK / 3 데이터 NACK / 4 그 밖).
  라이브러리들이 이 숫자로 분기한다
- 버퍼는 64바이트. Arduino 관례(32)보다 크게 잡았고, 넘치면 **조용히 자르지 않고**
  `endTransmission()` 이 1 을 돌려준다
- ⚠ **master 전용이다.** target(slave)은 없다 — TWIS 로 따로 만들어야 한다

#### ⚠ M2 DoD 가 SPI 에 걸려 있다

"I2C 센서 라이브러리 1종 동작" 을 하려 했는데 **라이브러리들이 `SPI.h` 를 조건 없이
include 한다.** `Adafruit_BusIO`(거의 모든 Adafruit 센서가 쓴다)도,
`Seeed_Arduino_LSM6DS3` 도 그렇다. **I2C 만 쓰는 코드여도 헤더가 없으면 컴파일이
안 된다.** 그래서 `SPI` 를 `attachInterrupt` 보다 먼저 해야 한다.

---

## 2.12 `SPI` — ✅ (2026-09-12)

`libraries/SPI/`. 예제 셋: `spi_flash_id`, `sd_card`, `sdfat_card`.

**실기 (NU54-DK, 헤더에 직접 땜한 플래시·SD):**

```
SCK=P2.01 MOSI=P2.02 MISO=P2.04 CS=P2.03
JEDEC: EF 40 18   Winbond, 16384 KB          <- W25Q128

  TEST2.WAV      8961960                     <- 표준 SD 라이브러리로 마운트
  DOOM           <dir> 16384
```

⚠ **이 배선은 기본 보드에 없다.** 사용자가 직접 땜한 것이라
`docs/boards/NU54-DK.md`(기본 보드 기준)는 고치지 않았다.

#### 루프백으로는 부족했을 검증이 끝났다

MOSI-MISO 를 묶는 루프백은 **모드가 틀려도 통과한다** — 자기 클럭을 자기가 받기
때문이다. 실제 슬레이브가 응답했으므로 **모드 0 의 극성·위상**이 맞고, P2 고속
도메인 배선과 출력 드라이브도 4 MHz 에서 문제없다는 것이 확인됐다.
variant 에 "M2 에서 실기 검증할 것" 으로 남아 있던 **P2 핀 배정이 이것으로 확정**됐다.

SD 쪽이 더 센 검증이다 — 저속 시작 -> 고속 전환, 다중 바이트 명령, 512바이트 섹터
읽기를 다 거쳐야 디렉토리가 나온다.

#### 만든 방식

- **라이브러리로 만들었다.** `Wire` 에서 배운 대로다 (§2.11) — 코어에 넣으면
  `--whole-archive` 때문에 안 쓰는 스케치까지 무거워진다
- 전송은 **인터럽트 + 세마포어**. 255바이트씩 나눠 보낸다
- **CS 는 건드리지 않는다.** Arduino 관례대로 스케치가 소유한다 — 버스 하나에
  장치가 여럿인 경우가 흔하다
- **`setPins()`** 를 넣었다. 기본값은 variant 지만 배선이 다르면 스케치에서 바꾼다.
  예제 둘 다 그 방법을 주석으로 보여 준다.
  ⚠ 도메인을 어기면 런타임에 조용히 안 된다 (SPIM00 = P2 전용)
- `usingInterrupt()` / `notUsingInterrupt()` 는 **빈 함수**다 (상류도 같다).
  동작하는 것처럼 보이지 않도록 keywords 에는 넣지 않았다

#### ⚠ 카드 감지(CD)는 이 보드에 배선돼 있지 않다

P2.00 에 **배선돼 있고 동작한다.** 보드의 Zephyr DTS 가 선언한
`GPIO_ACTIVE_HIGH` 와 일치한다.

#### ⚠ 한 번만 재면 "미배선" 으로 오판한다 — 실제로 그렇게 틀렸다

카드를 **꽂은 채** 재면 pullup=1 / pulldown=0 으로 **플로팅처럼 보인다.** 그래서
처음에 "안 붙어 있다" 고 결론냈는데 **틀렸다.** 카드를 빼고 다시 재니 pullup 을
걸어도 **0** — 외부에서 LOW 로 구동되고 있었다.

기계식 CD 스위치가 그렇게 생겼다: **슬롯이 비면 GND 로 단락, 카드가 들어가면 열림.**
열린 상태는 미배선과 전기적으로 구분되지 않는다.

→ **카드 유무 두 조건에서 재야 한다.** 한 조건만 보고 판정하지 마라.
   (`INPUT_PULLDOWN` 이 코어에 제대로 구현돼 있는지는 먼저 확인했다 —
   그게 안 됐으면 측정 자체가 무의미하다.)

풀업을 걸면 **카드 있음 = HIGH** 다. 예제 둘 다 CD 를 기본으로 켜 두었다 —
CD 가 없는 보드에서도 풀업 때문에 "카드 있음" 으로 읽혀 동작이 같다.

극성은 소켓마다 다르므로 예제 주석에 **재는 방법**을 적어 뒀다: INPUT_PULLUP 으로
읽고 INPUT_PULLDOWN 으로 다시 읽어, 두 값이 다르면 플로팅(미배선), 같으면 그 레벨이
카드 삽입 시 스위치가 구동하는 값이다.

#### 외부 라이브러리 현황

| | |
|---|---|
| `SD` (Arduino 공식) | ✅ 컴파일·실기 동작. 단 **8.3 단축 이름만** 나온다 |
| `SdFat` 2.3.0 | ✅ 컴파일·실기 동작. **긴 이름**이 나온다 (`System Volume Information`). 약 6 KB 더 든다 |
| `Seeed_Arduino_LSM6DS3` | 컴파일 O / 동작 X — 보드 매크로 문제 (위 §2 참조) |
| `Adafruit_BusIO` | ❌ `digitalPinToPort` 등 AVR 식 매크로 없음 |


## 2.13 `attachInterrupt` (GPIOTE) — ✅ (2026-09-12)

`cores/nrf54l/wiring_interrupt.{h,c}`. 실기 기록은 `docs/HIL/M2-interrupt.md`.

**⚠ P2 핀에는 인터럽트를 걸 수 없다.** GPIOTE 는 둘뿐이고 각자 자기 도메인 포트만
본다 — `GPIOTE20` = P1(채널 8), `GPIOTE30` = P0(채널 4), 그리고 **P2 를 담당하는
GPIOTE 가 아예 없다.** Pin Planner 의 SoC 정의로 확인했다
(`docs/PERIPHERAL-PINMAP.md` §4). 도메인 규칙의 예외가 아니라 **하드웨어가 없는 것**이다.

그래서 `attachInterruptOk()` 를 함께 제공한다. Arduino 표준 `attachInterrupt()` 는
반환값이 없어서 실패를 알릴 방법이 없고, P2 를 주면 조용히 아무 일도 일어나지 않는다.
호환을 위해 표준 판도 남겼다 (R12).

```c
bool attachInterruptOk(uint32_t pin, voidFuncPtr cb, uint32_t mode);  /* 실패를 알려준다 */
void attachInterrupt  (uint32_t pin, voidFuncPtr cb, uint32_t mode);  /* 상류 호환 */
void detachInterrupt  (uint32_t pin);
```

모드는 `RISING` / `FALLING` / `CHANGE` / `LOW_LEVEL`.

#### 구현에서 걸린 것

**① 레벨 트리거에 GPIOTE 채널을 주면 안 된다.** nrfx 는 채널이 주어지면 엣지 트리거만
쓸 수 있다. `LOW_LEVEL` 은 채널이 아니라 SENSE 로 도는 물건이라 `p_in_channel = NULL`
로 넘긴다. 섞으면 설정이 실패한다.

**② 인스턴스마다 벡터가 둘이다** (`GPIOTE20_0` / `GPIOTE20_1`). §7 F10 ③ 의 다중
인스턴스 경우라 직접 이어야 하는데, **하나만 이으면 절반이 조용히 사라진다.** 넷 다 잇고
`nm` 으로 `T` 확인했다.

**③ 인스턴스별 `NRFX_GPIOTE20_ENABLED` 같은 매크로가 없다.** 다른 드라이버와 달리
인스턴스 표가 SoC 헤더에서 자동 생성돼 존재하는 GPIOTE 가 전부 들어간다.
`NRFX_GPIOTE_ENABLED` 하나만 켜면 된다 — 없는 매크로를 찾느라 헤매기 쉽다.

#### 코어에 두어도 크기가 늘지 않는다

`attachInterrupt` 는 스케치가 `#include` 없이 부르므로 코어에 있어야 하는데,
`core.a` 가 `--whole-archive` 로 링크되므로(§7 F13 ①) `Wire` 때처럼 모든 스케치가
무거워질 위험이 있었다. **실측 결과 blink 가 25,264 B 로 전후 동일하다.**

`Wire` 와 갈린 이유는 **전역 객체의 유무**다. 전역 인스턴스의 생성자는 `.init_array` 에
들어가고 링커 스크립트가 그 섹션을 `KEEP` 하므로 GC 의 루트가 된다. GPIOTE 쪽은
함수뿐이라 안 부르면 통째로 사라진다.

> **일반화**: 코어에 **전역 객체**를 두면 모든 스케치가 비용을 치르고,
> **함수만** 두면 부르는 스케치만 치른다. 다음에 코어냐 라이브러리냐를 고를 때 이 기준을 쓴다.

---

## 2.14 `PinMap` 라이브러리 — ✅ (2026-09-12)

**코드가 없는 라이브러리다. 예제 주석이 본체다.**

nRF54L 은 페리페럴이 전원 도메인에 묶여 있어 아무 핀에나 붙지 않는데, 지금까지
그 사실을 빌드 에러로만 알 수 있었다. 아두이노 IDE 의 `boards.txt` 메뉴로 핀
플래너 흉내를 낼 수 있는지 먼저 검토했는데 **불가능했다** — 플랫폼 명세상 메뉴는
평면이고(중첩 없음), 메뉴끼리 의존 관계를 표현할 수 없고, 잘못된 조합을
비활성화할 수단이 없다. 핀당 메뉴를 두면 드롭다운이 31개 생기고, 그러고도
같은 신호를 두 핀에 배정하는 것을 못 막는다.

→ **표를 예제 주석으로 넣는 쪽으로 갔다.** IDE 안에서 오프라인으로 보이고,
링크가 썩지 않으며, 코어 버전과 표가 같이 굳는다.

### 예제

| | |
|---|---|
| 칩 | `pinmap_nRF54L05` / `L10` / `L15` / `LM20A` |
| 보드 | `pinmap_NU54-DK` / `pinmap_NU54V-DK` / `pinmap_XIAO_nRF54L15` |

보드 예제가 더 쓸모 있다 — **실제로 쓸 수 있는 것은 헤더에 나온 핀뿐**이고,
거기에 variant 가 이미 점유한 것까지 같이 보여 준다.

### 생성한다, 쓰지 않는다

`extras/gen_pinmap.py` 가 Pin Planner JSON(`socPeripherals[].signals[].allowedGpio`)
에서 굽는다. 제약이 156 항목이라 손으로 옮기면 반드시 틀린다.
보드 표의 헤더 핀은 `docs/boards/<보드>.md` 에서 **읽는다** — 핀 배정의 정본이
그 문서라고 CLAUDE.md §4.1 이 정했으므로 여기서 다시 적지 않는다.
문서 형식이 둘이라 파서도 둘이다 (NU54-DK 는 코드블록, XIAO 는 마크다운 표).

### 읽기 쉽게 만든 방법

처음 출력은 **못 읽을 물건**이었다. P1 핀마다 똑같은 500자가 반복됐다.
`allowedGpio` 가 `P1*` 와일드카드라 P1 의 모든 핀이 같은 목록을 받기 때문이다.

→ **포트 공통분을 위로 빼고, 표에는 그 핀만의 것을 남겼다.** 그랬더니 구조가
그대로 드러난다 — **P0·P1 은 포트 안에서 전부 동등하고, 구조가 있는 것은 P2 뿐이다.**

### 이 표가 곧바로 잡아낸 것

1. **`LED_BUILTIN`(NU54-DK 의 `PIN_LED1`)이 P2.09 다.** PWM 도 GPIOTE 도 P2 를
   담당하지 않으므로 **`analogWrite(LED_BUILTIN)` 과 `attachInterrupt` 가 그
   핀에서 안 된다.** PWM 예제는 LED2(P1.10) 로 짜야 한다
2. **XIAO 의 `Serial1`(UARTE21, P2.08/P2.07)이 합법이었다.** 보드 문서에
   "도메인 규칙과 어긋난다" 고 적어 둔 미해결 항목이었는데, 덜 적힌 쪽이 규칙이었다
3. **P2.03 은 `sQSPI.D2` 전용**이라 SPIM 도 UARTE 도 닿지 않는다
4. `docs/PERIPHERAL-PINMAP.md` §3 이 `attachInterrupt` 를 "P1·P2" 로 적고 있었다 — 틀렸다

### 한계

`architectures=nrf54l` 이지만 **번들 라이브러리라 Library Manager 에는 뜨지 않는다.**
`library.properties` 의 `url=` 도 사용자에게 보이지 않는다. 아두이노 IDE 가
외부 페이지를 여는 경로는 Board Manager 항목의 "More info"(`help.online`) 하나뿐이다.

## 3. 아직 검증 못 한 가정

| 가정 | 언제 검증되나 |
|---|---|
| ~~BASEPRI/PRIMASK 분리가 BLE 라디오 타이밍을 지키는지 (§7 F9)~~ | ✅ **풀렸다.** advertising 과 연결을 유지한 채 tickless idle 에서 틱 vs SYSCOUNTER 0.0 ppm |
| 링크 3개 이상 동시 연결 | 호스트가 2대뿐이라 못 해 봤다. 슬롯 관리는 개수와 무관하므로 2개에서 검증된 경로와 같다 |
| ~~BLE 실효 처리량~~ | ✅ **맥은 쟀다** — 양방향 26~28 KB/s, 병목은 호스트가 주는 연결 간격이었다 (§2.6). **iOS 는 아직이다.** 아이폰이 더 짧은 간격을 주면 수치가 올라간다 |
| `USE_LFRC` 경로 (크리스털 없는 보드) | 해당 보드가 생길 때 |
| probe-rs 가 아닌 J-Link 업로드 | 메뉴에 없음. 필요해지면 추가 |
| 10분 이상 장시간(수 시간) 안정성 | 5분까지만 확인 |

---

## 4. 이 프로젝트에서 두 번 이상 물린 것

새 세션에서 시간을 아끼려면 이것만이라도 보고 시작해라.

1. **CLAUDE.md 의 함정 목록(§7 F1~F13)이 이 프로젝트의 핵심 자산이다.**
   F1(SVC), F9(WFI/BASEPRI), F10(nrfx 4.x), F13(Arduino 빌드)은 전부
   "문서에 적혀 있던 내용이 틀려서" 오래 걸린 항목이다. 의심되면 실측해라.
2. **Arduino 는 `cores/` 를 `core.a` 로 묶는다.** weak 심볼 오버라이드가
   아카이브 경계를 넘지 못해 조용히 실패한다 → `-Wl,--whole-archive` 필수 (§7 F13).
   링크 후 `nm | grep <심볼>` 로 `T` 인지 확인하는 습관을 들여라.
3. **zsh 는 변수를 단어 분리하지 않는다.** `$ARGS` 로 넘기면 통째로 한 인자가 된다.
   반드시 배열 `VAR=(a b c)` + `"${VAR[@]}"`.
4. **probe-rs 는 detach 하면 코어를 재개한다.** halt 상태를 유지하며 여러 번
   읽는 디버깅은 안 된다. 대신 RAM 에 계측 변수를 심고 `probe-rs read` 로 읽는다.
   `g_fault`(magic `0xFA0175ED`) / `g_assert_file` / `g_assert_line` 이 이미 있다.
   심볼 주소는 `arm-none-eabi-nm <elf>` 로 뽑는다.
5. **디버거 attach 자체가 증상을 지울 수 있다.** tickless 버그가 그랬다.
   "SWD 로 보면 정상"이 곧 "문제 없음"은 아니다 (docs/HIL/M1-tickless.md).
6. **LFXO 는 소스만 고르면 끝이 아니다. 로드 커패시터도 보드마다 맞춰야 한다.**
   외부 캡이 없는 보드에 외부 캡 설정을 쓰면 발진이 빨라진다 (XIAO 실측 +805 ppm).
   F12 와 증상이 똑같다 — 타깃 안에서는 완벽히 정상으로 보이고 **호스트 시계와
   비교해야만 드러난다.** 값은 벤더 보드 정의(Zephyr DTS)에서 가져오는 게 빠르다.
7. **호스트(맥/폰)가 GATT 를 캐시한다. 코어 버그로 오진하기 딱 좋다.**
   보드의 static 주소는 고정이라 펌웨어를 바꿔도 그대로고, 호스트는 그 주소로
   **옛 속성 테이블을 계속 내준다.** 증상이 고약하다 — 광고도 보이고 연결도 되는데
   서비스가 덜 보이고 characteristic 이 0개이며, **PHY 1M / MTU 23 으로 붙는다.**
   하루에 두 번 걸렸다(`bleuart` 확인, 처리량 측정). 해법은 호스트에서 그 기기를
   삭제하거나 `Bluefruit.setAddr()` 로 주소를 흔드는 것이다.
8. **nRF54L05 는 L15 다이의 비닝이다.** 사양 밖 메모리가 물리적으로 있어서
   잘못된 링커 스크립트로도 동작해 버린다. 칩은 FICR 로 확인해라
   (`INFO.PART` @ `0x00FFC31C`).

---

## 5. 다른 PC에서 이어서 작업하기

**클론만으로는 안 된다. 두 가지가 더 필요하다.**

### 1) 위치 — sketchbook 의 `hardware/` 밑에 **심링크**

저장소를 옮기지 마라. 심링크만 걸면 git 작업은 원래 위치에서 그대로 한다.

```sh
mkdir -p ~/Documents/Arduino/hardware
ln -sfn <저장소 경로> ~/Documents/Arduino/hardware/baram-nrf54
```
(sketchbook 경로는 `arduino-cli config get directories.user` 로 확인)

⚠ **링크 이름은 반드시 `baram-nrf54` 여야 한다.** 이 방식에서는 FQBN 의 packager 가
디렉토리 이름으로 정해지는데, Board Manager 로 설치하면 인덱스의
`packages[0].name` 으로 정해진다. 둘을 맞춰 놓지 않으면 개발 중 쓰던 FQBN 이
릴리스 설치본에서 안 먹는다.

`arduino-cli board listall | grep NU54` 로 확인.

**개발은 이 방식으로만 한다.** Board Manager 설치 경로는 고칠 때마다 아카이브 →
업로드 → 인덱스 갱신 → 재설치를 돌아야 해서 개발 루프로 못 쓴다.
그쪽은 M5 DoD("깨끗한 환경에서 Board Manager URL 로 설치 → blink 업로드")를
검증할 때만 쓴다.

### 2) `nrf54l/platform.local.txt` — **gitignore 되므로 직접 만들어야 한다**

```sh
cp nrf54l/platform.local.txt.example nrf54l/platform.local.txt
# 그 안의 toolchain.path 를 자기 PC 경로로 고친다
```

없으면 이렇게 된다 (실제로 재현해 본 결과):

- **업로드**: 확실히 실패한다
  `cannot execute upload tool: fork/exec {runtime.tools.probe-rs-0.32.0.path}/bin/probe-rs`
- **컴파일**: ⚠ **조용히 잘못될 수 있다.** 다른 Arduino 패키지(STM32 등)가
  설치해 둔 xpack GCC 가 잡혀서 빌드는 성공하는데 컴파일러 버전이 다르다.
  (실측: 같은 스케치가 38004 B → 38504 B 로 나왔다)

### 3) 준비물

| | |
|---|---|
| Arm GNU Toolchain | **xPack 14.2.1-1.1**. 릴리스가 이 버전에 고정돼 있다 (`platform.txt` 의 `runtime.tools.xpack-arm-none-eabi-gcc-14.2.1-1.1`). 다른 것으로도 빌드는 되지만 **크기·측정값이 달라져 기존 HIL 기록과 비교할 수 없다.** ⚠ **버전 번호가 같아도 배포판이 다르면 다르다** — 실측: `rtos_scheduler` / `nu54dk` 가 Arm 공식 14.2.Rel1 로는 23,752 B / 3,432 B, xPack 14.2.1-1.1 로는 **24,088 B / 3,576 B** 였다 (2026-09-07) |
| arduino-cli | 1.0.3 / 1.2.2 에서 확인 |
| probe-rs | `0.32.0`. **저장소에 동봉하지 않는다** (2026-09-07 에 뺐다 — 38 MB 짜리 macOS 바이너리 하나가 저장소 최대 객체였고, 릴리스 아카이브에서는 이미 제외돼 있어 사용자에게는 쓰이지 않았다). 릴리스 설치본은 Board Manager 가 받아 오고, 개발용은 릴리스 `probe-rs-0.32.0` 의 자산(전 OS)이나 업스트림에서 받아 `platform.local.txt` 의 `probers.path` 로 가리킨다 |
| 하드웨어 | NU54-DK 계열은 외부 CMSIS-DAP 프로브 필요. **XIAO 는 온보드라 USB-C 하나면 된다** |

xPack GCC 는 이렇게 받는다 (Board Manager 가 쓰는 것과 같은 아카이브다):

```sh
curl -L -o /tmp/gcc.tar.gz \
  https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/download/v14.2.1-1.1/xpack-arm-none-eabi-gcc-14.2.1-1.1-darwin-arm64.tar.gz
mkdir -p ~/opt && tar xzf /tmp/gcc.tar.gz -C ~/opt
```

### 4) 예제 — 이제 저장소 안에 있다

`nrf54l/libraries/Bluefruit54Lib/examples/` 에 넣었다. IDE 의
**파일 → 예제 → Bluefruit54Lib** 에서 열린다. 전부 3보드 컴파일 확인됐다.

| 예제 | 내용 |
|---|---|
| `Peripheral/bleuart` | **Adafruit 원본 이식.** include 2줄만 지웠다 (원본 고지 유지) |
| `Peripheral/custom_service` | 128비트 커스텀 GATT 서비스 (읽기/알림/쓰기) |
| `Hardware/rtos_scheduler` | `Scheduler.startLoop()` 두 번째 태스크 + millis/micros |
| `Hardware/board_test` | 새 보드 붙였을 때 첫 확인 (LED 극성 / 버튼 / Serial) |

**예제 주석은 영어로 쓴다.** 사용자 대상이라 README 와 같은 기준이다.

### 5) 저장소에 없는 것

시험 스케치는 저장소 밖에 있다. 새 PC에서는 직접 만들어야 한다:
`~/Documents/Arduino/nu54dk_blink/nu54dk_blink.ino`
(LED1 1초 점멸 + `Scheduler.startLoop` 로 LED2 500ms 점멸
 + `millis` / `micros` / 버튼 상태를 1초마다 `Serial` 출력)

### 6) 동작 확인 순서

```sh
arduino-cli board listall | grep NU54          # 보드 2종이 보이는지
arduino-cli compile --fqbn baram-nrf54:nrf54l:nu54dk  <스케치>
arduino-cli upload  --fqbn baram-nrf54:nrf54l:nu54dk  <스케치>
```
시리얼 `/dev/cu.usbserial-*` (CP2102N) 115200 에서
`millis=... micros=... btn1=1` 이 2초 간격으로 나오고,
**Δmillis = 2000 이면서 Δmicros = 2000000** 이면 정상이다.
(둘 중 하나만 봐서는 tickless 드리프트를 못 잡는다 — §7 F9b)

시험 스케치: `~/Documents/Arduino/nu54dk_blink/nu54dk_blink.ino`
(LED1 1초 점멸 + `Scheduler.startLoop` 로 LED2 500ms + millis/micros/버튼 출력)

시리얼: `/dev/cu.usbserial-*` (CP2102N), 115200.
