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
| ~~**B1**~~ ✅ | `BLEUuid` / `BLEService` / `BLECharacteristic` + 최소 `Bluefruit` 싱글턴 | **완료.** 탐색·읽기·알림·쓰기 전부 실증 |
| ~~**B2**~~ ✅ | `BLEUart` (NUS) | **완료.** 18바이트 에코 왕복 일치 |
| ~~**B3**~~ ✅ | MTU 협상(247), `BLEConnection`, `BLEDis`, `BLEBas`, `autoConnLed` 등 | **완료.** Adafruit `bleuart` 예제가 API 호출 그대로 동작 |
| ~~**B4**~~ ✅ | `BLEDfu` 스텁, 파일시스템 안내 헤더, `bluefruit.h` 가 서비스 포함 | **완료. Adafruit 원본 `bleuart.ino` 가 include 2줄 삭제만으로 동작 = M3 DoD** |
| **B5** (진행 중) | **다중 연결(최대 5)** — 설계 확정, 구현 전 | `bleuart_multi` + 호스트 2대 |
| B6 (남음) | `BLESecurity` / 본딩, `getPeerName()`(GATT 클라이언트), HID, Beacon, central | |

지금 위치: **M3 DoD 달성.** Adafruit 원본 `bleuart.ino` 가
`#include <Adafruit_LittleFS.h>` / `<InternalFileSystem.h>` **두 줄 삭제만으로**
컴파일·동작한다 (`~/Documents/Arduino/bleuart_orig/`).
서비스 4종·DIS·배터리·UART·MTU 247 전부 확인.
DoD 문구를 그렇게 바꾼 근거(예제 71개 전수 조사)는 CLAUDE.md §8.1.

**아직 없는 것:**

- **본딩 / 페어링** (`BLESecurity`). pairing 계열 예제는 아직 안 된다.
  키는 `peer_manager` 4 KB 파티션에 고정 레코드로 넣을 계획이다 (§8.1)
- **`getPeerName()`** — GATT 클라이언트 경로가 없다. 빈 문자열 + false
  (`docs/HIL/M3-softdevice.md` §3.8)
- **central 역할** — peripheral 전용 구성이다
- **실제 DFU** — `BLEDfu` 는 서비스만 등록하고 명확히 거절한다. M4 에서 연결 `nrf54l/libraries/Bluefruit54Lib/` 에
`BLEUuid`/`BLEService`/`BLECharacteristic`/`bluefruit` 가 올라가 있고,
커스텀 GATT 서비스로 읽기·알림·쓰기가 실기에서 확인됐다.
시험 스케치는 `~/Documents/Arduino/nrf54_ble_gatt/` (GATT), `~/Documents/Arduino/nrf54_bleuart/` (NUS).

⚠ MTU 를 키울 때 걸린 것 네 가지는 `docs/HIL/M3-softdevice.md` §3.7 에 있다.
특히 `ble_gap_cfg_role_count_t.adv_set_count` 는 구조체 첫 필드라
`memset` 뒤에 빠뜨리기 쉽고, 증상이 "BLE 를 못 켠다" 로만 보인다.

⚠ B1 에서 잡은 것: **`DATA_LENGTH_UPDATE_REQUEST` 에 답하지 않으면 연결은
유지되는데 ATT 가 전혀 흐르지 않는다.** 자세한 건 `docs/HIL/M3-softdevice.md` §3.5.

**파일시스템은 B4 전까지 필요 없다.** 예제의 `#include <Adafruit_LittleFS.h>` 는
본딩 저장 때문이고, RRAM 에는 erase 가 없어 그대로 못 올린다.
배경과 권장 경로는 CLAUDE.md §8.1.

### B5 — 다중 연결 (2026-09-06 시점: RAM 확보 완료, 라이브러리 구현 전)

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
