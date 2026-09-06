# 진행 상황 / 다음 세션 인수인계

최종 갱신: 2026-09-06 · 커밋 `b8cc273`

프로젝트 지침과 설계 결정은 [CLAUDE.md](../CLAUDE.md) 가 정본이다.
이 문서는 **"지금 어디까지 됐고 다음에 뭘 하면 되는지"** 만 짧게 적는다.

---

## 1. 지금 동작하는 것 (M1 거의 완료)

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

---

## 2. 바로 다음에 할 일

### (a) 전류 측정 — M1 을 닫으려면 이게 남았다

tickless 를 켠 목적이 전력인데 아직 재지 못했다. **SWD 프로브를 물리적으로
분리하고** 재야 한다 (§7 F8 — 붙어 있으면 수치가 안 나온다).

측정 4종 (§4.6):
1. `delay(1000)` 루프 + `Serial` 켠 상태
2. 같은 조건에서 `Serial.end()` 후 (UARTE 가 바닥 전류를 올리는지)
3. `systemOff()` 후 — **`systemOff()` 는 아직 구현 안 됨**
4. (M3) advertising 중

기준선은 Nordic `ble_pwr_profiling` 샘플을 같은 보드에 구워서 잡는다.
이 비교 없이는 "이 정도면 괜찮은가"를 판단할 근거가 없다.

같이 판단할 것: `port_grtc.c` 의 `nrfx_grtc_active_request_set(true)` 를 빼도
되는지. `MODE.AUTOEN` 이 이미 0 이라 중복일 가능성이 높고, 뺐을 때 전력이
내려가면 빼면 된다. 기능적으로는 없어도 틱이 정확하다 (확인됨).

### (b) 앱 레벨 저전력 API (§4.5)

`waitForEvent()`, `systemOff(pin, wake_logic)`, `readResetReason()`.
`readResetReason()` 은 `NRF_RESET` 이다 (`NRF_POWER->RESETREAS` 아님, `NRF_RESETINFO` 도 아님).

### (c) 릴리스 파이프라인 — ✅ **0.1.0 배포 완료 (2026-09-06)**

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

### B 단계 — Bluefruit API 계층 (진행 중)

**목표는 M3 DoD**: Adafruit `Bluefruit52Lib/examples/Peripheral/bleuart` 원본이
**무수정으로** 컴파일·동작하는 것.

규모를 먼저 알아 둘 것: Bluefruit52Lib 본체만 **약 250 KB / 30여 파일**이고
`services/`(BLEUart·BLEDis·BLEBas·BLEDfu)와 파일시스템 2종이 더 붙는다.
한 번에 끝나지 않으므로 아래처럼 쪼갠다. **각 단계마다 bleak 으로 실증한다.**

| 단계 | 내용 | 검증 |
|---|---|---|
| **B1** | `BLEUuid` / `BLEService` / `BLECharacteristic` / `BLEGatt` + 최소 `Bluefruit` 싱글턴 | 커스텀 GATT 서비스를 호스트에서 탐색·읽기 |
| **B2** | `BLEUart` (NUS) | 양방향 송수신 |
| **B3** | `BLEConnection`, `BLEPeriph` 콜백, `BLEAdvertising` 전체, `BLEDis` / `BLEBas` | 예제 다수가 컴파일되기 시작 |
| **B4** | `BLESecurity` / 본딩, `BLEDfu` 스텁, 파일시스템 호환 헤더 | **`bleuart.ino` 무수정 = DoD** |

지금 위치: **B1 착수 전.** A 단계까지 끝났고, GATT 서비스가 하나도 없어서
연결은 되지만 호스트가 서비스 탐색에 실패하고 곧 끊는다.

**파일시스템은 B4 전까지 필요 없다.** 예제의 `#include <Adafruit_LittleFS.h>` 는
본딩 저장 때문이고, RRAM 에는 erase 가 없어 그대로 못 올린다.
배경과 권장 경로는 CLAUDE.md §8.1.

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

## 3. 아직 검증 못 한 가정

| 가정 | 언제 검증되나 |
|---|---|
| BASEPRI/PRIMASK 분리가 BLE 라디오 타이밍을 지키는지 (§7 F9) | M3. advertising 유지 + tickless 동시 동작이 최우선 확인 항목 |
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
7. **nRF54L05 는 L15 다이의 비닝이다.** 사양 밖 메모리가 물리적으로 있어서
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
| Arm GNU Toolchain | **xPack 14.2.1-1.1**. 릴리스가 이 버전에 고정돼 있다 (`platform.txt` 의 `runtime.tools.xpack-arm-none-eabi-gcc-14.2.1-1.1`). 다른 버전으로도 빌드는 되지만 **크기·측정값이 달라져 기존 HIL 기록과 비교할 수 없다** |
| arduino-cli | 1.0.3 / 1.2.2 에서 확인 |
| probe-rs | `0.32.0`. 개발용으로는 저장소 동봉본(`nrf54l/tools/probe-rs/macosx/bin`, **macOS 만**)을 쓰고, 릴리스 설치본은 Board Manager 가 툴로 내려받는다 |
| 하드웨어 | NU54-DK 계열은 외부 CMSIS-DAP 프로브 필요. **XIAO 는 온보드라 USB-C 하나면 된다** |

xPack GCC 는 이렇게 받는다 (Board Manager 가 쓰는 것과 같은 아카이브다):

```sh
curl -L -o /tmp/gcc.tar.gz \
  https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/download/v14.2.1-1.1/xpack-arm-none-eabi-gcc-14.2.1-1.1-darwin-arm64.tar.gz
mkdir -p ~/opt && tar xzf /tmp/gcc.tar.gz -C ~/opt
```

### 4) 저장소에 없는 것

시험 스케치는 저장소 밖에 있다. 새 PC에서는 직접 만들어야 한다:
`~/Documents/Arduino/nu54dk_blink/nu54dk_blink.ino`
(LED1 1초 점멸 + `Scheduler.startLoop` 로 LED2 500ms 점멸
 + `millis` / `micros` / 버튼 상태를 1초마다 `Serial` 출력)

### 5) 동작 확인 순서

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
